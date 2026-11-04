"""Bounded original LZO1X/v0 block reader, written from the instruction description.

Format reference: https://docs.kernel.org/staging/lzo.html
This is not a port of Linux or MiniLZO source. RLE/v1 streams are unsupported.
No compressor, file writes, dictionary sharing or unchecked buffer operations.
"""

def decompress(source, expected_size):
    if not 0 < expected_size <= 1024 * 1024:
        raise ValueError('Block output limit exceeded')
    cursor = 0
    output = bytearray()

    def byte():
        nonlocal cursor
        if cursor >= len(source):
            raise ValueError('Truncated LZO instruction')
        value = source[cursor]
        cursor += 1
        return value

    def literals(count):
        nonlocal cursor
        if cursor + count > len(source) or len(output) + count > expected_size:
            raise ValueError('Literal range exceeds block bounds')
        output.extend(source[cursor:cursor + count])
        cursor += count

    def length(value, mask, extra):
        if value:
            return value + extra
        value = mask
        while True:
            extension = byte()
            value += extension or 255
            if value + extra > expected_size:
                raise ValueError('Extended length exceeds output limit')
            if extension:
                return value + extra

    def match(distance, count):
        if distance <= 0 or distance > len(output) or len(output) + count > expected_size:
            raise ValueError(f'Back-reference exceeds block bounds: distance={distance}, count={count}, output={len(output)}, input={cursor}')
        for _ in range(count):
            output.append(output[-distance])

    state = 0
    opcode = byte()
    if opcode > 17:
        initial = opcode - 17
        literals(initial)
        state = min(initial, 4)
        opcode = byte()
    elif opcode == 17 and len(source) >= 5:
        raise ValueError('Versioned LZO/RLE stream unsupported')
    while True:
        if opcode < 16:
            if state == 0:
                literals(length(opcode, 15, 3))
                state = 4
                opcode = byte()
                continue
            distance = (byte() << 2) + (opcode >> 2) + (2049 if state == 4 else 1)
            count = 3 if state == 4 else 2
            state = opcode & 3
        elif opcode < 32:
            count = length(opcode & 7, 7, 2)
            operand = byte() | (byte() << 8)
            distance = 16384 + ((opcode & 8) << 11) + (operand >> 2)
            state = operand & 3
            if distance == 16384:
                if count != 3 or state or cursor != len(source) or len(output) != expected_size:
                    raise ValueError('Invalid LZO terminator or output size')
                return bytes(output)
        elif opcode < 64:
            count = length(opcode & 31, 31, 2)
            operand = byte() | (byte() << 8)
            distance = (operand >> 2) + 1
            state = operand & 3
        else:
            count = (opcode >> 5) + 1
            distance = (byte() << 3) + ((opcode >> 2) & 7) + 1
            state = opcode & 3
        match(distance, count)
        literals(state)
        opcode = byte()
