"""Own raw-block archive container writer; templates come only from the local game."""
import struct
from decode_bank import align

def encode_raw_bank(bank, template):
    if len(template) < 2048 or template[:16] != bytes.fromhex('00000000000000020000000000000001'):
        raise ValueError('Unknown original compressed-bank header')
    chunk, table, old_start = struct.unpack_from('>3I',template,16)
    if (chunk,table) != (4096,2048) or struct.unpack_from('>I',template,48)[0] != 131072:
        raise ValueError('Unsupported native block/table/page layout')
    if old_start != len(template) or old_start % 2048 or len(bank) % chunk:
        raise ValueError('Raw BANK must contain complete 4096-byte blocks: native decoder treats partial raw tails as LZO')
    count = (len(bank) + chunk - 1) // chunk
    start = align(table + count * 4,2048)
    header = bytearray(start)
    header[:min(start,len(template))] = template[:min(start,len(template))]
    struct.pack_into('>I',header,24,start)
    struct.pack_into('>QQ',header,32,len(bank),len(bank))
    for i in range(count):
        struct.pack_into('>I',header,table+i*4,min(chunk,len(bank)-i*chunk))
    # Native 96B9E0 selects raw copying only for size == fixed chunk (4096).
    # A 2048-byte raw tail is incorrectly dispatched as LZO and can overrun.
    # Full 4096-byte blocks divide the 131072-byte page exactly.
    return bytes(header) + bank
