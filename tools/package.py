#!/usr/bin/env python3
"""Build source, developer, or validated mod-manager archives. No implicit release."""
import argparse
import hashlib
import json
import re
import struct
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def validate_runtime(stage, metadata):
    if not metadata.get('release_ready') or not all(metadata['required_features'].values()):
        raise ValueError('Playable release is disabled: required features are unfinished.')
    for name in metadata['required_files']:
        p = stage / name
        if not p.is_file() or p.stat().st_size == 0:
            raise ValueError(f'Missing runtime file: {name}')
        head = p.read_bytes()
        if p.suffix == '.dll':
            if len(head) < 64 or head[:2] != b'MZ':
                raise ValueError('DLL is not a PE binary')
            offset = struct.unpack_from('<I', head, 60)[0]
            if offset + 24 > len(head) or head[offset:offset+4] != b'PE\0\0':
                raise ValueError('Invalid PE header')
            machine = struct.unpack_from('<H', head, offset+4)[0]
            flags = struct.unpack_from('<H', head, offset+22)[0]
            if machine != 0x8664 or not flags & 0x2000:
                raise ValueError('Expected Windows x64 DLL')
        if p.suffix == '.pex' and head[:4] != bytes.fromhex('fa57c0de'):
            raise ValueError(f'Invalid compiled Papyrus file: {name}')
        if p.suffix == '.esp' and head[:4] != b'TES4':
            raise ValueError('Invalid plugin header')

def validate_mcm_test(stage):
    """Accept only a complete MCM catalogue test payload, never a partial ZIP."""
    required = [
        'SKSE/Plugins/JGJGsTransmogSystem.dll',
        'JGJGsTransmogSystem.esp',
        'Scripts/JGJGTransmogMCM.pex',
        'Scripts/JGJGTransmogNative.pex',
    ]
    entries = {}
    for name in required:
        p = stage/name
        if not p.is_file() or p.stat().st_size == 0:
            raise ValueError(f'Missing MCM test file: {name}')
        data = p.read_bytes()
        if name.endswith('.dll'):
            if len(data) < 64 or data[:2] != b'MZ':
                raise ValueError('MCM test DLL is not a PE binary')
            offset = struct.unpack_from('<I', data, 60)[0]
            if offset + 24 > len(data) or data[offset:offset+4] != b'PE\0\0' or struct.unpack_from('<H', data, offset+4)[0] != 0x8664:
                raise ValueError('MCM test DLL is not Windows x64')
        elif name.endswith('.pex') and data[:4] != bytes.fromhex('fa57c0de'):
            raise ValueError(f'Invalid compiled Papyrus file: {name}')
        elif name.endswith('.esp') and data[:4] != b'TES4':
            raise ValueError('Invalid MCM test ESP header')
        elif name.endswith('.esp') and not (struct.unpack_from('<I', data, 8)[0] & 0x200):
            raise ValueError('MCM test plugin must be an ESL-flagged ESP-FE')
        entries[name] = data
    return entries

def archive(path, entries):
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, content in sorted(entries.items()):
            if name.startswith('/') or '..' in Path(name).parts:
                raise ValueError('Unsafe archive path')
            info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            z.writestr(info, content)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', choices=['source', 'developer', 'mcm-test', 'runtime'], required=True)
    parser.add_argument('--stage', type=Path)
    parser.add_argument('--out', type=Path, default=ROOT/'dist')
    args = parser.parse_args()
    metadata = json.loads((ROOT/'release.json').read_text())
    version = metadata['version']
    if not re.fullmatch(r'[0-9A-Za-z.-]+', version):
        raise ValueError('Unsafe version string')
    args.out.mkdir(parents=True, exist_ok=True)
    entries = {}
    if args.mode == 'source':
        names = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
        for name in names:
            if name and (ROOT/name).is_file():
                entries['JGJGs-Transmog-System/'+name] = (ROOT/name).read_bytes()
        suffix = 'SOURCE'
    elif args.mode == 'developer':
        if not args.stage or not (args.stage/'JGJGsTransmogSystem.dll').is_file():
            raise ValueError('Compiled developer DLL is missing')
        entries['developer-only/JGJGsTransmogSystem.dll'] = (args.stage/'JGJGsTransmogSystem.dll').read_bytes()
        entries['READ-ME-FIRST.txt'] = b'DEVELOPMENT COMPONENT ONLY. Not an installable transmog mod. No MCM quest, compiled scripts, appearance hooks, save persistence or 3D preview. Do not install as a playable release. See README.md in the repository.\n'
        suffix = 'DEVELOPER-ONLY'
    elif args.mode == 'mcm-test':
        if not args.stage:
            raise ValueError('--stage is required')
        entries.update(validate_mcm_test(args.stage))
        entries['JGJG-ARMOR-TEST-ONLY.txt'] = (
            b'TEST BUILD: Applies armor appearance overrides to the player through an SKSE render hook. Includes save/load serialization and a temporary on-character appearance preview. Weapons, favorite presets, and the side-by-side 3D preview are not included yet. Use a disposable MO2/Vortex profile and save. Requires Skyrim Steam 1.6.1170, matching SKSE, SkyUI, and Address Library.\n'
        )
        suffix = 'ARMOR-TEST-ONLY'
    else:
        if not args.stage:
            raise ValueError('--stage is required')
        validate_runtime(args.stage, metadata)
        # Data-relative root works in MO2/Vortex; manual install copies to Data.
        for p in args.stage.rglob('*'):
            if p.is_file():
                if p.is_symlink(): raise ValueError('Symlinks are not allowed')
                entries[p.relative_to(args.stage).as_posix()] = p.read_bytes()
        entries['JGJG-INSTALL.txt'] = (ROOT/'docs/INSTALL.md').read_bytes()
        suffix = 'Steam-1.6.1170'
    if args.mode != 'source':
        entries['THIRD-PARTY-NOTICES.txt'] = (ROOT/'THIRD-PARTY-NOTICES.txt').read_bytes()
    path = args.out/f'JGJGs-Transmog-System-{version}-{suffix}.zip'
    archive(path, entries)
    sums = []
    for p in sorted(args.out.glob('*.zip')):
        sums.append(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}')
    (args.out/'SHA256SUMS.txt').write_text('\n'.join(sums)+'\n')
    print(path)
if __name__ == '__main__':
    main()
