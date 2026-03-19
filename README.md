# Brush-n-Snack

Brush-n-Snack uses two MegaPi boards:

- Master board: drivetrain + two 4-LED light rings
- Slave board: arm joints + gripper

This README is now the single source of truth for setup and commands.

## Repo layout

- [master.ino](master.ino): current standalone Master firmware
- [slave.ino](slave.ino): current standalone Slave firmware
- [dev](dev): older test sketches and Python tooling for bridge/testing workflows

## How the current firmware works

The current root sketches are standalone:

- [master.ino](master.ino) controls only driving and LEDs.
- [slave.ino](slave.ino) controls only arm and gripper.
- Both accept line-based text commands over Serial3 (Bluetooth) and Serial (USB).

Each command is one line ending with a newline.

## Quick start

1. Flash [master.ino](master.ino) to the Master MegaPi.
2. Flash [slave.ino](slave.ino) to the Slave MegaPi.
3. Open Serial Monitor (115200 baud) on each board.
4. Send newline-terminated commands from Bluetooth app, USB serial tool, or script.

## Serial settings

- Baud rate: 115200
- Command format: one command per line
- Terminator: newline

## Master command reference

You can send commands as plain form (example: F360) or prefixed form (example: M:F360).

Drive:

- F<steps>: move forward
- B<steps>: move backward
- L or L<steps>: turn left (default 360 if no value)
- R or R<steps>: turn right (default 360 if no value)
- x: stop drive motors

LED and turn-signal behavior:

- N<R>,<G>,<B>: set default cruise color
- T<R>,<G>,<B>: set turn signal color
- Q0 or Q1: rainbow mode off/on (fixed fast per-pixel rainbow effect)

Typical responses from Master on Serial3:

- M>OK F360
- M>OK STOP
- M>OK N0,0,255

## Slave command reference

You can send commands as plain form (example: A1100) or prefixed form (example: S:A1100).

Arm:

- A<slot><delta>: move arm motor target by delta
- Slots: 1 (base), 2 (shoulder), 3 (elbow)
- Examples: A1100, A2-150

Gripper:

- Go: open gripper
- Gc: close gripper
- Gs: stop gripper
- x: stop all arm motors and gripper

Typical responses from Slave on Serial3:

- S>OK A1
- S>OK Go
- S>OK STOP

## Wiring and communication notes

- Keep both boards and Bluetooth links at the same baud rate.
- Treat serial as a stream, not packets. Newline delimiters matter.
- If command parsing seems flaky, first verify line endings in your sender.

## Safety behavior

- Slave gripper has an automatic runtime timeout (currently 5 seconds) to prevent overrun.
- Use x as emergency stop per board.

## Developer tools in dev

The [dev](dev) folder is for experiments and convenience tools, not the main runtime firmware.

- [dev/web_controller.py](dev/web_controller.py): browser UI that sends serial commands
- [dev/bluetooth_arduino_cli.py](dev/bluetooth_arduino_cli.py): RFCOMM terminal CLI
- test sketches for Bluetooth and ultrasonic bring-up

Important: some dev sketches/scripts target older command variants. If behavior differs, trust [master.ino](master.ino) and [slave.ino](slave.ino) as the current protocol.

## Common troubleshooting

No response:

1. Confirm baud is 115200 everywhere.
2. Confirm newline is sent with each command.
3. Confirm you are sending Master commands to Master and Slave commands to Slave.

Movement/LED mismatch:

1. Verify you are using the current root sketches.
2. Power-cycle board after flashing.
3. Test with a minimal command set first: F360, x, N0,0,255, A1100, Gs.

## License

See [LICENSE](LICENSE).
