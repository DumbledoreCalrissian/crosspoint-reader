"""Independent little-endian weather v1 fixture; no firmware codec dependency."""
import datetime
from pathlib import Path
import struct
import zlib

def day(date):
    return (datetime.date.fromisoformat(date) - datetime.date(1970, 1, 1)).days

current = day('2026-11-15')
payload = struct.pack('<4sHHQBBBii80s64s3s', b'WTHR', 1, 0, 1, 1, 1, 0,
                      35681236, 139767125, b'Tokyo', b'Tokyo', b'JP')
payload += struct.pack('<iHhiQQBBB', current, 720, 140, 32400,
                       current * 86400 + 720 * 60 - 32400, 0, 64, 61, 1)
for i, (high, low, rain, chance, code) in enumerate(
        [(160, 90, 24, 70, 61), (150, 80, 8, 40, 51), (140, 80, 0, 10, 3)]):
    payload += struct.pack('<ihhHBB', current + i, high, low, rain, chance, code)
payload += bytes(508 - len(payload))
payload += struct.pack('<I', zlib.crc32(payload))
Path(__file__).with_name('golden-weather-v1.bin').write_bytes(payload)
