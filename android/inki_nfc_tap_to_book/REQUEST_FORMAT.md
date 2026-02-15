# Booking request format (draft v1)

Goal: phone writes a tiny request into ST25DV user EEPROM over RF (ISO15693) while the tag is unpowered; inki later reads it over I²C at boot.

## Location
- App tries `Get System Info (0x2B)` to learn **block count** and **bytes-per-block**.
- It writes to the **last N blocks** required for the payload (payload is 16 bytes).
- If system info is unavailable, it falls back to assuming 128 blocks × 4 bytes and writes to blocks **124–127**.

## Payload (16 bytes)
All multibyte fields are **little-endian**.

| Offset | Size | Name | Example | Notes |
|---:|---:|---|---|---|
| 0 | 4 | magic | `INKI` | ASCII `0x49 0x4E 0x4B 0x49` |
| 4 | 1 | version | `0x01` | bump on incompatible changes |
| 5 | 1 | opcode | `0x11` | command selector (see below) |
| 6 | 2 | duration_minutes | `0x003C` | default 60 |
| 8 | 4 | unix_seconds | `0x...` | time of write |
| 12 | 4 | nonce | random | prevents accidental replays |

### Opcode mapping (current harness test flow)
- `0x11`: LED1 slow blink command
- `0x12`: LED2 fast blink command
- `0x01`: legacy alias accepted by firmware (mapped to LED1 slow)

## Clearing / ack
- Simplest: inki overwrites the same blocks with `0x00` after processing.
