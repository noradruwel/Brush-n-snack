#include <MeMegaPi.h>

/* ═══════════════════════════════════════
   HARDWARE
   ═══════════════════════════════════════ */
// Drive motors (encoder)
MeEncoderOnBoard motor_L1(SLOT2);   // Left front
MeEncoderOnBoard motor_L2(SLOT1);   // Left rear
MeEncoderOnBoard motor_R1(SLOT4);   // Right front
MeEncoderOnBoard motor_R2(SLOT3);   // Right rearb

// LED strips (directly on Master)
MeRGBLed led1(PORT_5);              // Left  – 4 LEDs
MeRGBLed led2(PORT_8);              // Right – 4 LEDs

/* ═══════════════════════════════════════
   CONSTANTS
   ═══════════════════════════════════════ */
const int    DRIVE_SPEED       = 300;
const int    NUM_LEDS          = 4;
const unsigned long TURN_FADE_MS = 12;
const int TURN_STEP = 8;
const int TURN_MIN_BRIGHTNESS = 20;
const unsigned long FADE_MS    = 30;   // ms per color step

/* ═══════════════════════════════════════
   STATE
   ═══════════════════════════════════════ */
// Drive position accumulators (incremental movement)
long  posL = 0;           // absolute target for L motors
long  posR = 0;           // absolute target for R motors (already accounts for inversion)

// Turn signals
bool  turningLeft   = false;
bool  turningRight  = false;
int   turnBrightness = 0;
int   turnDir = 1;
unsigned long lastTurnFade = 0;
byte  turnColor[3] = {255, 170, 0};   // warm amber/yellow

// LED smooth-fade
byte  curColor [2][3] = { {0,0,255}, {0,0,255} };
byte  tgtColor [2][3] = { {0,0,255}, {0,0,255} };
unsigned long lastFade = 0;

/* ═══════════════════════════════════════
   INTERRUPT SERVICE ROUTINES
   ═══════════════════════════════════════ */
void isr_L1() { digitalRead(motor_L1.getPortB()) == 0 ? motor_L1.pulsePosMinus() : motor_L1.pulsePosPlus(); }
void isr_L2() { digitalRead(motor_L2.getPortB()) == 0 ? motor_L2.pulsePosMinus() : motor_L2.pulsePosPlus(); }
void isr_R1() { digitalRead(motor_R1.getPortB()) == 0 ? motor_R1.pulsePosMinus() : motor_R1.pulsePosPlus(); }
void isr_R2() { digitalRead(motor_R2.getPortB()) == 0 ? motor_R2.pulsePosMinus() : motor_R2.pulsePosPlus(); }

/* ═══════════════════════════════════════
   SLAVE COMMUNICATION  (Serial3, prefix "S:")
   ═══════════════════════════════════════ */
void sendSlave(const String &cmd) { Serial3.println("S:" + cmd); }

// Arm: moveTo position at speed
void sendArmMoveTo(int slot, long pos, int spd) {
  sendSlave("A" + String(slot) + "," + String(pos) + "," + String(spd));
}

// Arm: stop all + gripper
void sendArmStopAll() { sendSlave("x"); }

// Gripper
void sendGripperOpen()  { sendSlave("Go"); }
void sendGripperClose() { sendSlave("Gc"); }
void sendGripperStop()  { sendSlave("Gs"); }

/* ═══════════════════════════════════════
   LED CONTROL  (local on Master)
   ═══════════════════════════════════════ */
void applyLed(byte conn, byte r, byte g, byte b) {
  MeRGBLed &led = (conn == 1) ? led1 : led2;
  for (int i = 0; i < NUM_LEDS; i++) led.setColorAt(i, r, g, b);
  led.show();
  curColor[conn - 1][0] = r;
  curColor[conn - 1][1] = g;
  curColor[conn - 1][2] = b;
}

byte scaleByBrightness(byte base, int brightness) {
  return (byte)(((int)base * brightness) / 255);
}

// Instant set (turn signals / direct command)
void setLed(byte conn, byte r, byte g, byte b) {
  applyLed(conn, r, g, b);
  tgtColor[conn - 1][0] = r;
  tgtColor[conn - 1][1] = g;
  tgtColor[conn - 1][2] = b;
}

// Start smooth fade
void fadeLedTo(byte conn, byte r, byte g, byte b) {
  tgtColor[conn - 1][0] = r;
  tgtColor[conn - 1][1] = g;
  tgtColor[conn - 1][2] = b;
}

void updateFades() {
  if (turningLeft || turningRight) return;   // turn signals take full LED control
  unsigned long now = millis();
  if (now - lastFade < FADE_MS) return;
  lastFade = now;

  bool changed = false;
  for (int c = 0; c < 2; c++) {
    for (int i = 0; i < 3; i++) {
      if      (curColor[c][i] < tgtColor[c][i]) { curColor[c][i]++; changed = true; }
      else if (curColor[c][i] > tgtColor[c][i]) { curColor[c][i]--; changed = true; }
    }
  }
  if (changed) {
    applyLed(1, curColor[0][0], curColor[0][1], curColor[0][2]);
    applyLed(2, curColor[1][0], curColor[1][1], curColor[1][2]);
  }
}

/* ═══════════════════════════════════════
   TURN-SIGNAL LOGIC
   ═══════════════════════════════════════ */
void updateTurnSignals(bool force) {
  if (!force && !turningLeft && !turningRight) return;

  if (turningLeft || turningRight) {
    unsigned long now = millis();
    if (force || (now - lastTurnFade >= TURN_FADE_MS)) {
      lastTurnFade = now;
      turnBrightness += (turnDir * TURN_STEP);
      if (turnBrightness >= 255) { turnBrightness = 255; turnDir = -1; }
      if (turnBrightness <= TURN_MIN_BRIGHTNESS)  { turnBrightness = TURN_MIN_BRIGHTNESS; turnDir = 1; }
    } else {
      return;
    }
  }

  if (turningLeft) {
    setLed(1,
           scaleByBrightness(turnColor[0], turnBrightness),
           scaleByBrightness(turnColor[1], turnBrightness),
           scaleByBrightness(turnColor[2], turnBrightness));
    setLed(2, 0, 0, 255);
  } else if (turningRight) {
    setLed(1, 0, 0, 255);
    setLed(2,
           scaleByBrightness(turnColor[0], turnBrightness),
           scaleByBrightness(turnColor[1], turnBrightness),
           scaleByBrightness(turnColor[2], turnBrightness));
  } else {
    turnBrightness = 0;
    turnDir = 1;
    setLed(1, 0, 0, 255);
    setLed(2, 0, 0, 255);
  }
}

/* ═══════════════════════════════════════
   DRIVE LOGIC  (moveTo position-based)
   ═══════════════════════════════════════ */
// delta > 0 = forward, delta < 0 = backward
void moveDrive(long delta, int spd) {
  turningLeft = turningRight = false;
  updateTurnSignals(true);
  posL += delta;
  posR -= delta;   // R motors are physically mirrored: negate for same real-world direction
  motor_L1.moveTo(posL, spd);
  motor_L2.moveTo(posL, spd);
  motor_R1.moveTo(posR, spd);
  motor_R2.moveTo(posR, spd);
}

// delta > 0 = right, delta < 0 = left
void turnDrive(long delta, int spd) {
  turningLeft  = (delta < 0);
  turningRight = (delta > 0);
  updateTurnSignals(true);
  posL -= delta;
  posR -= delta;   // negated: corrects physical turn direction
  motor_L1.moveTo(posL, spd);
  motor_L2.moveTo(posL, spd);
  motor_R1.moveTo(posR, spd);
  motor_R2.moveTo(posR, spd);
}

void stopDrive() {
  turningLeft = turningRight = false;
  updateTurnSignals(true);
  motor_L1.setMotorPwm(0);
  motor_L2.setMotorPwm(0);
  motor_R1.setMotorPwm(0);
  motor_R2.setMotorPwm(0);
}

/* ═══════════════════════════════════════
   COMMAND PROCESSING
   ═══════════════════════════════════════ */
void processCommand(const String &in) {
  if (in.length() == 0) return;
  char cmd = in.charAt(0);
  float val = (in.length() > 1) ? in.substring(1).toFloat() : 0;

  switch (cmd) {

    /* ── Drive ───────────────────────── */
    case 'F': moveDrive( (long)val, DRIVE_SPEED);                    Serial3.println("M>OK F" + String((long)val)); break;
    case 'B': moveDrive(-(long)val, DRIVE_SPEED);                    Serial3.println("M>OK B" + String((long)val)); break;
    case 'L': turnDrive(val == 0 ? -360 : -(long)val, DRIVE_SPEED); Serial3.println("M>OK L");                     break;
    case 'R': turnDrive(val == 0 ?  360 :  (long)val, DRIVE_SPEED); Serial3.println("M>OK R");                     break;
    case 'x': stopDrive();                                           Serial3.println("M>OK STOP");                  break;

    /* ── LEDs (local) ────────────────── */
    case 'C': {  // Smooth fade: C<conn>,<R>,<G>,<B>
      int p1 = in.indexOf(','), p2 = in.indexOf(',', p1+1), p3 = in.indexOf(',', p2+1);
      if (p1 > 0 && p2 > p1 && p3 > p2) {
        byte conn = in.substring(1, p1).toInt();
        byte r = in.substring(p1+1, p2).toInt(), g = in.substring(p2+1, p3).toInt(), b = in.substring(p3+1).toInt();
        if (conn == 1 || conn == 2) { fadeLedTo(conn, r, g, b); Serial3.println("M>OK C" + String(conn)); }
      }
      break;
    }
    case 'D': {  // Direct set: D<conn>,<R>,<G>,<B>
      int p1 = in.indexOf(','), p2 = in.indexOf(',', p1+1), p3 = in.indexOf(',', p2+1);
      if (p1 > 0 && p2 > p1 && p3 > p2) {
        byte conn = in.substring(1, p1).toInt();
        byte r = in.substring(p1+1, p2).toInt(), g = in.substring(p2+1, p3).toInt(), b = in.substring(p3+1).toInt();
        if (conn == 1 || conn == 2) { setLed(conn, r, g, b); Serial3.println("M>OK D" + String(conn)); }
      }
      break;
    }
    case 'T': {  // Turn color: T<R>,<G>,<B>
      int p1 = in.indexOf(','), p2 = in.indexOf(',', p1+1);
      if (p1 > 0 && p2 > p1) {
        turnColor[0] = in.substring(1, p1).toInt();
        turnColor[1] = in.substring(p1+1, p2).toInt();
        turnColor[2] = in.substring(p2+1).toInt();
        Serial3.println("M>OK T" + String(turnColor[0]) + "," + String(turnColor[1]) + "," + String(turnColor[2]));
        updateTurnSignals(true);
      }
      break;
    }

    /* ── Arm (forwarded to Slave) ────── */
    case 'A': {
      if (in.length() > 1 && in.charAt(1) == 'x') {
        sendArmStopAll();
        Serial3.println("M>OK Ax");
      } else {
        // A<slot>,<position>,<speed>
        int c1 = in.indexOf(','), c2 = in.indexOf(',', c1+1);
        if (c1 > 0 && c2 > c1) {
          int  slot = in.substring(1, c1).toInt();
          long pos  = atol(in.substring(c1+1, c2).c_str());
          int  spd  = in.substring(c2+1).toInt();
          sendArmMoveTo(slot, pos, spd);
          Serial3.println("M>OK A" + String(slot));
        }
      }
      break;
    }

    /* ── Gripper (forwarded to Slave) ── */
    case 'G': {
      if (in.length() > 1) {
        char a = in.charAt(1);
        if      (a == 'o') { sendGripperOpen();  Serial3.println("M>OK Go"); }
        else if (a == 'c') { sendGripperClose(); Serial3.println("M>OK Gc"); }
        else if (a == 's') { sendGripperStop();  Serial3.println("M>OK Gs"); }
      }
      break;
    }
  }
}

/* ═══════════════════════════════════════
   SETUP
   ═══════════════════════════════════════ */
void setup() {
  Serial.begin(115200);
  Serial3.begin(115200);
  Serial3.setTimeout(10);

  // Drive motor interrupts
  attachInterrupt(motor_L1.getIntNum(), isr_L1, RISING);
  attachInterrupt(motor_L2.getIntNum(), isr_L2, RISING);
  attachInterrupt(motor_R1.getIntNum(), isr_R1, RISING);
  attachInterrupt(motor_R2.getIntNum(), isr_R2, RISING);

  // PWM 8 KHz
  TCCR1A = _BV(WGM10);
  TCCR1B = _BV(CS11) | _BV(WGM12);
  TCCR2A = _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS21);

  // Init drive motors
  MeEncoderOnBoard* m[] = {&motor_L1, &motor_L2, &motor_R1, &motor_R2};
  for (int i = 0; i < 4; i++) {
    m[i]->setPulse(7);
    m[i]->setRatio(26.9);
    m[i]->setPosPid(1.8, 0, 1.2);
    m[i]->setSpeedPid(0.18, 0, 0);
  }

  // Init LEDs – default blue
  setLed(1, 0, 0, 255);
  setLed(2, 0, 0, 255);

  Serial.println("Master ready");
}

/* ═══════════════════════════════════════
   MAIN LOOP
   ═══════════════════════════════════════ */
void loop() {
  // Turn-signal flicker
  updateTurnSignals(false);

  // LED smooth fades
  updateFades();

  // ── Serial3 (Bluetooth / shared bus) ──
  if (Serial3.available()) {
    String in = Serial3.readStringUntil('\n');
    in.trim();
    if      (in.startsWith("M:")) processCommand(in.substring(2));
    else if (in.startsWith("S>")) Serial.println("[Slave] " + in.substring(2));
  }

  // ── USB Serial (debug) ──
  if (Serial.available()) {
    String in = Serial.readStringUntil('\n');
    in.trim();
    if      (in.startsWith("S:")) Serial3.println(in);           // forward to slave
    else if (in.startsWith("M:")) processCommand(in.substring(2));
    else                          processCommand(in);             // no prefix → master
  }

  // PID loops
  motor_L1.loop();
  motor_L2.loop();
  motor_R1.loop();
  motor_R2.loop();

  // Auto-clear turn signals once motors have settled at target
  if (turningLeft || turningRight) {
    if (abs(motor_L1.getCurPos() - posL) < 20 &&
        abs(motor_R1.getCurPos() - posR) < 20) {
      turningLeft = turningRight = false;
      updateTurnSignals(true);   // sets both LEDs back to blue
    }
  }
}
