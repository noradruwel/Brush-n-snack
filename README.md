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
