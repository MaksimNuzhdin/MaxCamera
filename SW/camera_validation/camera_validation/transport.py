"""GVCP control and classic GVSP image reception using only UDP sockets."""
import socket
import struct
import time


class ProtocolError(RuntimeError):
    def __init__(self, status):
        self.status = status
        super().__init__(f'GVCP status 0x{status:04x}')


class Control:
    def __init__(self, ip, bind='0.0.0.0', timeout=1.0):
        self.ip, self.timeout, self.rid = ip, timeout, 0
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((bind, 0))
        self.sock.settimeout(min(timeout, .2))
        self.trace = []

    def close(self):
        self.sock.close()

    def command(self, opcode, body=b'', flags=1):
        self.rid = self.rid % 65535 + 1
        wire = struct.pack('!BBHHH', 0x42, flags, opcode, len(body), self.rid) + body
        self.sock.sendto(wire, (self.ip, 3956))
        # Do not retry writes: a lost ACK must not execute a command twice.
        end = time.monotonic() + self.timeout
        while time.monotonic() < end:
            try:
                ack, peer = self.sock.recvfrom(65535)
            except socket.timeout:
                continue
            if peer != (self.ip, 3956) or len(ack) < 8:
                continue
            status, code, length, rid = struct.unpack('!HHHH', ack[:8])
            if rid != self.rid or code != opcode + 1:
                continue
            sensitive=opcode==0x86 and len(body)>=4 and 0x12000<=struct.unpack_from('!I',body)[0]<0x12020
            self.trace.append({'opcode': opcode, 'request': '[REDACTED]' if sensitive else body.hex(), 'status': status,
                               'response': ack[8:].hex(), 'time': time.time()})
            if length != len(ack) - 8:
                raise ValueError('Invalid GVCP acknowledgment length')
            if status:
                raise ProtocolError(status)
            return ack[8:]
        raise TimeoutError(f'GVCP timeout: opcode={opcode:#x}')

    def read(self, address):
        return struct.unpack('!I', self.command(0x80, struct.pack('!I', address)))[0]

    def write(self, address, value):
        ack = self.command(0x82, struct.pack('!II', address, value & 0xffffffff))
        if ack != b'\0\0\0\1':
            raise ValueError(f'Unexpected write acknowledgment: {ack.hex()}')

    def memory(self, address, length):
        data = self.command(0x84, struct.pack('!IHH', address, 0, length))
        if len(data) != length + 4 or struct.unpack('!I', data[:4])[0] != address:
            raise ValueError('Invalid READMEM response')
        return data[4:]


def receive_frame(control, receiver, timeout, allow_reorder=False):
    """Require complete metadata/payload; optionally reassemble packet permutation."""
    end, heartbeat = time.monotonic() + timeout, None
    frame = None
    while time.monotonic() < end:
        if heartbeat is None or time.monotonic() - heartbeat > .25:
            if control.read(0xa00) != 2:
                raise RuntimeError('Control ownership lost')
            heartbeat = time.monotonic()
        if time.monotonic() >= end:
            break
        try:
            data, peer = receiver.recvfrom(65535)
        except socket.timeout:
            continue
        # Blocking control/socket calls can return after the frame deadline.
        # A late trailer must not turn an expired capture into a success.
        if time.monotonic() >= end:
            break
        if peer[0] != control.ip or len(data) < 8:
            continue
        status, block, info = struct.unpack('!HHI', data[:8])
        kind, packet = info >> 24, info & 0xffffff
        if status:
            raise ValueError(f'GVSP status {status}')
        if kind == 1:
            if frame is not None:
                raise ValueError('Incomplete GVSP frame before next leader')
            if len(data) not in {40,44} or packet != 0:
                raise ValueError('Invalid image/JPEG leader')
            jpeg=len(data)==40
            if jpeg:
                if struct.unpack_from('!H',data,10)[0]!=6:raise ValueError('Invalid JPEG payload type')
                encoded,frequency,encoding=struct.unpack_from('!QQI',data,20)
                if not 0<encoded<=64*1024*1024 or frequency!=1000000 or encoding!=0:
                    raise ValueError('Invalid JPEG leader metadata')
                pixel,width,height=0,0,0
            else:
                # The current MaxCamera image profile has no transmitted ROI
                # offsets or row/frame padding. Do not decode other layouts as
                # an ordinary contiguous image merely because the length fits.
                payload_type = struct.unpack_from('!H', data, 10)[0]
                if payload_type != 1:
                    raise ValueError(f'Unsupported image payload type {payload_type}')
                offset_x, offset_y, padding_x, padding_y = struct.unpack_from('!IIHH', data, 32)
                if offset_x or offset_y:
                    raise ValueError('Unsupported image offsets in current acceptance profile')
                if padding_x or padding_y:
                    raise ValueError('Unsupported image padding in current acceptance profile')
                pixel, width, height = struct.unpack_from('!III', data, 20)
            timestamp = struct.unpack_from('!Q', data, 12)[0]
            frame = dict(block=block, pixel=pixel, width=width, height=height,
                         timestamp=timestamp, data=bytearray(), next_packet=1, jpeg=jpeg, encoded_bytes=encoded if jpeg else None)
            if allow_reorder:
                expected_bytes = encoded if jpeg else width*height*((pixel >> 16) & 255)//8
                if not 0 < expected_bytes <= 64*1024*1024:
                    raise ValueError('Unsupported reordered image payload size')
                frame.update(chunks={}, trailer_packet=None, received_bytes=0, expected_bytes=expected_bytes,
                             maximum_packets=min(expected_bytes,65535))
        elif frame and block == frame['block']:
            if kind not in {2, 3}:
                raise ValueError(f'Unsupported GVSP packet kind {kind}')
            if allow_reorder:
                if kind == 3:
                    if not 1 <= packet <= frame['maximum_packets'] or len(data) == 8:
                        raise ValueError('Invalid reordered data packet')
                    if packet in frame['chunks']:
                        raise ValueError('Duplicate GVSP data packet')
                    if frame['trailer_packet'] is not None and packet >= frame['trailer_packet']:
                        raise ValueError('Data packet after declared trailer ID')
                    frame['chunks'][packet] = data[8:]
                    frame['received_bytes'] += len(data)-8
                    if frame['received_bytes'] > frame['expected_bytes']:
                        raise ValueError('Reordered payload exceeds declared size')
                else:
                    if frame['trailer_packet'] is not None:
                        raise ValueError('Duplicate GVSP trailer')
                    if not 2 <= packet <= frame['maximum_packets']+1:
                        raise ValueError('Invalid reordered trailer ID')
                    if frame['jpeg']:
                        if len(data)!=12 or struct.unpack_from('!I',data,8)[0]!=6:
                            raise ValueError('Invalid JPEG trailer')
                    elif len(data)!=16 or struct.unpack_from('!II',data,8)!=(1,frame['height']):
                        raise ValueError('Invalid image trailer')
                    if any(number >= packet for number in frame['chunks']):
                        raise ValueError('Payload ID exceeds trailer ID')
                    frame['trailer_packet'] = packet
                last = frame['trailer_packet']
                if last is not None and len(frame['chunks']) == last-1:
                    if frame['received_bytes'] != frame['expected_bytes']:
                        raise ValueError('Reordered payload length mismatch')
                    frame['data'] = b''.join(frame['chunks'][number] for number in range(1,last))
                    del frame['chunks']
                    return frame
                continue
            if packet != frame['next_packet']:
                raise ValueError('Missing, duplicate or reordered GVSP packet')
            frame['next_packet'] += 1
            if kind == 3:
                frame['data'].extend(data[8:])
                if len(frame['data']) > 64 * 1024 * 1024:
                    raise ValueError('Frame exceeds safety limit')
            elif kind == 2:
                if frame['jpeg']:
                    if len(data)!=12 or struct.unpack_from('!I',data,8)[0]!=6 or len(frame['data'])!=frame['encoded_bytes']:
                        raise ValueError('Invalid JPEG trailer or encoded length')
                elif len(data) != 16 or struct.unpack_from('!II', data, 8) != (1, frame['height']):
                    raise ValueError('Invalid image trailer')
                return frame
    raise TimeoutError('No complete image received')
