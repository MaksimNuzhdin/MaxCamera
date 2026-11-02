#!/usr/bin/env python3
"""Independent GVCP/GVSP wire probe; saves downloaded XML, coherent raw frames and JSON stats."""
import argparse
import json
from pathlib import Path
import socket
import struct
import time
import xml.etree.ElementTree as ET

class Control:
    def __init__(self, ip, bind):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((bind, 0))
        self.sock.settimeout(0.25)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.ip, self.request_id = ip, 0

    def command(self, opcode, body=b'', flags=1):
        self.request_id = self.request_id % 65535 + 1
        wire = struct.pack('!BBHHH', 0x42, flags, opcode, len(body), self.request_id) + body
        for _ in range(4):
            self.sock.sendto(wire, (self.ip, 3956))
            until = time.monotonic() + 0.3
            while time.monotonic() < until:
                try:
                    ack, peer = self.sock.recvfrom(2048)
                except socket.timeout:
                    break
                if len(ack) < 8:
                    continue
                status, code, length, rid = struct.unpack('!HHHH', ack[:8])
                if rid != self.request_id or code != opcode + 1:
                    continue
                if self.ip != '255.255.255.255' and peer[0] != self.ip:
                    continue
                if len(ack) != 8 + length:
                    raise RuntimeError('Malformed ACK length')
                if status:
                    raise RuntimeError(f'GVCP {opcode:04x} error {status:04x}: {ack[8:].hex()}')
                return ack[8:], peer
        raise TimeoutError(f'No GVCP ACK from {self.ip} for {opcode:04x}')

    def read(self, address):
        body, _ = self.command(0x80, struct.pack('!I', address))
        return struct.unpack('!I', body)[0]

    def write(self, address, value):
        data, _ = self.command(0x82, struct.pack('!II', address, value))
        assert data == b'\0\0\0\1', data.hex()

    def memory(self, address, length):
        data, _ = self.command(0x84, struct.pack('!IHH', address, 0, length))
        assert struct.unpack('!I', data[:4])[0] == address
        return data[4:]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--ip', default='255.255.255.255')
    p.add_argument('--bind', default='0.0.0.0')
    p.add_argument('--seconds', type=float, default=10)
    p.add_argument('--cycles', type=int, default=3)
    p.add_argument('--output', type=Path, default=Path('diagnostics/gige-probe'))
    p.add_argument('--discover-only', action='store_true')
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    c = Control(args.ip, args.bind)
    discovery, peer = c.command(2, flags=0x11)
    assert len(discovery) == 248
    mac = ':'.join(f'{x:02x}' for x in discovery[10:16])
    ip = socket.inet_ntoa(discovery[36:40])
    identity = {name: discovery[start:start+length].split(b'\0')[0].decode('ascii')
                for name, start, length in [('vendor',72,32),('model',104,32),('serial',216,16)]}
    print(f'DISCOVERY ip={ip} mac={mac} identity={identity}', flush=True)
    if args.discover_only:
        return
    # Only this project's explicitly provisioned development unit may be controlled.
    if mac != '02:00:00:00:00:01':
        raise RuntimeError('Unexpected unit MAC; refusing to control another device')
    c.ip = ip
    route = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    route.connect((ip, 3956))
    host_ip = args.bind if args.bind != '0.0.0.0' else route.getsockname()[0]
    route.close()
    url = c.memory(0x200,512).split(b'\0')[0].decode('ascii')
    filename, addr, length = url.removeprefix('Local:').split(';')
    addr, length = int(addr,16), int(length,16)
    xml = b''.join(c.memory(addr+off, min(512, ((length-off+3)//4)*4))
                   for off in range(0,length,512))[:length]
    ET.fromstring(xml)
    (args.output/'camera.xml').write_bytes(xml)
    print(f'XML {url}, {len(xml)} bytes parsed', flush=True)
    receiver = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    receiver.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,4*1024*1024)
    receiver.bind((host_ip,0));receiver.settimeout(0.1)
    results = []
    c.write(0xa00,2)
    try:
        for cycle in range(args.cycles):
            c.write(0xd18,int.from_bytes(socket.inet_aton(host_ip),'big'))
            c.write(0xd00,receiver.getsockname()[1])
            c.write(0xd04,1500)
            c.write(0xd08,50)
            c.write(0x1001c,1)
            payload = c.read(0x1000c)
            width,height,pixel_format=c.read(0x10000),c.read(0x10004),c.read(0x10008)
            assert payload==width*height*((pixel_format>>16)&255)//8
            c.write(0x10014,1)
            begin = heartbeat = time.monotonic()
            frames = incomplete = packets = 0
            frame = None
            block = expected = offset = 0
            latest = None
            while time.monotonic()-begin < args.seconds:
                if time.monotonic()-heartbeat > 0.5:
                    assert c.read(0xa00) == 2
                    heartbeat = time.monotonic()
                try:
                    data, source = receiver.recvfrom(2048)
                except socket.timeout:
                    continue
                if source[0] != ip or len(data)<8:
                    continue
                status, bid, info = struct.unpack('!HHI',data[:8]);kind,pid=info>>24,info&0xffffff
                packets += 1
                assert status == 0
                if bid == 0:
                    continue
                if kind == 1:
                    if frame is not None:
                        incomplete += 1
                    assert pid == 0 and len(data) == 44
                    assert struct.unpack_from('!III',data,20) == (pixel_format,width,height)
                    frame,block,expected,offset = bytearray(payload),bid,1,0
                elif bid == block and frame is not None:
                    if pid != expected:
                        incomplete += 1;frame = None;continue
                    expected += 1
                    if kind == 3:
                        if offset+len(data)-8 > payload:
                            raise RuntimeError('Payload overrun')
                        frame[offset:offset+len(data)-8] = data[8:]
                        offset += len(data)-8
                    elif kind == 2:
                        assert len(data)==16 and struct.unpack_from('!II',data,8)==(1,height)
                        if offset == payload:
                            frames += 1;latest = bytes(frame)
                        else:
                            incomplete += 1
                        frame = None
            c.write(0x10014,0)
            c.write(0x1001c,0)
            elapsed=time.monotonic()-begin
            result={'cycle':cycle+1,'frames':frames,'incomplete':incomplete,
                    'packets':packets,'seconds':elapsed,'fps':frames/elapsed,
                    'width':width,'height':height,'pixel_format':hex(pixel_format),'payload':payload}
            results.append(result)
            print(json.dumps(result),flush=True)
            if latest:
                (args.output/f'frame-{cycle+1}.raw').write_bytes(latest)
                if pixel_format==0x01080001:
                    (args.output/f'frame-{cycle+1}.pgm').write_bytes(f'P5\n{width} {height}\n255\n'.encode()+latest)
                elif pixel_format==0x02180014:
                    (args.output/f'frame-{cycle+1}.ppm').write_bytes(f'P6\n{width} {height}\n255\n'.encode()+latest)
            if not frames:
                raise RuntimeError('No complete frames received')
            time.sleep(0.25)
    finally:
        try:
            c.write(0x10014,0);c.write(0x1001c,0);c.write(0xa00,0)
        finally:
            receiver.close();c.sock.close()
            (args.output/'stats.json').write_text(json.dumps({'ip':ip,'mac':mac,**identity,'cycles':results},indent=2)+'\n')

if __name__ == '__main__':
    main()
