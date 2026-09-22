# Midea MAD50P1AWS Dehumidifier Protocol (V3)

Protocol for the May 2024 MAD50P1AWS with the SK105 adapter. Based on factory
captures and [the V3 code](midea_dehum_protocol_v3.cpp).

Use `protocol_version: 3` and `handshake_enabled: true`.
See [HARDWARE.md](../../HARDWARE.md#v3-hardware-wiring---mad50p1aws--sk105)
for wiring and UART settings. Communication is tested; minimum startup
requirements remain unknown.

## Frame Structure

All offsets below are zero-based offsets into the complete frame.

```text
AA LL TT RR 00 00 00 VV AG MT [body...] [sequence] [CRC8] CK
```

| Offset    | Meaning                                                                                                    |
| --------- | ---------------------------------------------------------------------------------------------------------- |
| 0         | Start marker `AA`                                                                                          |
| 1         | Total frame length minus one (`22` means 35 bytes)                                                         |
| 2         | Appliance type: `A1` dehumidifier, `FF` broadcast                                                          |
| 3         | Routing/synchronization field; meaning partially understood                                                |
| 4–7       | Header fields; captured transactions use zeroes                                                            |
| 8         | Agreement version: startup advertisements `08`, control/poll requests `00`, captured status responses `03` |
| 9         | Message type                                                                                               |
| 10 onward | Message-specific body                                                                                      |
| Last      | Additive checksum                                                                                          |

Control and poll frames include a sequence byte and CRC8. Startup frames are
sent as captured. V3 is the component's protocol selection, not a fixed value
for byte 8.

Factory requests use byte 7 `00`. The ESP fills byte 7 from the learned MCU
header version, so requests can use `03` after the ACK. The exact meaning of
bytes 7 and 8 across models is not fully known.

## Startup and Handshake

Fixed V3 sends its announce during setup, after UART initialization.
Auto-detection starts after 100 ms and tries V1, V2, then V3 in one-second
rounds. The table shows current software timing, not required device timing.

| Direction | Frame/action                                       | Timing or handling                                     |
| --------- | -------------------------------------------------- | ------------------------------------------------------ |
| ESP → MCU | Short `07` announce, 12 bytes                      | Handshake step 0; advance to step 1                    |
| MCU → ESP | Long `07` ACK, captured as 43 bytes                | Record appliance type and agreement; advance to step 2 |
| ESP → MCU | Six `A0` acquisition advertisements, 31 bytes each | 50, 383, 712, 1046, 1388, 1708 ms after short announce |
| ESP → MCU | Initial `0D` network advertisement, 31 bytes       | 2056 ms after short announce                           |
| ESP → MCU | Two connected/IP `0D` advertisements               | 10743 and 12719 ms after long ACK                      |
| MCU → ESP | `05/A0` status, 35 bytes                           | Shared receive path parses initial state               |

The six `A0` frames and first `0D` do not wait for the ACK. A missing ACK
causes a warning at 1500 ms; startup continues. Only the two later IP frames
need the ACK. A late ACK does not repeat the burst.

A status notification completes the handshake. A command response updates
state but does not complete the handshake.

### Announce and ACK

Short announce:

```text
AA 0B FF F4 00 00 01 00 08 07 00 F2
```

Captured long ACK (ellipsis abbreviates the `FF` payload):

```text
AA 2A A1 00 00 00 00 00 03 07 FF FF ... FF 4B
```

### Acquisition Advertisement

Six identical copies:

```text
AA 1E A1 BF 00 00 00 00 08 A0
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 DA
```

### Network Advertisements

Initial Wi-Fi/no-IP frame:

```text
AA 1E A1 BF 00 00 00 00 08 0D
01 01 00 00 00 00 00 FF 02 01 00 00 00 00 00 00 02 00 00 00 67
```

Connected/IP frames, in order:

```text
AA 1E A1 BF 00 00 00 00 08 0D
01 01 04 BA 08 A8 C0 FF 00 01 00 00 00 00 00 00 02 00 00 00 3B

AA 1E A1 BF 00 00 00 00 08 0D
01 01 04 BA 08 A8 C0 FF 00 00 00 00 01 00 00 00 03 00 00 00 3A
```

The IP bytes represent `192.168.8.186` in reverse order. They are fixed capture
values. Whether the appliance checks the IP is unknown. V3 uses the V1 message
handler for other handshake/network messages (`A0`, `05`, `63`).

Network fields below are inferred from captures:

| Payload offset | Frame offset | Observed meaning                                         |
| -------------: | -----------: | -------------------------------------------------------- |
|              0 |           10 | Module type (`01`)                                       |
|              1 |           11 | Wi-Fi mode/state (`01`)                                  |
|              2 |           12 | IP present/length (`00` or `04`)                         |
|            3–6 |        13–16 | IPv4 bytes, observed in little-endian order              |
|              7 |           17 | Signal indicator (`FF`)                                  |
|            8–9 |        18–19 | Router/cloud state; values change as connection advances |
|          10–15 |        20–25 | Unknown flags/reserved fields                            |
|             16 |           26 | Sequence/state value (`02`, then `03`)                   |
|          17–19 |        27–29 | Reserved/unknown                                         |

## Steady-State Communication

| Direction | Message type/subtype | Total bytes | Meaning                         |
| --------- | -------------------- | ----------- | ------------------------------- |
| ESP → MCU | `03/41`              | 33          | Status poll                     |
| MCU → ESP | `03/C8`              | 35          | Poll response                   |
| ESP → MCU | `02/48`              | 34          | Control request                 |
| MCU → ESP | `02/C8`              | 35          | Control response                |
| MCU → ESP | `05/A0`              | 35          | Unsolicited status notification |

### Status Query

The 20-byte body is `41 81 00 FF` followed by sixteen zero bytes:

```text
AA 20 A1 00 00 00 00 00 00 03
41 81 00 FF 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
SS CC CK
```

Sequence is at byte 30, CRC8 at 31, checksum at 32. Polling uses the configured
`status_poll_interval`; the tested configuration uses 1000 ms.

### Status Response Layout

`05/A0`, `02/C8`, and `03/C8` share the 35-byte state layout. The V3 message
handler accepts command/poll responses with agreement `03`; the unsolicited
status predicate matches length 35, prefix `AA 22 A1`, and subtype `05/A0`.

| Offset | Field              | Current decoding                                                                    |
| ------ | ------------------ | ----------------------------------------------------------------------------------- |
| 11     | Power              | Bit 0                                                                               |
| 12     | Mode               | Low nibble: `01` Normal, `02` Continuous, `03` Smart                                |
| 13     | Fan                | Low seven bits: `28` Low, `50` High                                                 |
| 14–16  | Timers             | Raw ON, OFF, extension bytes preserved when timer support is enabled                |
| 17     | Target humidity    | Direct percentage                                                                   |
| 19     | Feature/pump flags | Preserve as status; do not copy wholesale into commands                             |
| 26     | Current humidity   | Direct percentage                                                                   |
| 27–28  | Temperature        | Shared decoder: `(byte27 - 50) / 2`, then decimal adjustment from byte28 low nibble |
| 31     | Error              | Shared decoder's raw error code                                                     |
| 32     | Transaction ID     | Echoed request sequence for command/poll responses                                  |
| 33     | CRC8               | Payload CRC                                                                         |
| 34     | Checksum           | Frame checksum                                                                      |

Temperature decoding clamps the base value below −19 °C to −20 °C and above
50 °C to 50 °C, then adds the decimal nibble × 0.1 for nonnegative values or
subtracts it for negative values. Shared decoding does not independently verify
all V2 feature/error meanings on this model.

The factory adapter and this component echo each `05/A0` frame. Captures show
the command ID in the response and later notification. Whether the echo is
required is unknown.

## Control Commands

A control has a 21-byte body, then sequence, CRC8, and checksum:

```text
AA 21 A1 00 00 00 00 00 00 02
48 PP MM FF ON OF EX HH 00 PM 00 00 00 00 00 00 00 00 00 00 00
SS CC CK
```

| Frame offset | Body offset | Encoding                                                                 |
| ------------ | ----------- | ------------------------------------------------------------------------ |
| 10           | 0           | Write marker `48`                                                        |
| 11           | 1           | `43` ON, `42` OFF (factory beep bit retained)                            |
| 12           | 2           | `01` Normal, `02` Continuous, `03` Smart; other modes reject the command |
| 13           | 3           | Status `28` → request `A8` Low; status `50` → request `D0` High          |
| 14–16        | 4–6         | Last received raw timer bytes, or `7F 7F 00` without timer support       |
| 17           | 7           | Direct target percentage, e.g. `37` = 55%, `3C` = 60%                    |
| 19           | 9           | `08` only for an explicit pending pump-ON request; otherwise `00`        |
| 18, 20–30    | 8, 10–20    | Zero                                                                     |
| 31           | —           | Sequence                                                                 |
| 32–33        | —           | CRC8, checksum                                                           |

Controls preserve power, mode, fan, and target humidity unless changed by the
caller. Unsupported modes block the command. An unknown fan value uses Low
(`A8`) without changing the stored state; this is a startup fallback.

With timer support, commands copy the last raw timer bytes (initially
`00 00 00`). Without it, they use `7F 7F 00`. New timer settings are not encoded.

Do not copy status flags `10` or `18` into commands. Byte 25 has appeared as
`00` and `01` in captures; its meaning is unknown, so the encoder sends zero.

Captured power-ON request (Normal, Low, 55%):

```text
AA 21 A1 00 00 00 00 00 00 02
48 43 01 A8 7F 7F 00 37 00 00 00 00 00 00 00 00 00 00 00 00 00
09 6A 60
```

### Captured Control Responses and Variants

The response to the power-ON example above carries the same sequence `09`:

```text
AA 22 A1 00 00 00 00 00 03 02
C8 01 01 28 7F 7F 00 37 00 10 00 00 00 00 00 00 40 56 00 00 00 00
09 3A 28
```

Additional factory transactions confirm power and target humidity encoding:

| Action/state                    | Byte 11 | Byte 17 | Sequence | CRC8 | Checksum |
| ------------------------------- | ------- | ------- | -------- | ---- | -------- |
| Powered ON, target 55%          | `43`    | `37`    | `5F`     | `6C` | `08`     |
| Powered ON, target 60%          | `43`    | `3C`    | `0C`     | `A8` | `1A`     |
| Powered OFF, target remains 55% | `42`    | `37`    | `09`     | `80` | `4B`     |

## Transaction IDs and Checksums

Controls and polls share an 8-bit counter that wraps from `FF` to `00`.
Responses echo the request ID. The factory ID algorithm is unknown; consecutive
captured poll pairs used `26` and `3C`.

The component tracks one pending command. A matching `02/C8` response clears
it. An unmatched response still updates state.

CRC8 is calculated over the body including the appended sequence, using the
shared UART implementation. The final checksum is the two's complement of the
sum of all bytes after `AA`, excluding the checksum itself:

```python
def checksum(frame_without_checksum):
    return (-sum(frame_without_checksum[1:])) & 0xFF
```

## Capture Evidence and Startup Investigation

Two receivers recorded the factory adapter: LINE A is adapter → appliance;
LINE B is appliance → adapter. Times below start at the first announce.

|    Relative time | Frame                                     |
| ---------------: | ----------------------------------------- |
|             0 ms | Short `AA 0B ... 08 07` announce          |
|           +34 ms | Long `AA 2A ... 03 07` acknowledgement    |
|           +84 ms | First `AA 1E ... 08 A0` acquisition frame |
| +417 to +1758 ms | Five additional `A0` frames               |
|         +2090 ms | `0D` Wi-Fi/no-IP frame                    |
|        +10777 ms | First `0D` connected/IP frame             |
|        +12753 ms | Second `0D` connected/IP frame            |
|        +20829 ms | First `05/A0` status frame                |
|        +20988 ms | Same status observed on the other line    |

The six `A0` frames were about 330 ms apart, with no inbound `A0` request
between them. Byte 9 `A0` is a network message; byte 10 `A0` is a status subtype.

The factory adapter also started successfully when attached after appliance
boot. The reverse trace was corrupt, so it cannot show whether an ACK arrived.
This test does not prove the ACK is optional.

## Verification and Remaining Questions

Run `make test` in `components/midea_dehum/tests`. Tests cover startup, captured
controls, mode/fan values, pump flags, sequence matching, polling, and detection.

Still unknown:

- Minimum startup messages and delays, and the event that enables commands.
- Which network fields the appliance checks and whether status echo is required.
- Timer writes, other feature fields, and the factory sequence algorithm.
- Full meaning of header bytes 7 and 8.
- Short frames such as `AA 04 48 FF 40`.

Neither `63` nor V2's `E1` query appeared in the clean cold-boot capture.
For new write mappings, capture both directions and check the response ID and
state after each action.
