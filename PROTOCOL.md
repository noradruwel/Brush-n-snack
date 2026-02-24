# Brush-n-Snack — Serial Communication Protocol

## Architecture Overview

```
┌──────────┐   Bluetooth    ┌──────────────────┐   Serial3 (shared bus)   ┌──────────────────┐
│  Phone   │ ◄────────────► │  Master MegaPi   │ ◄──────────────────────► │  Slave MegaPi    │
│  (App)   │                │  (Drive base)    │                          │  (Arm + LEDs)    │
└──────────┘                └──────────────────┘                          └──────────────────┘
```

All three devices share **one serial connection** (`Serial3` @ 115200 baud).  
Messages are newline-terminated (`\n`) and use **prefixes** so each device knows what to process and what to ignore.

---

## Hardware Mapping

### Master MegaPi — Drive Base

| Slot  | Motor       | Purpose       |
|-------|-------------|---------------|
| SLOT1 | `motor_R2`  | Right rear    |
| SLOT2 | `motor_L2`  | Left rear     |
| SLOT3 | `motor_R1`  | Right front   |
| SLOT4 | `motor_L1`  | Left front    |

### Slave MegaPi — Robot Arm + LEDs

| Slot / Port | Component    | Purpose              |
|-------------|--------------|----------------------|
| SLOT1       | `armMotor1`  | Arm joint 1 (base)   |
| SLOT2       | `armMotor2`  | Arm joint 2 (shoulder) |
| SLOT3       | `armMotor3`  | Arm joint 3 (elbow)  |
| SLOT4       | `armMotor4`  | Arm joint 4 (gripper)|
| PORT_5      | `led1`       | LED strip connector 1 (4 LEDs) |
| PORT_6      | `led2`       | LED strip connector 2 (4 LEDs) |

---

## Message Format

```
<PREFIX>:<COMMAND>\n      ← commands (input)
<PREFIX>><RESPONSE>\n     ← responses (output)
```

### Prefixes

| Prefix | Direction          | Meaning                              |
|--------|--------------------|--------------------------------------|
| `M:`   | → Master           | Command destined for the Master      |
| `S:`   | → Slave            | Command destined for the Slave       |
| `M>`   | ← Master           | Response / acknowledgment from Master|
| `S>`   | ← Slave            | Response / acknowledgment from Slave |

Each device **only processes** messages with its own prefix and **ignores** the rest.

---

## Command Reference

### Master Commands (prefix `M:`)

| Command               | Example        | Description                                    |
|-----------------------|----------------|------------------------------------------------|
| `M:F<meters>`         | `M:F1.5`       | Drive forward 1.5 meters                       |
| `M:B<meters>`         | `M:B0.5`       | Drive backward 0.5 meters                      |
| `M:L[degrees]`        | `M:L` / `M:L45`| Turn left (default 90°, or specify degrees)   |
| `M:R[degrees]`        | `M:R` / `M:R30`| Turn right (default 90°, or specify degrees)  |
| `M:x`                 | `M:x`          | Immediate stop (all drive motors)              |
| `M:C<c>,<R>,<G>,<B>`  | `M:C1,255,0,0` | Set LED connector `c` to RGB (smooth, forwarded to Slave) |
| `M:A<s>,<speed>`      | `M:A2,150`     | Set arm motor slot `s` to `speed` (forwarded to Slave)    |
| `M:Ax`                | `M:Ax`         | Stop all arm motors (forwarded to Slave)       |

> The `C` and `A` commands are **convenience shortcuts**: the Master automatically relays them to the Slave as `S:L…` / `S:A…` / `S:x` messages.

### Slave Commands (prefix `S:`)

These can be sent by the **phone directly** or by the **Master** internally.

| Command               | Example          | Description                                  |
|-----------------------|------------------|----------------------------------------------|
| `S:L<c>,<R>,<G>,<B>`  | `S:L1,0,255,0`  | Smooth LED transition on connector `c`       |
| `S:D<c>,<R>,<G>,<B>`  | `S:D2,255,255,0`| Direct/immediate LED set on connector `c`    |
| `S:A<s>,<speed>`      | `S:A3,-100`     | Set arm motor slot `s` (1-4) to `speed` (-200…200) |
| `S:x`                 | `S:x`           | Stop all 4 arm motors immediately            |

#### LED Modes

- **`L` (Smooth)** — Color fades gradually toward the target (~10 ms per step). Good for ambiance.
- **`D` (Direct)** — Color changes instantly. Used internally by the Master for turn-signal blinking.

---

## Response Format

Responses are sent back on Serial3 so the phone (or other device) can confirm actions.

| Response          | Meaning                                |
|-------------------|----------------------------------------|
| `M>OK F1.5`       | Master confirmed forward 1.5 m         |
| `M>OK B0.5`       | Master confirmed backward 0.5 m        |
| `M>OK L`          | Master confirmed left turn             |
| `M>OK R`          | Master confirmed right turn            |
| `M>OK STOP`       | Master confirmed drive stop            |
| `M>OK C1`         | Master confirmed LED command forwarded |
| `M>OK A2`         | Master confirmed arm command forwarded |
| `M>OK Ax`         | Master confirmed arm-stop forwarded    |
| `S>OK L1`         | Slave confirmed LED smooth transition  |
| `S>OK A3`         | Slave confirmed arm motor set          |
| `S>OK STOP`       | Slave confirmed all arms stopped       |

---

## Turn-Signal Behavior

The Master automatically controls the LEDs during turns:

| State         | Connector 1 (Left) | Connector 2 (Right) |
|---------------|---------------------|----------------------|
| Straight      | Solid blue          | Solid blue           |
| Turning left  | Flicker yellow/off (200ms) | Solid blue    |
| Turning right | Solid blue          | Flicker yellow/off (200ms) |

Turn signals use **direct LED set** (`D` command) for instant response and automatically reset to blue when the turn ends.

---

## USB Serial Debug

Both boards also accept commands via **USB Serial** (`Serial` @ 115200 baud) for debugging:

### Master USB Serial
- Type a command **without prefix** → treated as a Master command (e.g., `F1.5`)
- Type `M:F1.5` → processed by Master
- Type `S:A1,200` → forwarded to Slave over Serial3

### Slave USB Serial
- Type a command **without prefix** → treated as a Slave command (e.g., `A1,200`)
- Type `S:L1,255,0,0` → processed by Slave
- Type `M:F1.5` → forwarded to Master over Serial3

Slave responses (`S>`) are printed on Master USB Serial as `[Slave] …` for debugging.  
Master responses (`M>`) are printed on Slave USB Serial as `[Master] …` for debugging.

---

## Example Session

```text
Phone sends:    M:F2          → Master drives forward 2 meters
Master replies: M>OK F2.00

Phone sends:    M:L           → Master turns left 90°
Master replies: M>OK L
                               (LEDs start flickering yellow on left side)

Phone sends:    S:A1,150      → Slave arm motor 1 at speed 150
Slave replies:  S>OK A1

Phone sends:    M:A3,-100     → Master relays arm motor 3 at speed -100
Master replies: M>OK A3
Slave replies:  S>OK A3

Phone sends:    S:L2,255,0,0  → Slave LED connector 2 fades to red
Slave replies:  S>OK L2

Phone sends:    M:x           → Master stops all drive motors
Master replies: M>OK STOP

Phone sends:    S:x           → Slave stops all arm motors
Slave replies:  S>OK STOP
```

---

## Wiring Notes

- Both MegaPi boards connect their **Serial3** (TX3/RX3) to the shared serial bus
- The **Bluetooth module** (e.g., HC-05/06) is also on the same bus
- Baud rate: **115200** on all devices
- All messages are **newline-terminated** (`\n`)
- `Serial3.setTimeout(10)` is set on both boards to prevent blocking reads
