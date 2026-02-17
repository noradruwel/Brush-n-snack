# Brush-n-snack

Arduino/MegaPi project with a **Master** and **Slave** board.

- `Master/` contains driving logic (wheels) and command parsing.
- `Slave/` contains arm motor and LED control.
- `dev/` contains Bluetooth bridge test sketches only.

## Bridge-only approach

This repository uses the Android app **Bluetooth Bridge** exclusively for test traffic:

- https://sites.google.com/view/communication-utilities/bridge-user-guide

Older test documentation outside the bridge flow was intentionally removed.

## `dev/` folder contents

Only these files are kept in `dev/`:

- `Master_BT_Test.ino`
- `Slave_BT_Test.ino`
- `Master_BT_Motor_Ultrasonic_Test.ino`
- `Slave_BT_Motor_Ultrasonic_Test.ino`

## Setup: Master ↔ Bridge ↔ Slave

1. Use the onboard Bluetooth interface on both MegaPi boards (no external Bluetooth wiring required).
2. Upload:
	- `dev/Master_BT_Test.ino` to the Master board
	- `dev/Slave_BT_Test.ino` to the Slave board
3. Open Serial Monitor on both boards at `115200`.
4. Open Bluetooth Bridge on Android and create two connections:
	- Device A = Master module
	- Device B = Slave module
5. Enable retransmission between A and B.
6. Verify status **Bridge active**.
7. From Master, send: `HELLO`, `TEST`, `ARM100`, `STOP`.

If everything works, Master will receive `[RECEIVED] ...` responses from Slave.

## Extended `dev/` test (arm open/close + ultrasonic starter)

Use the new files when you want Bluetooth command tests that include arm open/close control and a starting point for ultrasonic sensors:

- Upload `dev/Master_BT_Motor_Ultrasonic_Test.ino` to Master
- Upload `dev/Slave_BT_Motor_Ultrasonic_Test.ino` to Slave

Commands supported in this extended flow:

- `O` (open arm)
- `C` (close arm)
- `X` (stop arm)
- `A120` / `A-120` (manual PWM override)
- `D` (single distance read)

`Slave_BT_Motor_Ultrasonic_Test.ino` uses the MeMegaPi arm motor interface (`MeEncoderOnBoard`) and Makeblock ultrasonic library interface (`MeUltrasonicSensor`) like the official examples.

In this dev test, commands are plain text lines over `Serial3` to keep debugging simple.

### Extended flow documentation (complete)

Files:

- `dev/Master_BT_Motor_Ultrasonic_Test.ino`
- `dev/Slave_BT_Motor_Ultrasonic_Test.ino`

Serial settings:

- Master Serial Monitor: `115200`
- Slave Serial Monitor: `115200`
- Bluetooth (`Serial3`) on both boards: `115200`

Message format:

- One command per line (`\n` terminated), uppercase recommended.
- Slave returns one response line per command.

Command table:

- `H` → response: `HELLO received`
- `T` → response: `Test received - BT OK`
- `O` → action: arm open at default PWM, response: `OK OPEN`
- `C` → action: arm close at default PWM, response: `OK CLOSE`
- `X` → action: arm stop, response: `OK STOP`
- `A<number>` → action: set arm PWM (`-255..255`), response: `OK ARM <applied>`
- `D` → response: `DIST <cm>`

Examples:

- `A120` → `OK ARM 120`
- `A-80` → `OK ARM -80`
- `D` → `DIST 37`

Motor control notes:

- The extended dev sketch uses `setTarPWM(...)` + `armMotor.loop()` for simple open/close testing.
- `setPulse`, `setRatio`, and PID tuning are commonly used in speed/position control examples, but are intentionally omitted here to keep this PWM test minimal and easy to debug.

## Bridge settings (important)

- Set retransmission to **bidirectional** (A↔B).
- Use text mode on the Traffic page for quick diagnostics.
- Check line-break behavior in log settings (`new data` vs `CR/LF`) during command debugging.
- If the app runs in the background, exclude it from battery optimization to avoid disconnects.

## Stability test

1. Send `TEST` 20 times with a short pause between messages.
2. Verify every request has a response.
3. Verify both directions (Master→Slave and Slave→Master).
4. Restart only the app and confirm reconnect + bridge active.

## Important note from the Bridge guide

Serial data is a stream, not a guaranteed packet boundary. At higher speeds, messages may be split or merged. Keep commands simple and newline-terminated.

## Reference

Full guide:

- https://sites.google.com/view/communication-utilities/bridge-user-guide
