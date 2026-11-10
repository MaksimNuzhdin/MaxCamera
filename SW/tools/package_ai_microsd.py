#!/usr/bin/env python3
"""Create a FAT32 MicroSD disk image with BOOT.BIN and empty enrollment folders."""
import argparse
import datetime
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/ai-linux'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=OUT, help='Directory containing the matching BOOT.BIN')
    parser.add_argument('--image-name', default='maxcamera-ai-microsd.img')
    parser.add_argument('--date', default='2026-11-10', help='Reproducible FAT date, YYYY-MM-DD')
    parser.add_argument('--readme', type=Path, help='Optional English README.TXT to include at the card root')
    args = parser.parse_args()
    if Path(args.image_name).name != args.image_name or not args.image_name.endswith('.img'):
        parser.error('Provide a simple .img filename')
    stamp = datetime.date.fromisoformat(args.date)
    if not 1980 <= stamp.year <= 2107:
        parser.error('FAT date must be within1980–2107')
    out = args.out.resolve()
    boot = out/'BOOT.BIN'
    assert boot.is_file()
    boot_hash = hashlib.sha256(boot.read_bytes()).hexdigest()
    image = out/args.image_name
    partition_size, start = 512*1024*1024, 2048
    image_size = start*512+partition_size
    with tempfile.TemporaryDirectory(prefix='maxcamera-fat-') as directory:
        fatpath = Path(directory)/'partition.img'
        with fatpath.open('wb') as f:
            f.truncate(partition_size)
        subprocess.run(['mkfs.vfat','-F','32','-n','MAXCAMERA','-i',boot_hash[:8],'-h',str(start),str(fatpath)],check=True)
        with fatpath.open('r+b') as f:
            bpb = f.read(512)
            sector = struct.unpack_from('<H',bpb,11)[0]
            spc = bpb[13];reserved = struct.unpack_from('<H',bpb,14)[0];copies = bpb[16]
            fat_sectors = struct.unpack_from('<I',bpb,36)[0]
            root = struct.unpack_from('<I',bpb,44)[0]
            assert sector == 512 and root == 2 and copies == 2
            f.seek(reserved*sector);fat = bytearray(f.read(fat_sectors*sector))
            data_start = (reserved+copies*fat_sectors)*sector
            cluster_bytes = spc*sector
            next_cluster = 3
            def offset(cluster):
                return data_start+(cluster-2)*cluster_bytes
            def allocate(count):
                nonlocal next_cluster
                clusters = list(range(next_cluster,next_cluster+count));next_cluster += count
                for index, cluster in enumerate(clusters):
                    assert (cluster+1)*4 <= len(fat)
                    struct.pack_into('<I',fat,cluster*4,clusters[index+1] if index+1<count else 0x0fffffff)
                return clusters
            def entry(name, cluster, size=0, isdir=False):
                # Every emitted name is deliberately short 8.3 ASCII.
                base, _, ext = name.partition('.') if name not in ('.','..') else (name,'','')
                result = bytearray(32);result[:11]=(base.ljust(8)+ext.ljust(3)).encode('ascii')
                result[11] = 0x10 if isdir else 0x20
                date = ((stamp.year-1980)<<9)|(stamp.month<<5)|stamp.day
                struct.pack_into('<H',result,16,date);struct.pack_into('<H',result,18,date);struct.pack_into('<H',result,24,date)
                struct.pack_into('<H',result,20,cluster>>16);struct.pack_into('<H',result,26,cluster&0xffff)
                struct.pack_into('<I',result,28,size)
                return result
            assert boot.stat().st_size < partition_size-64*1024*1024, 'BOOT.BIN leaves insufficient preset space'
            count = (boot.stat().st_size+cluster_bytes-1)//cluster_bytes
            boot_clusters = allocate(count)
            with boot.open('rb') as source:
                for cluster in boot_clusters:
                    f.seek(offset(cluster));f.write(source.read(cluster_bytes))
            extra_entries=[]
            extra_files={'HASHES.TXT':(boot_hash+'  BOOT.BIN\r\n').encode('ascii')}
            if args.readme:
                text=args.readme.read_text(encoding='utf-8')
                if any('\u0400' <= ch <= '\u052f' for ch in text):
                    raise ValueError('README.TXT must follow the English-only policy')
                extra_files['README.TXT']=text.encode('utf-8')
            for name,payload in extra_files.items():
                clusters=allocate(max(1,(len(payload)+cluster_bytes-1)//cluster_bytes))
                for index,cluster in enumerate(clusters):
                    f.seek(offset(cluster));f.write(payload[index*cluster_bytes:(index+1)*cluster_bytes])
                extra_entries.append(entry(name,clusters[0],len(payload)))
            presets, faces, plates, objects = [allocate(1)[0] for _ in range(4)]
            def write_dir(cluster, entries):
                payload = b''.join(entries)+bytes(32)
                assert len(payload)<cluster_bytes
                f.seek(offset(cluster));f.write(payload)
            label=bytearray(32);label[:11]=b'MAXCAMERA  ';label[11]=8
            write_dir(root,[label,entry('BOOT.BIN',boot_clusters[0],boot.stat().st_size),entry('PRESETS',presets,isdir=True)]+extra_entries)
            write_dir(presets,[entry('.',presets,isdir=True),entry('..',0,isdir=True),
                               entry('FACES',faces,isdir=True),entry('PLATES',plates,isdir=True),entry('OBJECTS',objects,isdir=True)])
            for cluster in (faces,plates,objects):
                write_dir(cluster,[entry('.',cluster,isdir=True),entry('..',presets,isdir=True)])
            for copy in range(copies):
                f.seek((reserved+copy*fat_sectors)*sector);f.write(fat)
            # Refresh FSInfo free counts after allocation, including its backup.
            info = struct.unpack_from('<H',bpb,48)[0];backup = struct.unpack_from('<H',bpb,50)[0]
            total_sectors=struct.unpack_from('<I',bpb,32)[0]
            cluster_count=(total_sectors-reserved-copies*fat_sectors)//spc
            free_count=cluster_count-(next_cluster-2)
            for block in (info,backup+info):
                f.seek(block*sector+488);f.write(struct.pack('<II',free_count,next_cluster))
        subprocess.run(['fsck.fat','-n',str(fatpath)],check=True)
        with image.open('wb') as disk:
            disk.truncate(image_size)
            mbr = bytearray(512)
            mbr[446:462] = struct.pack('<B3sB3sII',0x80,b'\xfe\xff\xff',0x0c,b'\xfe\xff\xff',start,partition_size//512)
            mbr[510:] = b'\x55\xaa';disk.seek(0);disk.write(mbr);disk.seek(start*512)
            with fatpath.open('rb') as source:
                while data := source.read(4*1024*1024):
                    disk.write(data)
    # Read back the exact BOOT.BIN chain from the final image and compare hashes.
    digest = hashlib.sha256()
    with image.open('rb') as f:
        remaining = boot.stat().st_size
        for cluster in boot_clusters:
            f.seek(start*512+offset(cluster));chunk=f.read(min(cluster_bytes,remaining));digest.update(chunk);remaining-=len(chunk)
    assert remaining == 0 and digest.digest() == hashlib.sha256(boot.read_bytes()).digest()
    with image.open('rb') as f:
        disk_hash = hashlib.file_digest(f,'sha256').hexdigest() if hasattr(hashlib,'file_digest') else None
    if disk_hash is None:
        digest = hashlib.sha256()
        with image.open('rb') as f:
            while chunk := f.read(4*1024*1024):digest.update(chunk)
        disk_hash = digest.hexdigest()
    (out/'MICROSD-SHA256SUMS').write_text(f'{disk_hash}  {image.name}\n')
    print(f'Created {image} ({image_size} bytes); BOOT.BIN readback SHA256 verified. No physical media written.')

if __name__ == '__main__':
    main()
