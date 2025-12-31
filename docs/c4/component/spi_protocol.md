# Protocol (SPI)

## Scope
Defines the SPI framing and command set used between `gfx_master` and `gfx_slave`.

## Framing
`[0xA5][0x5A][LEN][COBS(cmd||payload)]`

- `LEN` is the number of COBS-encoded bytes.
- No trailing 0x00 delimiter is used.
- CRC is not used.
- Responses carry payload only (no command echo).

## RC codes
- `RC_OK=0x00`, `RC_BAD_LEN=0x01`, `RC_BAD_STATE=0x02`, `RC_UNKNOWN_ID=0x03`,
  `RC_RANGE=0x04`, `RC_INTERNAL=0x05`, `RC_PARSE_FAIL=0x0B`, `RC_NO_SPACE=0x0C`.

## Status flags (GET_STATUS)
- bit0: initialized
- bit1: dirty (at least one element changed since last GET_STATUS)
- bit2: overlay visible

## Typical host sync flow
- On INT (or periodic poll), send `GET_STATUS`.
- If dirty is set, read `dirty_id` via `GET_ELEMENT_STATE`.
- `GET_STATUS` clears the dirty flag (last-change only).

## Implemented commands
| Cmd | Request payload | Response payload | Notes |
| --- | --- | --- | --- |
| `0x00 PING` | none | `[RC, version, caps_lo, caps_hi]` | caps reserved |
| `0x01 JSON` | `[flags][json_bytes...]` | `[RC]` | one JSON object per frame; flags bit0=head, bit1=commit |
| `0x03 JSON_ABORT` | none | `[RC]` | placeholder (no-op) |
| `0x10 SET_ACTIVE_SCREEN` | `[screen_ord]` | `[RC]` | base screen ordinal |
| `0x20 GET_STATUS` | none | `[RC, flags, elem_count, screen_count, active_screen, version, dirty_id, 0,0,0]` | dirty_id is the most recent changed element id |
| `0x21 SCROLL_TO_SCREEN` | `[screen_ord]` or `[off_lo, off_hi, screen_ord]` | `[RC]` | base screen ordinal |
| `0x22 GET_ELEMENT_STATE` | `[eid]` | type-specific | see below |
| `0x30 SHOW_OVERLAY` | `[screen_eid, dur_lo, dur_hi, flags]` | `[RC]` | screen element id (ov=1) |
| `0x41 INPUT_EVENT` | `[index, event]` | `[RC]` | release events only |
| `0x50 GOTO_STANDBY` | none | no response | wakes on CS falling edge |

## Reserved / not implemented
- `0x13 SET_CURSOR`, `0x14 NAVIGATE_MENU`, `0x16 SET_ANIMATION` are legacy/reserved opcodes and are not handled by the current firmware. The slave responds with `RC_BAD_LEN` if they are sent.

## GET_ELEMENT_STATE (0x22)
- TEXT: `[RC, type, text_len, text_bytes...]`
- BARREL: `[RC, type, value_lo, value_hi]`
- TRIGGER: `[RC, type, version]`
- Other: `[RC, type, 0xFF]`
