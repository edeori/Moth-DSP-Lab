"""macOS AU state replay plus mixed AU/VST3 rendering; uses a built-in Apple AU by default."""
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import wave

exe, binary = map(Path, sys.argv[1:3])
vst3 = next((p for p in (binary, *binary.parents) if p.suffix == '.vst3'), binary)
# Stable Core Audio type/subtype/manufacturer identifier; no third-party installation required.
au = sys.argv[3] if len(sys.argv) > 3 else 'AudioUnit:Effects/aufx,hpas,appl'


def invoke(*args):
    result = subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True, timeout=45)
    assert result.returncode == 0, result.stderr + result.stdout
    return result.stdout


with tempfile.TemporaryDirectory(prefix='moth-au-integration-') as temp:
    root = Path(temp)
    settings = {'sampleRate': 48000, 'blockSize': 256, 'channels': 2, 'tailSeconds': .05}

    def scan(identifier, stem):
        path = root / f'{stem}.json'
        path.write_text(json.dumps({'action': 'scan', 'plugin': identifier}))
        invoke('--worker', path)
        result = json.loads(path.with_name(f'{stem}-result.json').read_text())
        assert 'error' not in result, result
        return result['types'][0]

    au_type = scan(au, 'scan-au')
    assert au_type['format'] == 'AudioUnit', au_type
    target = {'plugin': '' if au.startswith('AudioUnit:') else au,
              'descriptionXml': au_type['xml'], 'state': ''}
    configure = root / 'configure.json'
    config = {'action': 'configure', 'target': target, 'settings': settings}
    configure.write_text(json.dumps(config))
    invoke('--worker', configure)
    inspected = json.loads((root / 'configure-result.json').read_text())
    assert 'error' not in inspected, inspected
    if inspected['parameters']:
        config['parameters'] = {inspected['parameters'][0]['id']: .2}
        configure.write_text(json.dumps(config))
        invoke('--worker', configure)
        configured = json.loads((root / 'configure-result.json').read_text())
        assert 'error' not in configured, configured
        assert abs(configured['parameters'][0]['normalizedValue'] - .2) < .01
        target['state'] = configured['state']
    else:
        target['state'] = inspected['state']
    assert target['state'], 'AU must export its host state'

    wav = root / 'input.wav'
    with wave.open(str(wav), 'wb') as writer:
        writer.setparams((2, 2, 48000, 0, 'NONE', 'not compressed'))
        writer.writeframes(b''.join(struct.pack('<hh', value, value) for value in
            (int(3000 * (math.sin(2 * math.pi * 80 * i / 48000) + math.sin(2 * math.pi * 2000 * i / 48000)))
             for i in range(12037))))
    project = root / 'project'
    compare_file = root / 'compare.json'
    job = {'project': str(project), 'input': str(wav), 'settings': settings,
           'reference': target, 'candidate': target}

    def compare():
        before = set((project / 'sessions').iterdir()) if (project / 'sessions').exists() else set()
        compare_file.write_text(json.dumps(job))
        invoke('--compare', compare_file)
        return next(p for p in (project / 'sessions').iterdir() if p not in before and not p.name.startswith('.'))

    saved = compare()
    manifest = json.loads((saved / 'manifest.json').read_text())
    summary = json.loads((saved / 'analysis_summary.json').read_text())
    assert manifest['referenceRender']['metadata']['pluginFormat'] == 'AudioUnit'
    assert manifest['referenceRender']['metadata']['pluginIdentifier'].startswith('AudioUnit:')
    assert summary['residualRmsDbFS'] <= -140, summary['residualRmsDbFS']
    assert abs(summary['gainDifferenceDb']) < .001
    job.update(reference=manifest['reference'], candidate=manifest['candidate'])
    replay = compare()
    assert (saved / 'reference.wav').read_bytes() == (replay / 'reference.wav').read_bytes()
    # Reinspect the restored state through the same path as the generic parameter editor.
    config = {'action': 'configure', 'target': manifest['reference'], 'settings': settings}
    configure.write_text(json.dumps(config))
    invoke('--worker', configure)
    restored = json.loads((root / 'configure-result.json').read_text())
    assert 'error' not in restored, restored
    if inspected['parameters']:
        assert abs(restored['parameters'][0]['normalizedValue'] - .2) < .01
    vst_type = scan(str(vst3), 'scan-vst3')
    job['candidate'] = {'plugin': str(vst3), 'descriptionXml': vst_type['xml'], 'state': ''}
    mixed = compare()
    mixed_manifest = json.loads((mixed / 'manifest.json').read_text())
    assert mixed_manifest['referenceRender']['metadata']['pluginFormat'] == 'AudioUnit'
    assert mixed_manifest['candidateRender']['metadata']['pluginFormat'] == 'VST3'
    assert (mixed / 'residual.wav').exists()
print(f'PASS {au_type["name"]}: AU scan, parameters, state replay, isolated render, mixed AU/VST3 session')
