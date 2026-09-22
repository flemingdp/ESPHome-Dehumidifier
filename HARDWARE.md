# Hardware Setup

The Midea WiFi dongle is just a UART-to-cloud bridge — unplug it and connect your ESP board instead. There are a few different ways to wire this up depending on your dehumidifier model and ESP board.

## USB-A Port Pinout

All known Midea dehumidifiers use a standard USB-A port for the WiFi dongle. The port carries UART signals (not USB data) at 9600 baud.

**The D+/D- naming is the opposite of what you'd expect:**

| USB-A Pin | Signal | Direction | Connect to ESP |
|-----------|--------|-----------|----------------|
| 1 | GND | — | GND |
| 2 | D- | MCU TX | ESP RX |
| 3 | D+ | MCU RX | ESP TX |
| 4 | 5V | Power | VIN / 5V |

> **⚠ D+/D- are crossed:** Pin 2 (D-) is the MCU's **TX** line, and Pin 3 (D+) is the MCU's **RX** line. Connect MCU TX → ESP RX and MCU RX → ESP TX. If you get no response from the MCU, swap the two data wires.

---

## Choosing a Build Path

For the May 2024 **MAD50P1AWS / SK105 with V3**, use the
[S8050 wiring below](#v3-hardware-wiring---mad50p1aws--sk105).
The following direct and BSS138 examples describe other builds.

The MAD50PS1QWT-A MCU uses 5V logic (verified). Other models may differ — if you're unsure, measure the voltage on pin 2 (D-) with a multimeter before connecting. Whether you need a level shifter depends on your ESP board and how much risk you're comfortable with:

| Approach | Level shifter? | Best for |
|----------|---------------|----------|
| **Direct USB-A adapter** | No | ESP boards with 5V-tolerant GPIO, or users willing to run 3.3V pins at their own risk |
| **Level-shifted build** | Yes (BSS138) | Safe, reliable 3.3V ↔ 5V translation — recommended for new builds |

### Option A: Direct USB-A Adapter (Simplest)

Many users have had success plugging a cheap USB-A breakout adapter directly into the dehumidifier port and wiring TX/RX/GND straight to the ESP. No level shifter.

This works because:
- The MCU's TX line (5V) is read fine by most ESP32 GPIO pins (they're generally 5V-tolerant for short signals at 9600 baud)
- The ESP's TX line (3.3V) is high enough to register as a logic high on the MCU's 5V RX input

> **⚠ At your own risk:** Driving a 3.3V GPIO with a 5V signal is outside spec for ESP8266 and some ESP32 variants. It works in practice for most people, but a level shifter is the safe approach. If you experience garbled data or GPIO damage, switch to Option B.

**Parts:**

| Part | Notes |
|------|-------|
| ESP32 or ESP8266 board | Any variant with a hardware UART |
| USB-A to screw terminal breakout | Pre-made adapter, no soldering |

**Wiring:**

```
Dehumidifier                 ESP board
USB-A Port
┌────────┐
│ 5V (4) ├────────────────── VIN / 5V
│D- (2)  │  MCU TX ────────── RX
│D+ (3)  │  MCU RX ────────── TX
│GND (1) ├────────────────── GND
└────────┘
```

**ESPHome UART config:**

```yaml
uart:
  tx_pin: GPIO16  # → D+ (MCU RX)
  rx_pin: GPIO17  # ← D- (MCU TX)
  baud_rate: 9600
```

### Option B: Level-Shifted Build (Recommended)

Uses a BSS138 4-channel level shifter for proper 3.3V ↔ 5V translation. This is the safe, reliable approach — recommended for new builds.

#### Example: XIAO ESP32C6 + BSS138

A compact build for the MAD50PS1QWT-A using a Seeed Studio XIAO ESP32C6 and a BSS138 4-channel level shifter. The same approach works with any ESP32 variant — just adjust the GPIO pin numbers.

**Parts:**

| Part | Notes |
|------|-------|
| Seeed Studio XIAO ESP32C6 | USB-C, WiFi 6 + BLE 5 |
| BSS138 4-Channel Level Shifter | Bidirectional 3.3V↔5V, open-drain MOSFET |
| USB-A Male Plug (solderable) | Direct plug into dehumidifier |

**Wiring:**

```
Dehumidifier              BSS138 Level Shifter              XIAO ESP32C6
USB-A Port                HV side      LV side
┌────────┐                        ┌──────────────────────── 5V
│ 5V (4) ├─────────────── HV ─────┘     LV ──────────────── 3V3
│ D- (2) │──[MCU TX]───── B1 ◄────────► A1 ──────────────── D7 (GPIO17) RX
│ D+ (3) │──[MCU RX]───── B2 ◄────────► A2 ──────────────── D6 (GPIO16) TX
│ GND (1)├─────────────── GND ───────── GND ─────────────── GND
└────────┘
```

**ESPHome UART config for this build:**

```yaml
uart:
  tx_pin: GPIO16  # D6 on XIAO ESP32C6 → level shifter → D+ (MCU RX)
  rx_pin: GPIO17  # D7 on XIAO ESP32C6 ← level shifter ← D- (MCU TX)
  baud_rate: 9600
```

> **Note:** ESP32C6 uses different GPIO mappings than other ESP32 variants. On the XIAO ESP32C6, D6=GPIO16 and D7=GPIO17. Check your board's pinout if using a different ESP32.

---

## V3 Hardware Wiring - MAD50P1AWS / SK105

Tested with a May 2024 MAD50P1AWS / Cube 50 Pint **with pump**, factory SK105
adapter, ESP32 DevKit v1, and ESPHome 2026.9.x.

Direct ESP32 TX did not work on this unit. An S8050 open-collector TX circuit
allowed reliable communication. This result applies to this tested unit;
other models can use different interfaces.

### Wiring

Parts: ESP32 DevKit v1, S8050 NPN transistor, one 1k resistor, and wires.

```text
ESP32 DevKit v1                           Midea MAD50P1AWS

GPIO17 (UART TX) ----[ 1k ]---- B
                              |  S8050 NPN
                              C ---------------- D+ (MCU RX)
                              E
                              |
GND --------------------------+----------------- GND

GPIO16 (UART RX) -------------------------------- D- (MCU TX)
```

`B`, `C`, and `E` mean base, collector, and emitter. The diagram shows
connections, not the transistor's physical pin order.

The tested **S8050 D331** used pin 1 = emitter, pin 2 = base, pin 3 = collector.
**Check your transistor's datasheet: pin order varies by manufacturer and package.**

The tested RX path connects D- directly to GPIO16. It has no divider or level
shifter. A 10k/20k divider was tried but is not part of this working circuit.
This records the tested connection; it does not establish electrical
compatibility with other boards.

### Configuration

Use **GPIO17 for TX** and **GPIO16 for RX**. Follow the GPIO numbers even if
your board labels them `RX2` and `TX2`. These assignments differ from the
generic examples above.

```yaml
uart:
  id: uart_midea
  tx_pin:
    number: GPIO17
    inverted: true
  rx_pin:
    number: GPIO16
    inverted: false
    mode:
      input: true
      pullup: false
  baud_rate: 9600
  data_bits: 8
  parity: NONE
  stop_bits: 1
  rx_buffer_size: 512

midea_dehum:
  id: midea_dehum_comp
  uart_id: uart_midea
  protocol_version: 3
  handshake_enabled: true
  status_poll_interval: 1000
```

The S8050 pulls D+ LOW when GPIO17 is HIGH. When GPIO17 is LOW, the transistor
releases D+ and the appliance pull-up makes it HIGH. Set TX `inverted: true`
to correct this inversion. Keep RX `inverted: false`.

### Measurements and troubleshooting

Measured idle voltage was about 5.0 V without the factory adapter and 4.9 V
with it connected. Direct ESP32 TX looked valid on another UART receiver,
but the appliance did not respond. The S8050 circuit resolved that failure.
This suggests the input needs a LOW drive and a released HIGH state; the
appliance input circuit is not confirmed.

If the appliance does not respond:

1. Check GPIO17 TX, TX inversion, and the 1k base resistor.
2. Check the transistor pin order, collector to D+, and emitter to common GND.
3. Check D- to GPIO16 RX and the shared ground.
4. Do not assume bytes seen on another receiver prove the appliance can read them.

For this unit, check the circuit before following generic advice to swap data
wires. Use the configuration above; the [general example](dehumidifier.yaml)
uses different GPIO assignments and disables the handshake.

See the [V3 protocol reference](components/midea_dehum/protocol_v3.md) for frames
and startup timing.

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| No response from MCU | TX/RX swapped | Swap D+/D- connections |
| Garbage data | Wrong baud rate | Verify 9600 baud |
| ESP not powering | 5V not connected | Check USB-A pin 4 |
| Intermittent data | Missing level shifter reference voltage | Check HV (5V) and LV (3.3V) connections on the BSS138 |
| NaN temperature | Level shifter not powered | Verify both HV and LV rails are powered |
