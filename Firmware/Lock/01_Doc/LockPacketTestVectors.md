# Lock Packet Test Vectors

The formal 12-byte CC1101 payload is:

```text
D5 01 ID FLAGS DIST_L DIST_H ANGLE_L ANGLE_H SEQ_L SEQ_H QUALITY CRC8
```

CRC-8 uses polynomial `0x07`, initial value `0x00`, no reflection, and covers bytes 0 through 10.

| Offset | Length | Field | Encoding |
|---:|---:|---|---|
| 0 | 1 | Magic | Fixed `0xD5` |
| 1 | 1 | Version | Fixed `0x01` |
| 2 | 1 | ID | Low four bits are displayed as `xxxx` |
| 3 | 1 | Flags | Bit 0 marks the position valid |
| 4 | 2 | Distance | Unsigned little-endian, unit 1 mm |
| 6 | 2 | Angle | Signed little-endian, unit 0.1 degree |
| 8 | 2 | Sequence | Unsigned little-endian, increments once per frame |
| 10 | 1 | Quality | `0` to `255` |
| 11 | 1 | CRC-8 | Application CRC over offsets 0 through 10 |

The CC1101 FIFO length byte and hardware CRC status are outside this 12-byte application payload. The receiver accepts a position only after the radio length, hardware CRC, application length, magic, version, and application CRC all pass.

## SENSING vector

- ID: `1101` (`0x0D`)
- position valid: yes
- radial distance: 2500 mm (`0x09C4`)
- angle: +10.0 degrees (`100`, `0x0064`)
- sequence: 0
- quality: 100

```text
D5 01 0D 01 C4 09 64 00 00 00 64 08
```

The last byte `08` is the independently calculated CRC-8.

## Boundary expectations

| Valid ID | Distance | Angle | Expected state |
|---|---:|---:|---|
| yes | 3051 mm | 0 deg | LOCKED / OUT OF RANGE |
| yes | 3000 mm | 0 deg | SENSING |
| yes | 2500 mm | 0 deg | SENSING |
| yes | 2000 mm | 0 deg | WELCOME |
| yes | 1500 mm | 0 deg | WELCOME |
| yes | 1000 mm | 0 deg | UNLOCKED |
| yes | 800 mm | 0 deg | UNLOCKED |
| yes | 800 mm | +50 deg | OUTSIDE / CLOSED |
| no | 800 mm | 0 deg | INVALID / CLOSED |

An application frame with `FLAGS bit0 = 0` is treated as an invalid position and immediately returns to the locked state.
