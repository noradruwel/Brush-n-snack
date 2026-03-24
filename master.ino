#include <MeMegaPi.h>

/*
Standalone master sketch:
- Controls drive + LEDs only.
- Accepts commands directly from Serial3 (BT) and Serial (USB).
*/

// ===== Hardware =====
MeEncoderOnBoard motor_L1(SLOT2);  // Left front
MeEncoderOnBoard motor_L2(SLOT1);  // Left rear
MeEncoderOnBoard motor_R1(SLOT4);  // Right front
MeEncoderOnBoard motor_R2(SLOT3);  // Right rear
MeRGBLed led1(PORT_5);             // Left light ring (4 LEDs)
MeRGBLed led2(PORT_8);             // Right light ring (4 LEDs)
MeUltrasonicSensor ultraSensorLeft(PORT_6);
MeUltrasonicSensor ultraSensorRight(PORT_7);

// ===== Constants =====
const long SERIAL_BAUD = 115200;
const int DRIVE_SPEED = 300;
const int NUM_LEDS = 4;

const int TURN_STEP = 10;
const int TURN_MIN_BRIGHTNESS = 30;
const unsigned long TURN_FADE_MS = 10;
const int ULTRASONIC_STOP_CM = 15;
const unsigned long ULTRASONIC_POLL_MS = 100;

const unsigned long LED_RENDER_MS = 10;
const unsigned long RAINBOW_STEP_MS = 8;  // fixed fast NeoPixel-style speed

// ===== Motion state =====
long posL = 0;
long posR = 0;

bool turningLeft = false;
bool turningRight = false;

// ===== LED state =====
byte defaultColor[3] = {0, 0, 255};
byte turnColor[3] = {255, 170, 0};

bool rainbowEnabled = false;
byte rainbowHue = 0;
unsigned long lastRainbowStepMs = 0;

int turnBrightness = 0;
int turnDir = 1;
unsigned long lastTurnFadeMs = 0;

unsigned long lastRenderMs = 0;
unsigned long lastUltrasonicPollMs = 0;
bool obstacleStopActive = false;
bool obstacleStopNotified = false;

MeEncoderOnBoard *driveMotors[] = {&motor_L1, &motor_L2, &motor_R1, &motor_R2};

// ===== Encoder interrupts =====
void isr_L1() { digitalRead(motor_L1.getPortB()) == 0 ? motor_L1.pulsePosMinus() : motor_L1.pulsePosPlus(); }
void isr_L2() { digitalRead(motor_L2.getPortB()) == 0 ? motor_L2.pulsePosMinus() : motor_L2.pulsePosPlus(); }
void isr_R1() { digitalRead(motor_R1.getPortB()) == 0 ? motor_R1.pulsePosMinus() : motor_R1.pulsePosPlus(); }
void isr_R2() { digitalRead(motor_R2.getPortB()) == 0 ? motor_R2.pulsePosMinus() : motor_R2.pulsePosPlus(); }

// ===== Utility helpers =====
long parseLongSuffix(const String &s, int start, long defaultValue = 0) {
  if (s.length() <= start) return defaultValue;
  return atol(s.substring(start).c_str());
}

bool parseRgb(const String &s, int start, byte out[3]) {
  int p1 = s.indexOf(',', start);
  int p2 = (p1 >= 0) ? s.indexOf(',', p1 + 1) : -1;
  if (p1 < 0 || p2 < 0) return false;
  out[0] = (byte)s.substring(start, p1).toInt();
  out[1] = (byte)s.substring(p1 + 1, p2).toInt();
  out[2] = (byte)s.substring(p2 + 1).toInt();
  return true;
}

byte scaleByBrightness(byte base, int brightness) {
  return (byte)(((int)base * brightness) / 255);
}

void wheelColor(byte pos, byte &r, byte &g, byte &b) {
  if (pos < 85) {
    r = 255 - pos * 3;
    g = pos * 3;
    b = 0;
  } else if (pos < 170) {
    pos -= 85;
    r = 0;
    g = 255 - pos * 3;
    b = pos * 3;
  } else {
    pos -= 170;
    r = pos * 3;
    g = 0;
    b = 255 - pos * 3;
  }
}

void replyMaster(const String &msg) {
  Serial3.println(msg);
}

// ===== LED rendering =====
void setRingPixel(MeRGBLed &led, int idx, byte r, byte g, byte b) {
  led.setColorAt(idx, r, g, b);
}

void renderSolidRing(MeRGBLed &led, byte r, byte g, byte b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    setRingPixel(led, i, r, g, b);
  }
}

void renderRainbowRing(MeRGBLed &led) {
  byte r, g, b;
  wheelColor(rainbowHue, r, g, b);
  for (int i = 0; i < NUM_LEDS; i++) {
    setRingPixel(led, i, r, g, b);
  }
}

void renderCruiseRings() {
  if (rainbowEnabled) {
    renderRainbowRing(led1);
    renderRainbowRing(led2);
  } else {
    renderSolidRing(led1, defaultColor[0], defaultColor[1], defaultColor[2]);
    renderSolidRing(led2, defaultColor[0], defaultColor[1], defaultColor[2]);
  }
}

void stepRainbow() {
  if (!rainbowEnabled) return;
  if (millis() - lastRainbowStepMs < RAINBOW_STEP_MS) return;
  lastRainbowStepMs = millis();
  rainbowHue++;
}

void stepTurnPulse() {
  if (!turningLeft && !turningRight) {
    turnBrightness = 0;
    turnDir = 1;
    return;
  }

  if (millis() - lastTurnFadeMs < TURN_FADE_MS) return;
  lastTurnFadeMs = millis();

  turnBrightness += turnDir * TURN_STEP;
  if (turnBrightness >= 255) {
    turnBrightness = 255;
    turnDir = -1;
  }
  if (turnBrightness <= TURN_MIN_BRIGHTNESS) {
    turnBrightness = TURN_MIN_BRIGHTNESS;
    turnDir = 1;
  }
}

void overlayTurnSignals() {
  if (!turningLeft && !turningRight) return;

  byte tr = scaleByBrightness(turnColor[0], turnBrightness);
  byte tg = scaleByBrightness(turnColor[1], turnBrightness);
  byte tb = scaleByBrightness(turnColor[2], turnBrightness);

  if (turningLeft) {
    renderSolidRing(led1, tr, tg, tb);
  }
  if (turningRight) {
    renderSolidRing(led2, tr, tg, tb);
  }
}

void renderLedsIfDue() {
  if (millis() - lastRenderMs < LED_RENDER_MS) return;
  lastRenderMs = millis();

  // Render order matters: cruise first, then turn overlay so turn colors never get overruled.
  stepRainbow();
  stepTurnPulse();
  renderCruiseRings();
  overlayTurnSignals();

  led1.show();
  led2.show();
}

// ===== Drive helpers =====
void commandDriveTargets(long leftTarget, long rightTarget, int speed) {
  posL = leftTarget;
  posR = rightTarget;

  motor_L1.moveTo(posL, speed);
  motor_L2.moveTo(posL, speed);
  motor_R1.moveTo(posR, speed);
  motor_R2.moveTo(posR, speed);
}

void moveDrive(long delta, int speed) {
  turningLeft = false;
  turningRight = false;

  // Right side is mirrored to keep forward motion aligned.
  commandDriveTargets(posL + delta, posR - delta, speed);
}

void turnDrive(long delta, int speed) {
  turningLeft = (delta < 0);
  turningRight = (delta > 0);
  commandDriveTargets(posL - delta, posR - delta, speed);
}

void stopDrive() {
  turningLeft = false;
  turningRight = false;

  // Keep software targets aligned to where the robot currently is.
  posL = motor_L1.getCurPos();
  posR = motor_R1.getCurPos();

  motor_L1.setMotorPwm(0);
  motor_L2.setMotorPwm(0);
  motor_R1.setMotorPwm(0);
  motor_R2.setMotorPwm(0);
}

bool isObstacleTooClose() {
  if (millis() - lastUltrasonicPollMs < ULTRASONIC_POLL_MS) {
    return obstacleStopActive;
  }
  lastUltrasonicPollMs = millis();

  long leftCm = ultraSensorLeft.distanceCm();
  long rightCm = ultraSensorRight.distanceCm();

  bool leftBlocked = (leftCm > 0 && leftCm <= ULTRASONIC_STOP_CM);
  bool rightBlocked = (rightCm > 0 && rightCm <= ULTRASONIC_STOP_CM);
  obstacleStopActive = leftBlocked || rightBlocked;
  return obstacleStopActive;
}

void autoStopOnObstacle() {
  if (!isObstacleTooClose()) {
    obstacleStopNotified = false;
    return;
  }

  stopDrive();
  if (!obstacleStopNotified) {
    obstacleStopNotified = true;
    replyMaster("M>SAFETY STOP ULTRASONIC");
  }
}

void autoClearTurnWhenReached() {
  if (!turningLeft && !turningRight) return;

  if (abs(motor_L1.getCurPos() - posL) < 20 && abs(motor_R1.getCurPos() - posR) < 20) {
    turningLeft = false;
    turningRight = false;
  }
}

// ===== Command processing =====
// Supported:
// F<pos>, B<pos>, L[pos], R[pos], x
// N<R>,<G>,<B> (default cruise color)
// T<R>,<G>,<B> (turn indicator color)
// Q<0|1> (rainbow off/on)
// Optional prefix accepted: M:
void processCommand(const String &rawInput) {
  String in = rawInput;
  if (in.startsWith("M:")) in = in.substring(2);
  in.trim();
  if (in.length() == 0) return;

  char cmd = in.charAt(0);

  if (cmd == 'F') {
    long dist = parseLongSuffix(in, 1, 0);
    moveDrive(dist, DRIVE_SPEED);
    replyMaster("M>OK F" + String(dist));
    return;
  }

  if (cmd == 'B') {
    long dist = parseLongSuffix(in, 1, 0);
    moveDrive(-dist, DRIVE_SPEED);
    replyMaster("M>OK B" + String(dist));
    return;
  }

  if (cmd == 'L') {
    long amount = parseLongSuffix(in, 1, 0);
    turnDrive((amount == 0) ? -360 : -amount, DRIVE_SPEED);
    replyMaster("M>OK L");
    return;
  }

  if (cmd == 'R') {
    long amount = parseLongSuffix(in, 1, 0);
    turnDrive((amount == 0) ? 360 : amount, DRIVE_SPEED);
    replyMaster("M>OK R");
    return;
  }

  if (cmd == 'x') {
    stopDrive();
    replyMaster("M>OK STOP");
    return;
  }

  if (cmd == 'N') {
    byte rgb[3];
    if (!parseRgb(in, 1, rgb)) return;
    // Custom cruise color should become active immediately and stay persistent.
    rainbowEnabled = false;
    defaultColor[0] = rgb[0];
    defaultColor[1] = rgb[1];
    defaultColor[2] = rgb[2];
    replyMaster("M>OK N" + String(defaultColor[0]) + "," + String(defaultColor[1]) + "," + String(defaultColor[2]));
    return;
  }

  if (cmd == 'T') {
    byte rgb[3];
    if (!parseRgb(in, 1, rgb)) return;
    turnColor[0] = rgb[0];
    turnColor[1] = rgb[1];
    turnColor[2] = rgb[2];
    replyMaster("M>OK T" + String(turnColor[0]) + "," + String(turnColor[1]) + "," + String(turnColor[2]));
    return;
  }

  if (cmd == 'Q') {
    rainbowEnabled = (parseLongSuffix(in, 1, 0) != 0);
    replyMaster("M>OK Q" + String(rainbowEnabled ? 1 : 0));
    return;
  }
}

void initPidAndTimers() {
  // Timer setup expected by Makeblock encoder control.
  TCCR1A = _BV(WGM10);
  TCCR1B = _BV(CS11) | _BV(WGM12);
  TCCR2A = _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS21);

  for (int i = 0; i < 4; i++) {
    driveMotors[i]->setPulse(7);
    driveMotors[i]->setRatio(26.9);
    driveMotors[i]->setPosPid(1.8, 0, 1.2);
    driveMotors[i]->setSpeedPid(0.18, 0, 0);
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

  attachInterrupt(motor_L1.getIntNum(), isr_L1, RISING);
  attachInterrupt(motor_L2.getIntNum(), isr_L2, RISING);
  attachInterrupt(motor_R1.getIntNum(), isr_R1, RISING);
  attachInterrupt(motor_R2.getIntNum(), isr_R2, RISING);

  initPidAndTimers();
  renderCruiseRings();
  led1.show();
  led2.show();

  Serial.println("Standalone Master ready");
}

void loop() {
  pollCommandStream(Serial3);
  pollCommandStream(Serial);

  autoStopOnObstacle();

  for (int i = 0; i < 4; i++) {
    driveMotors[i]->loop();
  }

  autoClearTurnWhenReached();
  renderLedsIfDue();
}
