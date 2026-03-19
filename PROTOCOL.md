# Brush-n-Snack — Serial Communication Protocol

## Architecture

```
┌──────────┐   Bluetooth    ┌──────────────────┐   Serial3 (shared bus)   ┌──────────────────┐
│  Phone   │ ◄────────────► │  Master MegaPi   │ ◄──────────────────────► │  Slave MegaPi    │
│  (App)   │                │  Drive + LEDs    │                          │  Arm + Gripper   │
└──────────┘                └──────────────────┘                          └──────────────────┘
```

All devices share **Serial3** @ 115200 baud. Messages are newline-terminated (`\n`) with prefixes.

---

## Hardware Mapping

### Master MegaPi — Drive Base + LEDs

| Slot / Port | Component   | Type          | Purpose       |
|-------------|-------------|---------------|---------------|
| SLOT1       | `motor_R2`  | Encoder motor | Right rear    |
| SLOT2       | `motor_L2`  | Encoder motor | Left rear     |
| SLOT3       | `motor_R1`  | Encoder motor | Right front   |
| SLOT4       | `motor_L1`  | Encoder motor | Left front    |
| PORT_5      | `led1`      | RGB LED strip | Left (4 LEDs) |
| PORT_6      | `led2`      | RGB LED strip | Right (4 LEDs)|

### Slave MegaPi — Robot Arm + Gripper

| Slot / Port | Component | Type                  | Purpose         |
|-------------|----------|-----------------------|-----------------|
| SLOT1       | `arm1`   | Encoder motor         | Base rotation   |
| SLOT2       | `arm2`   | Encoder motor         | Shoulder        |
| SLOT3       | `arm3`   | Encoder motor         | Elbow           |
| PORT4A      | `gripper`| DC motor (no encoder) | Gripper open/close |

---

## Message Format

```
<PREFIX>:<COMMAND>\n      ← commands
<PREFIX>><RESPONSE>\n     ← responses
```

| Prefix | Direction  | Meaning                     |
|--------|------------|-----------------------------|
| `M:`   | → Master   | Command for the Master      |
| `S:`   | → Slave    | Command for the Slave       |
| `M>`   | ← Master   | Response from Master        |
| `S>`   | ← Slave    | Response from Slave         |

Each device only processes its own prefix and ignores the rest.

---

## Command Reference

### Master Commands (`M:`)

| Command                   | Example           | Description                                |
|---------------------------|-------------------|--------------------------------------------|
| `M:F<pos>`                | `M:F1800`         | Drive forward to position (encoder degrees)|
| `M:B<pos>`                | `M:B1800`         | Drive backward to position                 |
| `M:L[pos]`                | `M:L` / `M:L180`  | Turn left (default 360)                   |
| `M:R[pos]`                | `M:R` / `M:R180`  | Turn right (default 360)                  |
| `M:x`                     | `M:x`             | Stop all drive motors                      |
| `M:C<c>,<R>,<G>,<B>`      | `M:C1,255,0,0`   | Smooth LED fade on connector `c` (1-2)     |
| `M:D<c>,<R>,<G>,<B>`      | `M:D2,0,255,0`   | Direct/instant LED set on connector `c`    |
| `M:A<s><delta>`           | `M:A1100`        | Arm motor `s` (1-3): add `delta` to current target (default speed) |
| `M:Ax`                    | `M:Ax`            | Stop all arms + gripper (forwarded)        |
| `M:Go`                    | `M:Go`            | Open gripper (forwarded)                   |
| `M:Gc`                    | `M:Gc`            | Close gripper (forwarded)                  |
| `M:Gs`                    | `M:Gs`            | Stop gripper (forwarded)                   |

> **LEDs** (`C`, `D`) are handled locally on Master.  
> **Arm** (`A`) and **Gripper** (`G`) commands are forwarded to Slave as `S:A…` / `S:G…` / `S:x`.  
> Drive uses `moveTo(position, speed)` — position in encoder degrees, default speed 300.

### Slave Commands (`S:`)

Sent by phone directly or by Master internally.

| Command                   | Example           | Description                                |
|---------------------------|-------------------|--------------------------------------------|
| `S:A<s><delta>`           | `S:A2-150`       | Arm motor `s` (1-3): add `delta` to current target (default speed) |
| `S:Go`                    | `S:Go`            | Open gripper (DC motor forward)            |
| `S:Gc`                    | `S:Gc`            | Close gripper (DC motor backward)          |
| `S:Gs`                    | `S:Gs`            | Stop gripper                               |
| `S:x`                     | `S:x`             | Stop all 3 arm motors + gripper            |

---

## Responses

| Response         | Meaning                               |
|------------------|---------------------------------------|
| `M>OK F1800`     | Master confirmed forward to pos 1800  |
| `M>OK B1800`     | Master confirmed backward to pos 1800 |
| `M>OK L`         | Master confirmed left turn            |
| `M>OK R`         | Master confirmed right turn           |
| `M>OK STOP`      | Master confirmed drive stop           |
| `M>OK C1`        | Master confirmed LED smooth fade      |
| `M>OK D2`        | Master confirmed LED direct set       |
| `M>OK A1`        | Master confirmed arm command forwarded|
| `M>OK Ax`        | Master confirmed arm-stop forwarded   |
| `M>OK Go`        | Master confirmed gripper open         |
| `M>OK Gc`        | Master confirmed gripper close        |
| `M>OK Gs`        | Master confirmed gripper stop         |
| `S>OK A2`        | Slave confirmed arm moveTo            |
| `S>OK Go`        | Slave confirmed gripper opening       |
| `S>OK Gc`        | Slave confirmed gripper closing       |
| `S>OK Gs`        | Slave confirmed gripper stopped       |
| `S>OK STOP`      | Slave confirmed all stopped           |

---

## Arm Protocol Notes

- Compact arm commands use this pattern: `A<motor><value>`.
- The first character after `A` is always the arm motor number (`1`, `2`, or `3`).
- The remaining characters are parsed as a signed integer delta (`+` optional, `-` supported).
- Example: `M:A1100` means motor `1` target position `+= 100`.
- Example: `M:A2-250` means motor `2` target position `-= 250`.

---

## Turn-Signal Behavior

Automatic during turns (Master handles LEDs locally):

| State         | Left LED (conn 1)           | Right LED (conn 2)          |
|---------------|-----------------------------|-----------------------------|
| Straight      | Solid blue                  | Solid blue                  |
| Turning left  | Flicker yellow/off (200 ms) | Solid blue                  |
| Turning right | Solid blue                  | Flicker yellow/off (200 ms) |

Uses direct LED set internally. Resets to blue when turn ends.

---

## LED Modes

- **`C` (Smooth)** — Fades gradually toward target color (~10 ms per step)
- **`D` (Direct)** — Instant color change

---

## USB Serial Debug (115200 baud)

### Master
- No prefix → Master command (e.g. `F1800`)
- `M:F1800` → Master command
- `S:A1100` → forwarded to Slave

### Slave
- No prefix → Slave command (e.g. `A1100`)
- `S:A1100` → Slave command
- `M:F1800` → forwarded to Master

`S>` responses appear on Master USB as `[Slave] …`  
`M>` responses appear on Slave USB as `[Master] …`

---

## Example Session

```text
Phone sends:    M:F1800          → Drive forward to position 1800
Master replies: M>OK F1800

Phone sends:    M:L              → Turn left (default 360)
Master replies: M>OK L
                                  (left LEDs flicker yellow)

Phone sends:    M:A1100          → Arm motor 1 target +100 (default speed)
Master replies: M>OK A1
Slave replies:  S>OK A1

Phone sends:    M:A2-150         → Arm motor 2 target -150 (default speed)
Master replies: M>OK A2
Slave replies:  S>OK A2

Phone sends:    M:Gc             → Close gripper
Master replies: M>OK Gc
Slave replies:  S>OK Gc

Phone sends:    M:Gs             → Stop gripper
Master replies: M>OK Gs
Slave replies:  S>OK Gs

Phone sends:    M:C1,255,0,0     → Left LEDs fade to red
Master replies: M>OK C1

Phone sends:    M:x              → Stop driving
Master replies: M>OK STOP

Phone sends:    S:x              → Stop all arms + gripper
Slave replies:  S>OK STOP
```

---

## Wiring

- Both MegaPi boards: **Serial3** (TX3/RX3) on shared bus
- Bluetooth module (HC-05/06) on same bus
- Baud: **115200**, newline-terminated (`\n`)
- `Serial3.setTimeout(10)` on both boards
