#!/usr/bin/env python3
"""One-time migration of flat Lab storage. Close the app first. Defaults to dry-run.
Keeps a complete backup beside the project and verifies every original WAV hash.
Immutable handoff snapshots are never rewritten. Unknown captures remain unassigned.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def organize(root, app, apply=False):
    root = root.resolve()
    journal = root / 'storage-migration.json'
    if journal.exists():
        raise RuntimeError('Migration already recorded; do not repeat it')
    workflows = {p.stem: (p, json.loads(p.read_text())) for p in (root/'workflows').glob('*.json')}
    layout = json.loads(subprocess.check_output([str(app.resolve()), "--storage-layout", str(root)], text=True))
    mapping, runs = {}, {}
    for wid, (path, w) in workflows.items():
        if w.get('phase') in ('capturing', 'processing', 'running'):
            raise RuntimeError(f'Unfinished work: {path}')
        relative = layout[wid]
        w.update(storageRun=relative, workflowId=wid)
        runs[wid] = root/relative
    # Explicit journal references and embedded workflow IDs establish ownership.
    owners = {}
    for wid, (_, w) in workflows.items():
        for key in ('result1', 'result2', 'result3', 'captureFolder', 'legacyBypass'):
            if w.get(key): owners.setdefault(w[key], wid)
        for value in w.get('scenarioResults', {}).values():
            if isinstance(value, str): owners.setdefault(value, wid)
    for sub, filename, destination in [('sessions','manifest.json','results'), ('setup-checks','setup-check.json','captures')]:
        for folder in sorted((root/sub).iterdir()):
            if not folder.is_dir(): continue
            data = json.loads((folder/filename).read_text())
            hw = data.get('hardware', data)
            wid = hw.get('workflowId') or owners.get(str(folder))
            if not wid:
                # Match preserved raw capture to a session by exact acquisition timestamp.
                for manifest in (root/'sessions').glob('*/manifest.json'):
                    m = json.loads(manifest.read_text())
                    if m.get('hardware', {}).get('createdUtc') == hw.get('createdUtc'):
                        wid = m.get('hardware', {}).get('workflowId') or owners.get(str(manifest.parent))
                        if wid: break
            base = runs.get(wid, root/'archive/unassigned-captures')
            mapping[str(folder)] = str(base/destination/folder.name)
    # Existing export is an immutable historical snapshot: move it as a whole.
    for folder in (root/'exports').glob('*'):
        if not folder.is_dir(): continue
        snapshot = json.loads((folder/'workflow.json').read_text())
        wid = next((i for i, (_, w) in workflows.items() if w.get('scenarioRevision') == snapshot.get('scenarioRevision')), None)
        base = runs.get(wid, root/'archive/unassigned-exports')
        mapping[str(folder)] = str(base/'exports'/folder.name)
    print(json.dumps({'folders':len(mapping),'runs':{i:str(p.relative_to(root)) for i,p in runs.items()}},ensure_ascii=False,indent=2))
    if not apply: return
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    backup = root.parent/'storage-backups'/stamp/root.name
    backup.parent.mkdir(parents=True, exist_ok=False)
    shutil.copytree(root, backup)
    original_wavs = {str(p.relative_to(root)):digest(p) for p in root.rglob('*.wav')}
    def remap(text):
        for old,new in sorted(mapping.items(), key=lambda kv:-len(kv[0])):
            text = text.replace(old, new)
        return text
    try:
        for old,new in mapping.items():
            target=Path(new)
            if target.exists(): raise RuntimeError(f'Destination exists: {target}')
            target.parent.mkdir(parents=True, exist_ok=True)
            Path(old).rename(target)
        # Export checksums refer to their original immutable contents.
        for p in root.rglob('*'):
            if p.is_file() and p.suffix in ('.json','.md','.txt') and 'exports' not in p.relative_to(root).parts:
                before = p.read_text()
                after = remap(before)
                if before != after: p.write_text(after)
        for wid, (path, w) in workflows.items():
            old_stat=(backup/'workflows'/path.name).stat()
            data=json.loads(remap(json.dumps(w,ensure_ascii=False)))
            path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
            import os
            os.utime(path,(old_stat.st_atime,old_stat.st_mtime)) # Preserve current workflow selection.
            run=runs[wid];run.mkdir(parents=True, exist_ok=True)
            (run/'run.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
            rows=['# '+(w.get('name') or 'Korai kézi mérési sorozat'),'',
                  'Profil: '+w.get('scenario',{}).get('title','Korai kézi mérések'),'',
                  '- `results/`: kiértékelt mérések, WAV-ok, beállítások és riportok.',
                  '- `captures/`: nyers felvételek és bypass-ellenőrzések.',
                  '- `exports/`: változatlan, önálló Codex-átadócsomagok.',
                  '- `run.json`: a sorozat állapota és teljes forgatókönyve.','', '## Eredmények','']
            for p in sorted((run/'results').glob('*/manifest.json')):
                m=json.loads(p.read_text()); h=m.get('hardware',{}); step=h.get('scenarioStep',{})
                excluded=(p.parent/'operator-review.json').exists()
                label=f'{step.get("id", "")} {step.get("title", p.parent.name)}'.strip()
                if excluded:label+=' — felülvizsgálati jelöléssel'
                from urllib.parse import quote
                rows.append(f'- [{label}]({quote(str(p.parent.relative_to(run)))}/report.md)')
            (run/'README.md').write_text('\n'.join(rows)+'\n')
        for relative, expected in original_wavs.items():
            current=Path(remap(str(root/relative)))
            if not current.is_file() or digest(current)!=expected:
                raise RuntimeError(f'WAV verification failed: {relative}')
        for sub in ('sessions','setup-checks','exports'):
            folder=root/sub
            if (folder/'.DS_Store').exists(): (folder/'.DS_Store').unlink()
            if folder.exists() and not any(folder.iterdir()): folder.rmdir()
        record={'schemaVersion':1,'backup':str(backup),'movedFolders':mapping,'verifiedWavs':len(original_wavs)}
        journal.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
        (root/'README.md').write_text('# Moth DSP Lab — mérések\n\n'
            '- `measurements/`: profil → mérési sorozat → results, captures, exports.\n'
            '- `profiles/`: újra felhasználható mérési tervek.\n'
            '- `workflows/`: az alkalmazás folytatáshoz használt naplói.\n'
            '- `archive/unassigned-captures/`: a profilok előtti, bizonyíthatóan nem hozzárendelhető ellenőrzések.\n'
            '- `storage-migration.json`: az eredeti és új helyek, biztonsági másolat helye.\n')
        print(f'OK: {len(original_wavs)} WAV hashes verified; backup: {backup}')
    except Exception:
        # Keep failed output for inspection and restore the entire pre-migration project.
        failed=root.with_name(root.name+'-migration-failed-'+stamp)
        root.rename(failed)
        shutil.copytree(backup,root)
        raise

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project',type=Path)
    parser.add_argument('--app',type=Path,required=True)
    parser.add_argument('--apply',action='store_true')
    args=parser.parse_args()
    organize(args.project,args.app,args.apply)
