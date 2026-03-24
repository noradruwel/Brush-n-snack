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

| Command | Example | What it does | Typical response |
|---|---|---|---|
| `F<steps>` | `F360` or `M:F360` | Drive forward by encoder steps | `M>OK F360` |
| `B<steps>` | `B360` or `M:B360` | Drive backward by encoder steps | `M>OK B360` |
| `L` / `L<steps>` | `L` or `M:L180` | Turn left (default 360 if omitted) | `M>OK L` |
| `R` / `R<steps>` | `R` or `M:R180` | Turn right (default 360 if omitted) | `M>OK R` |
| `x` | `x` or `M:x` | Stop drive motors immediately | `M>OK STOP` |
| `N<R>,<G>,<B>` | `N0,0,255` | Set default cruise color for both light rings (also exits rainbow mode) | `M>OK N0,0,255` |
| `T<R>,<G>,<B>` | `T255,170,0` | Set turn-signal color | `M>OK T255,170,0` |
| `Q0` / `Q1` | `Q1` | Rainbow mode off/on (fixed fast solid color across both rings) | `M>OK Q1` |

## Slave command reference

You can send commands as plain form (example: A1100) or prefixed form (example: S:A1100).

| Command | Example | What it does | Typical response |
|---|---|---|---|
| `A<slot><delta>` | `A1100`, `S:A2-150` | Move arm target by delta on selected slot | `S>OK A1`, `S>OK A2` |
| `Go` | `Go` or `S:Go` | Open gripper | `S>OK Go` |
| `Gc` | `Gc` or `S:Gc` | Close gripper | `S>OK Gc` |
| `Gs` | `Gs` or `S:Gs` | Stop gripper motor | `S>OK Gs` |
| `x` | `x` or `S:x` | Stop all arm motors and gripper | `S>OK STOP` |

Arm slot mapping:

| Slot | Motor |
|---|---|
| `1` | Wrist motor |
| `2` | Elbow motor |
| `3` | Axis rotation motor |

This mapping reflects the current physical build wiring.

## Wiring and communication notes

- Keep both boards and Bluetooth links at the same baud rate.
- Treat serial as a stream, not packets. Newline delimiters matter.
- If command parsing seems flaky, first verify line endings in your sender.
- Master ultrasonic sensors are on PORT_6 and PORT_7.

## Safety behavior

- Slave gripper has an automatic runtime timeout (currently 5 seconds) to prevent overrun.
- Master automatically stops drive motors if ultrasonic distance on PORT_6 or PORT_7 is between 1 and 15 cm.
- Ultrasonic check interval is 100 ms (module minimum measurement interval).
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
