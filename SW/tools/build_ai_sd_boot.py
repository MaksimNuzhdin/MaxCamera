#!/usr/bin/env python3
"""Build a MicroSD BOOT.BIN with R5 camera + A53 Linux, without programming media."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
from build_ai_rootfs import ROOT, VENDOR

OUT = ROOT/'build/ai-linux'

def run(*args):
    return subprocess.run([str(a) for a in args], check=True, text=True, capture_output=True).stdout

def nodes(dtb, path='/'):
    yield path
    for child in run('fdtget', '-l', dtb, path).split():
        yield from nodes(dtb, path.rstrip('/')+'/'+child)

def main():
    global OUT
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', type=Path, default=OUT, help='Isolated directory containing rootfs and receiving boot artifacts')
    parser.add_argument('--usb-device-node', help='Verified DWC3 child path; enables only its parent and USB2 peripheral mode')
    parser.add_argument('--r5', type=Path, default=ROOT/'build/ai-r5/MaxCameraApp.elf')
    parser.add_argument('--bit', type=Path, default=ROOT/'diagnostics/fpga-ppc4-v13-20261102/logic-ppc4-v13.bit')
    parser.add_argument('--bl31', type=Path, default=ROOT/'build/ai-linux/tfa-uart1/zynqmp/release/bl31/bl31.elf')
    args = parser.parse_args()
    OUT = args.out.resolve()
    OUT.mkdir(parents=True, exist_ok=True)
    initrd = OUT/'rootfs.cpio.gz'
    assert initrd.is_file(), 'Build the rootfs first'
    assert 0x26000000+initrd.stat().st_size<0x3f000000, 'Initramfs exceeds its DDR loading range'
    # Reject incompatible Image and every R5 load segment crossing the carveout.
    image = (VENDOR/'Image').read_bytes()
    assert image[56:60] == b'ARM\x64', 'Not an AArch64 Linux Image'
    assert struct.unpack_from('<Q',image,8)[0] == 0, 'This entry stub requires an Image with text_offset zero'
    assert 0x20200000+struct.unpack_from('<Q',image,16)[0] < 0x22000000, 'Kernel runtime footprint overlaps the DTB'
    data = args.r5.read_bytes()
    assert data[:5] == b'\x7fELF\x01', 'R5 must be an ELF32 image'
    phoff = struct.unpack_from('<I', data, 28)[0]
    phsize, phnum = struct.unpack_from('<HH', data, 42)
    for i in range(phnum):
        kind, offset, va, pa, file_size, mem_size, flags, align = struct.unpack_from('<8I', data, phoff+i*phsize)
        if kind == 1:
            assert pa+mem_size <= 0x10000000 or 0xfffc0000 <= pa < 0x100000000, (hex(pa), mem_size)
    dtb = OUT/'system.dtb';shutil.copy2(VENDOR/'system.dtb', dtb)
    # Disable all generated PL peripherals and all R5-owned PS peripherals.
    # Keep firmware/clocks, interrupt controller, timer, UART console and SD only.
    disabled = []
    for path in list(nodes(dtb)):
        name = path.rsplit('/',1)[-1]
        if path.count('/') == 1 and name.startswith(('amba_pl','pl-bus','vcap_','fpga-region','zynqmp-ipi')):
            run('fdtput', '-t', 's', dtb, path, 'status', 'disabled');disabled.append(path)
        if path.startswith('/amba/') or path.startswith('/axi/'):
            safe = ('interrupt-controller', 'serial', 'mmc', 'cci@')
            if path.count('/') == 2 and not name.startswith(safe):
                run('fdtput', '-t', 's', dtb, path, 'status', 'disabled');disabled.append(path)
        if 'remoteproc' in name or 'r5f' in name or 'rpu' in name:
            run('fdtput', '-t', 's', dtb, path, 'status', 'disabled');disabled.append(path)
    for path in list(nodes(dtb)):
        if path.rsplit('/',1)[-1].startswith('memory@'):
            run('fdtput', '-t', 'x', dtb, path, 'reg', '0', '10000000', '0', '30000000')
    existing = list(nodes(dtb))
    if '/reserved-memory' not in existing:
        run('fdtput', '-c', dtb, '/reserved-memory')
    for prop in ('#address-cells', '#size-cells'):
        run('fdtput', '-t', 'x', dtb, '/reserved-memory', prop, '2')
    run('fdtput', dtb, '/reserved-memory', 'ranges')
    run('fdtput', '-c', dtb, '/reserved-memory/maxcamera-ipc@1f000000')
    run('fdtput', '-t', 'x', dtb, '/reserved-memory/maxcamera-ipc@1f000000', 'reg', '0', '1f000000', '0', '1000000')
    run('fdtput', dtb, '/reserved-memory/maxcamera-ipc@1f000000', 'no-map')
    # Existing CMA/video pools are removed because the inference worker owns no DMA.
    for path in run('fdtget', '-l', dtb, '/reserved-memory').split():
        if path != 'maxcamera-ipc@1f000000':
            run('fdtput', '-r', dtb, '/reserved-memory/'+path)
    if args.usb_device_node:
        node = args.usb_device_node
        if node not in existing or run('fdtget', '-t', 's', dtb, node, 'compatible').strip() != 'snps,dwc3':
            raise ValueError('USB device node must identify the board-verified snps,dwc3 controller')
        parent = node.rsplit('/', 1)[0]
        for path in (parent, node):
            run('fdtput', '-t', 's', dtb, path, 'status', 'okay')
            if path in disabled:
                disabled.remove(path)
        run('fdtput', '-t', 's', dtb, node, 'dr_mode', 'peripheral')
        run('fdtput', '-t', 's', dtb, node, 'maximum-speed', 'high-speed')
        # Do not enable shared GPIO/PHY controllers automatically. Board reset,
        # pinmux and clock ownership must be checked before loading this candidate.
    bootargs = 'console=ttyPS0,115200 earlycon clk_ignore_unused pd_ignore_unused maxcpus=1 nr_cpus=1 cma=0 swiotlb=128 rootfstype=ramfs init=/init loglevel=6'
    run('fdtput', '-t', 's', dtb, '/chosen', 'bootargs', bootargs)
    for key, value in [('linux,initrd-start', 0x26000000), ('linux,initrd-end', 0x26000000+initrd.stat().st_size)]:
        run('fdtput', '-t', 'x', dtb, '/chosen', key, '0', f'{value:x}')
    cc = Path(os.environ.get('MAX_CAMERA_AARCH64_CC',str(Path(os.environ.get('VITIS_ROOT','/path/to/Vitis/2024.2'))/'gnu/aarch64/lin/aarch64-none/bin/aarch64-none-elf-gcc')))
    entry = OUT/'linux-entry.elf'
    run(cc, '-mcpu=cortex-a53', '-nostdlib', '-Wl,-T,'+str(ROOT/'linux/ai/entry.ld')+',--build-id=none', ROOT/'linux/ai/entry.S', '-o', entry)
    platform = Path(os.environ.get('MAX_CAMERA_BOOT_COMPONENTS', str(ROOT.parent/'platform/export/platform/sw/boot')))
    parts = [('[bootloader,destination_cpu=a53-0]', platform/'fsbl.elf'),
             ('[pmufw_image]', platform/'pmufw.elf'),
             ('[destination_device=pl]', args.bit.resolve()),
             ('[destination_cpu=r5-0]', args.r5.resolve()),
             ('[destination_cpu=a53-0,exception_level=el-3,trustzone]', args.bl31.resolve()),
             ('[destination_cpu=a53-0,load=0x20200000]', VENDOR/'Image'),
             ('[destination_cpu=a53-0,load=0x22000000]', dtb),
             ('[destination_cpu=a53-0,load=0x26000000]', initrd),
             ('[destination_cpu=a53-0,exception_level=el-2]', entry)]
    for _, p in parts:
        assert p.is_file(), p
    bif = OUT/'boot.bif';bif.write_text('maxcamera:\n{\n'+''.join(f'  {attrs} {p}\n' for attrs,p in parts)+'}\n')
    bootgen = Path(os.environ.get('VITIS_ROOT','/path/to/Vitis/2024.2'))/'bin/bootgen'
    run(bootgen, '-arch', 'zynqmp', '-image', bif, '-w', 'on', '-o', OUT/'BOOT.BIN')
    (OUT/'boot-headers.txt').write_text(run(bootgen, '-arch', 'zynqmp', '-read', OUT/'BOOT.BIN'))
    manifest = dict(usb_device_node=args.usb_device_node, status='BUILT_NOT_BOARD_QUALIFIED', disabled_linux_nodes=disabled,
                    bootargs=bootargs, memory=dict(r5_end='0x10000000', linux_start='0x10000000', ipc_start='0x1f000000'),
                    inputs={str(p): hashlib.sha256(p.read_bytes()).hexdigest() for _,p in parts})
    (OUT/'boot-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    files = [OUT/'BOOT.BIN', dtb, initrd, entry]
    (OUT/'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in files))
    print(f'Built {OUT}/BOOT.BIN; MicroSD hardware boot is unqualified.')

if __name__ == '__main__':
    main()
