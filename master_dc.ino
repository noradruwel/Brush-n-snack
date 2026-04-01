#include <MeMegaPi.h>

/*
Standalone master (DC-style drive):
- No encoder position targets.
- Direct DC motor control via run()/stop().
- Commands: F, B, L, R, x, optional V<speed>
- Accepts commands from Serial3 (BT) and Serial (USB).
*/

// ===== Hardware =====
// Adjust these ports to match your motor wiring on MegaPi.
MeMegaPiDCMotor motor_L1(PORT2B);  // Left front
MeMegaPiDCMotor motor_L2(PORT1B);  // Left rear
MeMegaPiDCMotor motor_R1(PORT4B);  // Right front
MeMegaPiDCMotor motor_R2(PORT3B);  // Right rear

// ===== Constants =====
const long SERIAL_BAUD = 115200;
const int DEFAULT_DRIVE_SPEED = 220;
const int MIN_DRIVE_SPEED = 80;
const int MAX_DRIVE_SPEED = 255;

// ===== State =====
int driveSpeed = DEFAULT_DRIVE_SPEED;

// ===== Utility =====
long parseLongSuffix(const String &s, int start, long defaultValue = 0) {
  if (s.length() <= start) return defaultValue;
  return atol(s.substring(start).c_str());
}

void replyMaster(const String &msg) {
  Serial3.println(msg);
}

void setDriveRun(int leftSpeed, int rightSpeed) {
  motor_L1.run(leftSpeed);
  motor_L2.run(leftSpeed);
  motor_R1.run(rightSpeed);
  motor_R2.run(rightSpeed);
}

void driveForward() {
  // Right side is mirrored in this build.
  setDriveRun(driveSpeed, -driveSpeed);
}

void driveBackward() {
  setDriveRun(-driveSpeed, driveSpeed);
}

void turnLeft() {
  setDriveRun(driveSpeed, driveSpeed);
}

void turnRight() {
  setDriveRun(-driveSpeed, -driveSpeed);
}

void stopDrive() {
  motor_L1.stop();
  motor_L2.stop();
  motor_R1.stop();
  motor_R2.stop();
}

// ===== Command processing =====
// Supported:
// F[...], B[...], L[...], R[...]  (suffix ignored so F360 still works)
// x (stop)
// V<speed> (set speed, 80..255)
// Optional prefix accepted: M:
void processCommand(const String &rawInput) {
  String in = rawInput;
  if (in.startsWith("M:")) in = in.substring(2);
  in.trim();
  if (in.length() == 0) return;

  char cmd = in.charAt(0);

  if (cmd == 'F') {
    driveForward();
    replyMaster("M>OK F");
    return;
  }

  if (cmd == 'B') {
    driveBackward();
    replyMaster("M>OK B");
    return;
  }

  if (cmd == 'L') {
    turnLeft();
    replyMaster("M>OK L");
    return;
  }

  if (cmd == 'R') {
    turnRight();
    replyMaster("M>OK R");
    return;
  }

  if (cmd == 'x') {
    stopDrive();
    replyMaster("M>OK STOP");
    return;
  }

  if (cmd == 'V') {
    long requested = parseLongSuffix(in, 1, driveSpeed);
    if (requested < MIN_DRIVE_SPEED) requested = MIN_DRIVE_SPEED;
    if (requested > MAX_DRIVE_SPEED) requested = MAX_DRIVE_SPEED;
    driveSpeed = (int)requested;
    replyMaster("M>OK V" + String(driveSpeed));
    return;
  }
}

void pollCommandStream(Stream &stream) {
  if (!stream.available()) return;
  String line = stream.readStringUntil('\n');
  line.trim();
  processCommand(line);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial3.begin(SERIAL_BAUD);
  Serial3.setTimeout(10);

  stopDrive();
  Serial.println("Standalone Master DC ready");
}

void loop() {
  pollCommandStream(Serial3);
  pollCommandStream(Serial);
}
                                                                                                                                                                      