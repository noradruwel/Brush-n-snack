#include <MeMegaPi.h>

/* ═══════════════════════════════════════
   HARDWARE
   ═══════════════════════════════════════ */
// Arm encoder motors (position-controlled)
MeEncoderOnBoard arm1(SLOT1);   // Base rotation
MeEncoderOnBoard arm2(SLOT2);   // Shoulder
MeEncoderOnBoard arm3(SLOT3);   // Elbow

// Gripper DC motor (no encoder – just open/close at speed)
MeMegaPiDCMotor gripper(PORT4A);

/* ═══════════════════════════════════════
   CONSTANTS
   ═══════════════════════════════════════ */
const int ARM_SPEED     = 200;   // default arm speed
const int GRIPPER_SPEED = 100;
const unsigned long GRIPPER_MAX_RUN_MS = 5000;

bool gripperRunning = false;
unsigned long gripperStartMs = 0;
long armTargets[3] = {0, 0, 0};

/* ═══════════════════════════════════════
   INTERRUPT SERVICE ROUTINES
   ═══════════════════════════════════════ */
void isr_Arm1() { digitalRead(arm1.getPortB()) == 0 ? arm1.pulsePosMinus() : arm1.pulsePosPlus(); }
// SLOT2 encoder channels are physically swapped on the MeMegaPi PCB — flip direction to compensate
void isr_Arm2() { digitalRead(arm2.getPortB()) == 0 ? arm2.pulsePosPlus() : arm2.pulsePosMinus(); }
void isr_Arm3() { digitalRead(arm3.getPortB()) == 0 ? arm3.pulsePosMinus() : arm3.pulsePosPlus(); }

void gripperOpen();
void gripperClose();
void gripperStop();

/* ═══════════════════════════════════════
   ARM HELPERS
   ═══════════════════════════════════════ */
MeEncoderOnBoard* getArm(int slot) {
  switch (slot) {
    case 1: return &arm1;
    case 2: return &arm2;
    case 3: return &arm3;
    default: return nullptr;
  }
}

void armMoveTo(int slot, long pos, int spd) {
  MeEncoderOnBoard* m = getArm(slot);
  if (m) {
    armTargets[slot - 1] = pos;
    m->moveTo(pos, (float)spd);
  }
}

// moveDrive-style helper: add delta on top of stored target
void armMoveBy(int slot, long delta, int spd) {
  if (slot < 1 || slot > 3) return;
  long target = armTargets[slot - 1] + delta;
  armMoveTo(slot, target, spd);
}

void stopAll() {
  arm1.setMotorPwm(0);
  arm2.setMotorPwm(0);
  arm3.setMotorPwm(0);
  gripperStop();
}

/* ═══════════════════════════════════════
   GRIPPER HELPERS
   ═══════════════════════════════════════ */
void gripperOpen()  {
  gripper.run(-GRIPPER_SPEED);
  gripperRunning = true;
  gripperStartMs = millis();
}

void gripperClose() {
  gripper.run( GRIPPER_SPEED);
  gripperRunning = true;
  gripperStartMs = millis();
}

void gripperStop()  {
  gripper.stop();
  gripperRunning = false;
}

/* ═══════════════════════════════════════
   COMMAND PROCESSING
   (arrives WITHOUT the "S:" prefix)
   ═══════════════════════════════════════ */
void processCommand(const String &in) {
  if (in.length() == 0) return;
  char cmd = in.charAt(0);

  switch (cmd) {

    /* ── Arm moveBy: A<slot><deltaPos> ── */
    case 'A': {
      if (in.length() >= 3 && in.charAt(1) >= '1' && in.charAt(1) <= '3') {
        // Only format: A<slot><deltaPos>, e.g. A1100 or A2-250
        int slot = in.charAt(1) - '0';
        long delta = atol(in.substring(2).c_str());
        armMoveBy(slot, delta, ARM_SPEED);
        Serial3.println("S>OK A" + String(slot));
      }
      break;
    }

    /* ── Gripper: Go / Gc / Gs ── */
    case 'G': {
      if (in.length() > 1) {
        char a = in.charAt(1);
        if      (a == 'o') { gripperOpen();  Serial3.println("S>OK Go"); }
        else if (a == 'c') { gripperClose(); Serial3.println("S>OK Gc"); }
        else if (a == 's') { gripperStop();  Serial3.println("S>OK Gs"); }
      }
      break;
    }

    /* ── Stop everything ── */
    case 'x':
      stopAll();
      Serial3.println("S>OK STOP");
      break;
  }
}

/* ═══════════════════════════════════════
   SETUP
   ═══════════════════════════════════════ */
void setup() {
  Serial.begin(115200);
  Serial3.begin(115200);
  Serial3.setTimeout(10);

  // Arm motor interrupts
  attachInterrupt(arm1.getIntNum(), isr_Arm1, RISING);
  attachInterrupt(arm2.getIntNum(), isr_Arm2, RISING);
  attachInterrupt(arm3.getIntNum(), isr_Arm3, RISING);

  // PWM 8 KHz
  TCCR1A = _BV(WGM10);
  TCCR1B = _BV(CS11) | _BV(WGM12);
  TCCR2A = _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS21);

  // Init arm motors
  MeEncoderOnBoard* m[] = {&arm1, &arm2, &arm3};
  for (int i = 0; i < 3; i++) {
    m[i]->setPulse(7);
    m[i]->setRatio(26.9);
    m[i]->setPosPid(1.8, 0, 1.2);
    m[i]->setSpeedPid(0.18, 0, 0);
  }

  Serial.println("Slave ready");
}

/* ═══════════════════════════════════════
   MAIN LOOP
   ═══════════════════════════════════════ */
void loop() {
  // ── Serial3 (shared bus) ──
  if (Serial3.available()) {
    String in = Serial3.readStringUntil('\n');
    in.trim();
    if      (in.startsWith("S:")) processCommand(in.substring(2));
    else if (in.startsWith("M>")) Serial.println("[Master] " + in.substring(2));
  }

  // ── USB Serial (debug) ──
  if (Serial.available()) {
    String in = Serial.readStringUntil('\n');
    in.trim();
    if      (in.startsWith("M:")) Serial3.println(in);           // forward to master
    else if (in.startsWith("S:")) processCommand(in.substring(2));
    else                          processCommand(in);             // no prefix → slave
  }

  // PID loops
  arm1.loop();
  arm2.loop();
  arm3.loop();

  // Auto-stop gripper after fixed run window to avoid stalling at limits.
  if (gripperRunning && (millis() - gripperStartMs >= GRIPPER_MAX_RUN_MS)) {
    gripperStop();
    Serial.println("[Slave] Gripper auto-stopped");
  }
}
