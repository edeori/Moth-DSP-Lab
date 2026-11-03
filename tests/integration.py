"""End-to-end VST3 scan, isolated render, state replay and artifact checks."""
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile
import wave

exe, binary = map(Path, sys.argv[1:])
plugin = next((p for p in (binary, *binary.parents) if p.suffix == '.vst3'), binary)


def wave_frames(path):
    data = path.read_bytes()
    cursor, align = 12, None
    while cursor + 8 <= len(data):
        kind, size = struct.unpack_from('<4sI', data, cursor)
        if kind == b'fmt ':
            align = struct.unpack_from('<H', data, cursor + 8 + 12)[0]
        if kind == b'data':
            return size // align
        cursor += 8 + size + size % 2
    raise AssertionError('No WAV data chunk')


def invoke(*args):
    result = subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True, timeout=60)
    if result.returncode:
        raise AssertionError(result.stderr + result.stdout)
    return result.stdout


with tempfile.TemporaryDirectory(prefix='moth-integration-') as temp:
    root = Path(temp)
    scan = root / 'scan.json'
    scan.write_text(json.dumps({'action': 'scan', 'plugin': str(plugin)}))
    invoke('--worker', scan)
    types = json.loads((root / 'scan-result.json').read_text())['types']
    rng = random.Random(12)
    wav = root / 'input.wav'
    with wave.open(str(wav), 'wb') as writer:
        writer.setparams((2, 2, 48000, 0, 'NONE', 'not compressed'))
        writer.writeframes(b''.join(struct.pack('<hh', x, -x) for x in (rng.randint(-10000, 10000) for _ in range(12037))))

    settings = {'sampleRate': 48000, 'blockSize': 256, 'channels': 2, 'tailSeconds': .05}
    default_target = {'plugin': str(plugin), 'descriptionXml': types[0]['xml'], 'state': ''}
    configure = root / 'configure.json'
    configure.write_text(json.dumps({'action': 'configure', 'target': default_target, 'settings': settings}))
    invoke('--worker', configure)
    parameters = json.loads((root / 'configure-result.json').read_text())['parameters']
    ids = {p['name']: p['id'] for p in parameters}

    def target(gain, delay, terminate=False):
        configure.write_text(json.dumps({'action': 'configure', 'target': default_target, 'settings': settings,
            'parameters': {ids['Gain']: gain / 2, ids['Delay']: delay / 2048,
                           ids['Terminate worker']: float(terminate)}}))
        invoke('--worker', configure)
        result = json.loads((root / 'configure-result.json').read_text())
        assert 'error' not in result, result
        return dict(default_target, state=result['state'])

    job = {'project': str(root / 'project'), 'input': str(wav),
           'settings': {'sampleRate': 48000, 'blockSize': 256, 'channels': 2, 'tailSeconds': .05},
           'reference': target(1, 0), 'candidate': target(.5, 137)}
    compare = root / 'compare.json'
    compare.write_text(json.dumps(job))
    invoke('--compare', compare)
    sessions = root / 'project' / 'sessions'
    saved = next(p for p in sessions.iterdir() if not p.name.startswith('.'))
    manifest = json.loads((saved / 'manifest.json').read_text())
    summary = json.loads((saved / 'analysis_summary.json').read_text())
    assert manifest['status'] == 'complete'
    assert manifest['stimulusSha256'] == hashlib.sha256(wav.read_bytes()).hexdigest()
    assert manifest['candidateRender']['metadata']['reportedLatencySamples'] == 137
    assert abs(summary['gainDifferenceDb'] + 6.0206) < .002, summary
    assert summary['candidateDelaySamples'] == 0, 'Reported delay must already be compensated'
    assert summary['alignmentReliable']
    assert wave_frames(saved / 'candidate.wav') == 12037 + 2400
    assert wave_frames(saved / 'candidate-raw.wav') == 12037 + 2400 + 137
    assert (saved / 'residual.wav').exists() and (saved / 'report.md').exists()
    # Rerender using the exact initial states saved by the host.
    job.update(reference=manifest['reference'], candidate=manifest['candidate'])
    compare.write_text(json.dumps(job))
    invoke('--compare', compare)
    replay = next(p for p in sessions.iterdir() if p != saved and not p.name.startswith('.'))
    assert (saved / 'reference.wav').read_bytes() == (replay / 'reference.wav').read_bytes()
    assert (saved / 'candidate.wav').read_bytes() == (replay / 'candidate.wav').read_bytes()
    # A different block size must preserve this fixture's output exactly.
    job['settings']['blockSize'] = 1024
    compare.write_text(json.dumps(job))
    invoke('--compare', compare)
    varied = next(p for p in sessions.iterdir() if p not in (saved, replay) and not p.name.startswith('.'))
    assert (saved / 'candidate.wav').read_bytes() == (varied / 'candidate.wav').read_bytes()
    # Abrupt worker termination must fail the measurement and preserve completed data.
    preserved = (saved / 'manifest.json').read_bytes()
    job['candidate'] = target(1, 0, terminate=True)
    compare.write_text(json.dumps(job))
    result = subprocess.run([str(exe), '--compare', str(compare)], capture_output=True, timeout=60)
    assert result.returncode != 0
    assert (saved / 'manifest.json').read_bytes() == preserved
    # Unsupported plugin fails without modifying any previously completed session.
    preserved = (saved / 'manifest.json').read_bytes()
    job['candidate']['plugin'] = str(root / 'missing.vst3')
    compare.write_text(json.dumps(job))
    result = subprocess.run([str(exe), '--compare', str(compare)], capture_output=True, timeout=60)
    assert result.returncode != 0
    assert (saved / 'manifest.json').read_bytes() == preserved
    assert any(json.loads((p / 'manifest.json').read_text())['status'] == 'failed'
               for p in sessions.iterdir() if p.name.startswith('.pending'))
print('PASS VST3 isolation, gain, latency, state replay, export, failure preservation')
