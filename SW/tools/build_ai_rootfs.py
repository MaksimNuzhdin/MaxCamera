#!/usr/bin/env python3
"""Build a minimal A53 initramfs from the installed PetaLinux SDK and pinned wheels."""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SDK = Path(os.environ.get('MAX_CAMERA_LINUX_SYSROOT', '/opt/petalinux/2024.2/sysroots/cortexa72-cortexa53-xilinx-linux'))
VENDOR = Path(os.environ.get('MAX_CAMERA_LINUX_ARTIFACTS', '/home/maxim/workarea/axu2cgb/axu2cgb_fpga/images/linux'))


def vendor_busybox():
    data = (VENDOR/'rootfs.cpio').read_bytes()
    pos = 0
    while data[pos:pos+6] in (b'070701', b'070702'):
        fields = [int(data[pos+6+i*8:pos+14+i*8], 16) for i in range(13)]
        size, namesize = fields[6], fields[11]
        name = data[pos+110:pos+110+namesize-1].decode()
        start = (pos+110+namesize+3) & ~3
        if name in ('usr/bin/busybox', './usr/bin/busybox'):
            return data[start:start+size]
        pos = (start+size+3) & ~3
    raise RuntimeError('Vendor BusyBox was not found')


def archive(tree, output):
    """Root-owned reproducible newc, including devices without host mknod/root."""
    with output.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as stream:
        index = 0
        def emit(name, mode, data=b'', major=0, minor=0):
            nonlocal index
            index += 1
            encoded = name.encode()+b'\0'
            values = [index, mode, 0, 0, 1, 0, len(data), 0, 0, major, minor, len(encoded), 0]
            header = b'070701'+b''.join(f'{v:08x}'.encode() for v in values)
            stream.write(header+encoded+b'\0'*((-110-len(encoded)) % 4))
            stream.write(data+b'\0'*(-len(data) % 4))
        for p in sorted(tree.rglob('*')):
            name = p.relative_to(tree).as_posix()
            mode = p.lstat().st_mode
            payload = os.readlink(p).encode() if p.is_symlink() else p.read_bytes() if p.is_file() else b''
            emit(name, mode, payload)
        emit('dev/console', stat.S_IFCHR | 0o600, major=5, minor=1)
        emit('dev/null', stat.S_IFCHR | 0o666, major=1, minor=3)
        emit('TRAILER!!!', 0)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', type=Path, default=ROOT/'build/ai-linux')
    args = parser.parse_args()
    out = args.out.resolve();tree = Path('/tmp/maxcamera-ai-rootfs')
    out.mkdir(parents=True, exist_ok=True)
    if tree.exists():
        shutil.rmtree(tree)
    for name in ('usr/bin', 'usr/lib', 'bin', 'sbin', 'lib', 'etc', 'proc', 'sys', 'dev', 'tmp', 'run', 'media/sd', 'opt/maxcamera'):
        (tree/name).mkdir(parents=True, exist_ok=True)
    (tree/'usr/bin/busybox').write_bytes(vendor_busybox())
    (tree/'usr/bin/busybox').chmod(0o755)
    for name in ('sh', 'mount', 'mkdir', 'sleep', 'cat', 'echo', 'poweroff'):
        (tree/'bin'/name).symlink_to('/usr/bin/busybox')
    shutil.copy2(SDK/'usr/bin/python3.12', tree/'usr/bin/python3.12')
    (tree/'usr/bin/python3').symlink_to('python3.12')
    shutil.copytree(SDK/'usr/lib/python3.12', tree/'usr/lib/python3.12',
                    ignore=shutil.ignore_patterns('site-packages', '__pycache__', 'test', 'tests', '.debug', '*.a', '*.pyc'))
    # The exported SDK omits mmap; use the matching installed target component.
    mmap_source = Path(os.environ.get('MAX_CAMERA_MMAP_MODULE', str(VENDOR.parents[1]/'build/tmp/sysroots-components/cortexa72-cortexa53/python3/usr/lib/python3.12/lib-dynload/mmap.cpython-312-aarch64-linux-gnu.so')))
    shutil.copy2(mmap_source, tree/'usr/lib/python3.12/lib-dynload'/mmap_source.name)
    packages = tree/'usr/lib/python3.12/site-packages';packages.mkdir()
    wheels = sorted((ROOT/'build/ai-downloads').glob('*.whl'))
    if len(wheels) != 2:
        raise RuntimeError('Download the pinned NumPy and OpenCV AArch64 wheels first')
    for wheel in wheels:
        with zipfile.ZipFile(wheel) as z:
            z.extractall(packages)
    for model in json.loads((ROOT/'ai/models/MANIFEST.json').read_text())['models']:
        data = (ROOT/'ai/models'/model['file']).read_bytes()
        if hashlib.sha256(data).hexdigest() != model['sha256']:
            raise RuntimeError('Model integrity mismatch: '+model['file'])
    shutil.copytree(ROOT/'ai', tree/'opt/maxcamera/ai', ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
    cc = Path(os.environ.get('MAX_CAMERA_LINUX_CC', '/opt/petalinux/2024.2/sysroots/x86_64-petalinux-linux/usr/bin/aarch64-xilinx-linux/aarch64-xilinx-linux-gcc'))
    subprocess.run([str(cc), '--sysroot='+str(SDK), '-mcpu=cortex-a53', '-shared', '-fPIC', '-O2', str(ROOT/'ai/barrier.c'), '-o', str(tree/'opt/maxcamera/ai/libai_barrier.so')], check=True)
    # Resolve all ELF shared dependencies against the matching generic A53 SDK.
    pending = [p for p in tree.rglob('*') if p.is_file() and not p.is_symlink() and p.open('rb').read(4) == b'\x7fELF']
    known = {p.name for p in pending};missing = set()
    while pending:
        binary = pending.pop()
        text = subprocess.run(['readelf', '-d', str(binary)], capture_output=True, text=True, check=True).stdout
        names = re.findall(r'\(NEEDED\).*\[(.*?)\]', text)
        if binary.name in ('python3.12', 'busybox'):
            names.append('ld-linux-aarch64.so.1')
        for name in names:
            if name in known:
                continue
            source = next((d/name for d in (SDK/'lib', SDK/'usr/lib') if (d/name).exists()), None)
            if source is None:
                missing.add(name);continue
            dest = tree/'usr/lib'/name
            shutil.copy2(source.resolve(), dest)
            known.add(name);pending.append(dest)
    if missing:
        raise RuntimeError('Unresolved ELF dependencies: '+str(sorted(missing)))
    for p in (tree/'usr/lib').glob('*.so*'):
        (tree/'lib'/p.name).symlink_to('/usr/lib/'+p.name)
    shutil.copy2(ROOT/'linux/ai/init', tree/'init');(tree/'init').chmod(0o755)
    (tree/'etc/ld.so.conf').write_text('/usr/lib\n/lib\n')
    archive(tree, out/'rootfs.cpio.gz')
    manifest = dict(wheels={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in wheels},
                    source_sdk=str(SDK), vendor_busybox_source=str(VENDOR/'rootfs.cpio'),
                    mmap_module_source=str(mmap_source),
                    mmap_module_sha256=hashlib.sha256(mmap_source.read_bytes()).hexdigest(),
                    rootfs_sha256=hashlib.sha256((out/'rootfs.cpio.gz').read_bytes()).hexdigest(),
                    rootfs_bytes=(out/'rootfs.cpio.gz').stat().st_size)
    (out/'rootfs-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(json.dumps(manifest, indent=2))

if __name__ == '__main__':
    main()
