"""Read JPEG/PNG dimensions without allocating a decoded image."""
import struct


def validate_reference(data, max_bytes=4*1024*1024):
    if len(data) > max_bytes:
        raise ValueError('Reference image exceeds the storage limit')
    width = height = 0
    if data.startswith(b'\x89PNG\r\n\x1a\n') and len(data) >= 24 and data[12:16] == b'IHDR':
        width, height = struct.unpack('>II', data[16:24])
    elif data.startswith(b'\xff\xd8\xff'):
        offset = 2
        while offset < len(data):
            if data[offset] != 255:
                break
            while offset < len(data) and data[offset] == 255:
                offset += 1
            if offset >= len(data):
                break
            marker = data[offset]
            offset += 1
            if marker in (0xd9, 0xda):
                break
            if marker in (0xd8, 0x01) or 0xd0 <= marker <= 0xd7:
                continue
            if offset+2 > len(data):
                break
            length = int.from_bytes(data[offset:offset+2], 'big')
            if length < 2 or offset+length > len(data):
                break
            if marker in (0xc0, 0xc1, 0xc2) and length >= 8:
                height, width = struct.unpack('>HH', data[offset+3:offset+7])
                break
            offset += length
    if not width or not height or width*height > 12000000 or max(width, height) > 8192:
        raise ValueError('Provide a JPEG/PNG up to 12 megapixels, maximum dimension 8192')
    return width, height
