"""Captured hardware comparison, without opening an audio device."""
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
def invoke(*args, ok=True):
    r = subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True, timeout=45)
    assert (r.returncode == 0) == ok, r.stdout + r.stderr
    return r.stdout
with tempfile.TemporaryDirectory(prefix='moth-hardware-comparison-') as temporary:
    root = Path(temporary)
    scan = root / 'scan.json'
    scan.write_text(json.dumps({'action': 'scan', 'plugin': str(plugin)}))
    invoke('--worker', scan)
    desc = json.loads((root / 'scan-result.json').read_text())['types'][0]['xml']
    rng = random.Random(22)
    samples = [rng.randint(-400, 400) for _ in range(48000)]
    delay = 137
    for name, data in [('input', samples), ('capture', [0]*delay + samples[:-delay])]:
        with wave.open(str(root / f'{name}.wav'), 'wb') as f:
            f.setparams((1, 2, 48000, 0, 'NONE', 'not compressed'))
            f.writeframes(b''.join(struct.pack('<h', x) for x in data))
    routing = {'device': 'synthetic fixture', 'driver': 'fixture', 'inputChannel': 2, 'outputChannel': 1}
    bypass = dict(routing, status='passed', sampleRate=48000,
                  analysis={'passed': True, 'latencySamples': delay})
    metadata = dict(routing, blockSize=256, capturedSamples=len(samples), deviceError=False,
                    interfaceAndReampUnchangedConfirmed=True, requestedPedalStateConfirmed=True,
                    operatorPhysicalSettings='Synthetic unity gain plus known delay')
    job = {'project': str(root / 'project'), 'input': str(root / 'input.wav'),
           'capture': str(root / 'capture.wav'), 'candidate': {'plugin': str(plugin), 'descriptionXml': desc, 'state': ''},
           'bypassCheck': bypass, 'bypassFolder': str(root), 'metadata': metadata}
    request = root / 'compare.json'
    request.write_text(json.dumps(job))
    invoke('--compare-hardware', request)
    session = next((root / 'project/sessions').iterdir())
    manifest = json.loads((session / 'manifest.json').read_text())
    summary = json.loads((session / 'analysis_summary.json').read_text())
    assert manifest['reference']['kind'] == 'hardware'
    assert manifest['hardwareLatencyRemovedSamples'] == delay
    assert summary['residualRmsDbFS'] <= -140
    assert abs(summary['gainDifferenceDb']) < .001
    assert summary['absoluteLevelCalibration'] is False and summary['qualityScore'] is None
    assert (session / 'reference-raw.wav').exists()
    preserved = (session / 'manifest.json').read_bytes()
    for field, value in [('inputChannel', 1), ('capturedSamples', 20), ('deviceError', True),
                         ('interfaceAndReampUnchangedConfirmed', False), ('operatorPhysicalSettings', '')]:
        old = metadata[field]; metadata[field] = value
        request.write_text(json.dumps(job)); invoke('--compare-hardware', request, ok=False)
        metadata[field] = old
    bypass['status'] = 'failed'
    request.write_text(json.dumps(job)); invoke('--compare-hardware', request, ok=False)
    assert (session / 'manifest.json').read_bytes() == preserved
print('PASS hardware capture: known-delay null, raw artifacts, settings provenance, route/calibration guards')
