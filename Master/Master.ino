#include <MeMegaPi.h>

/* ==========================
   Configuration
   ========================== */
MeEncoderOnBoard motor_L1(SLOT4);
MeEncoderOnBoard motor_L2(SLOT2);
MeEncoderOnBoard motor_R1(SLOT3);
MeEncoderOnBoard motor_R2(SLOT1);

const float WHEEL_DIAMETER = 65.0;
const float TRACK_WIDTH = 200.0;
const float PI_VALUE = 3.14159;
const int ENCODER_PULSES = 360;
const float REDUCTION_RATIO = 46.67;
const int DRIVE_SPEED = 200;

// Turn-signal LED state
bool turningLeft = false;
bool turningRight = false;
unsigned long lastFlickerTime = 0;
bool flickerState = false;
const unsigned long FLICKER_INTERVAL = 200; // ms

/* ==========================
   Interrupt handlers
   ========================== */
void isr_L1() { digitalRead(motor_L1.getPortB()) ? motor_L1.pulsePosPlus() : motor_L1.pulsePosMinus(); }
void isr_L2() { digitalRead(motor_L2.getPortB()) ? motor_L2.pulsePosPlus() : motor_L2.pulsePosMinus(); }
void isr_R1() { digitalRead(motor_R1.getPortB()) ? motor_R1.pulsePosPlus() : motor_R1.pulsePosMinus(); }
void isr_R2() { digitalRead(motor_R2.getPortB()) ? motor_R2.pulsePosPlus() : motor_R2.pulsePosMinus(); }

/* ==========================
   Slave Communication (Serial3)
   All slave commands use "S:" prefix.
   ========================== */
void sendToSlave(const String &cmd) {
  Serial3.println("S:" + cmd);
}

// Direct LED set (immediate, for turn signals)
void sendLedDirect(byte connector, byte r, byte g, byte b) {
  sendToSlave("D" + String(connector) + "," + String(r) + "," + String(g) + "," + String(b));
}

// Smooth LED transition (for user-initiated colors)
void sendLedSmooth(byte connector, byte r, byte g, byte b) {
  sendToSlave("L" + String(connector) + "," + String(r) + "," + String(g) + "," + String(b));
}

// Arm motor speed: slot 1-4, speed -200..200
void sendArmSpeed(int slot, int speed) {
  sendToSlave("A" + String(slot) + "," + String(speed));
}

// Stop all arm motors on slave
void sendArmStopAll() {
  sendToSlave("x");
}

/* ==========================
   LED Turn-Signal Logic
   ========================== */
void updateLeds(bool forceUpdate) {
  bool needsUpdate = forceUpdate;

  if (turningLeft || turningRight) {
    unsigned long now = millis();
    if (now - lastFlickerTime >= FLICKER_INTERVAL) {
      flickerState = !flickerState;
      lastFlickerTime = now;
      needsUpdate = true;
    }
  }

  if (!needsUpdate) return;

  byte c1R, c1G, c1B, c2R, c2G, c2B;

  if (turningLeft) {
    // Left connector flickers yellow, right stays blue
    if (flickerState) { c1R = 255; c1G = 255; c1B = 0; }
    else              { c1R = 0;   c1G = 0;   c1B = 0; }
    c2R = 0; c2G = 0; c2B = 255;
  } else if (turningRight) {
    // Right connector flickers yellow, left stays blue
    c1R = 0; c1G = 0; c1B = 255;
    if (flickerState) { c2R = 255; c2G = 255; c2B = 0; }
    else              { c2R = 0;   c2G = 0;   c2B = 0; }
  } else {
    // Solid blue
    c1R = 0; c1G = 0; c1B = 255;
    c2R = 0; c2G = 0; c2B = 255;
  }

  sendLedDirect(1, c1R, c1G, c1B);
  sendLedDirect(2, c2R, c2G, c2B);
}

/* ==========================
   Motion Logic
   ========================== */
void moveMeters(float meters) {
  turningLeft = false;
  turningRight = false;
  updateLeds(true);

  long targetPulses = (meters * 1000.0 / (WHEEL_DIAMETER * PI_VALUE)) * ENCODER_PULSES * REDUCTION_RATIO;

  motor_L1.move(-targetPulses, DRIVE_SPEED);
  motor_L2.move(-targetPulses, DRIVE_SPEED);
  motor_R1.move(targetPulses, DRIVE_SPEED);
  motor_R2.move(targetPulses, DRIVE_SPEED);
}

void rotateDegrees(float degrees) {
  turningLeft = (degrees < 0);
  turningRight = (degrees > 0);
  updateLeds(true);

  float arcLength = (abs(degrees) * PI_VALUE * TRACK_WIDTH) / 360.0;
  long targetPulses = (arcLength / (WHEEL_DIAMETER * PI_VALUE)) * ENCODER_PULSES * REDUCTION_RATIO;

  if (degrees < 0) {
    motor_L1.move(targetPulses, DRIVE_SPEED);
    motor_L2.move(targetPulses, DRIVE_SPEED);
    motor_R1.move(targetPulses, DRIVE_SPEED);
    motor_R2.move(targetPulses, DRIVE_SPEED);
  } else {
    motor_L1.move(-targetPulses, DRIVE_SPEED);
    motor_L2.move(-targetPulses, DRIVE_SPEED);
    motor_R1.move(-targetPulses, DRIVE_SPEED);
    motor_R2.move(-targetPulses, DRIVE_SPEED);
  }
}

void stopDrive() {
  turningLeft = false;
  turningRight = false;
  updateLeds(true);

  motor_L1.setTarPWM(0); motor_L2.setTarPWM(0);
  motor_R1.setTarPWM(0); motor_R2.setTarPWM(0);
}

/* ==========================
   Command Processing
   ========================== */
void processCommand(const String &input) {
  if (input.length() == 0) return;

  char cmd = input.charAt(0);
  float val = 0;
  if (input.length() > 1) val = input.substring(1).toFloat();

  switch (cmd) {
    case 'F':
      moveMeters(val);
      Serial3.println("M>OK F" + String(val));
      break;

    case 'B':
      moveMeters(-val);
      Serial3.println("M>OK B" + String(val));
      break;

    case 'L':
      rotateDegrees(val == 0 ? -90 : -val);
      Serial3.println("M>OK L");
      break;

    case 'R':
      rotateDegrees(val == 0 ? 90 : val);
      Serial3.println("M>OK R");
      break;

    case 'x':
      stopDrive();
      Serial3.println("M>OK STOP");
      break;

    case 'C': {
      // LED color: C<connector>,<R>,<G>,<B>  → forwarded as smooth transition
      int c1 = input.indexOf(',');
      int c2 = input.indexOf(',', c1 + 1);
      int c3 = input.indexOf(',', c2 + 1);
      if (c1 > 0 && c2 > c1 && c3 > c2) {
        byte connector = input.substring(1, c1).toInt();
        byte r = input.substring(c1 + 1, c2).toInt();
        byte g = input.substring(c2 + 1, c3).toInt();
        byte b = input.substring(c3 + 1).toInt();
        if (connector == 1 || connector == 2) {
          sendLedSmooth(connector, r, g, b);
          Serial3.println("M>OK C" + String(connector));
        }
      }
      break;
    }

    case 'A': {
      // Arm motor: A<slot>,<speed> or Ax (stop all)
      if (input.length() > 1 && input.charAt(1) == 'x') {
        sendArmStopAll();
        Serial3.println("M>OK Ax");
      } else {
        int comma = input.indexOf(',');
        if (comma > 0) {
          int slot  = input.substring(1, comma).toInt();
          int speed = input.substring(comma + 1).toInt();
          sendArmSpeed(slot, speed);
          Serial3.println("M>OK A" + String(slot));
        }
      }
      break;
    }
  }
}

/* ==========================
   Setup
   ========================== */
void setup() {
  Serial.begin(115200);
  Serial3.begin(115200);
  Serial3.setTimeout(10);

  attachInterrupt(motor_L1.getIntNum(), isr_L1, RISING);
  attachInterrupt(motor_L2.getIntNum(), isr_L2, RISING);
  attachInterrupt(motor_R1.getIntNum(), isr_R1, RISING);
  attachInterrupt(motor_R2.getIntNum(), isr_R2, RISING);

  MeEncoderOnBoard* motors[] = {&motor_L1, &motor_L2, &motor_R1, &motor_R2};
  for (int i = 0; i < 4; i++) {
    motors[i]->setPulse(ENCODER_PULSES);
    motors[i]->setRatio(REDUCTION_RATIO);
    motors[i]->setPosPid(1.8, 0, 1.2);
    motors[i]->setSpeedPid(0.18, 0, 0);
  }

  delay(100);
  updateLeds(true);
  Serial.println("Master ready");
}

/* ==========================
   Main Loop
   ========================== */
void loop() {
  // Flicker turn-signal LEDs (only sends when state changes)
  if (turningLeft || turningRight) {
    updateLeds(false);
  }

  // ── Serial3 (Bluetooth / shared bus) ──
  if (Serial3.available()) {
    String input = Serial3.readStringUntil('\n');
    input.trim();

    if (input.startsWith("M:")) {
      processCommand(input.substring(2));
    }
    // S: messages are for the Slave → ignore
    // S> responses are from the Slave → relay to USB for debugging
    if (input.startsWith("S>")) {
      Serial.println("[Slave] " + input.substring(2));
    }
  }

  // ── USB Serial (debug console) ──
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.startsWith("S:")) {
      // Forward directly to slave
      Serial3.println(input);
    } else if (input.startsWith("M:")) {
      processCommand(input.substring(2));
    } else {
      // No prefix → treat as master command
      processCommand(input);
    }
  }

  // PID loops must always run
  motor_L1.loop();
  motor_L2.loop();
  motor_R1.loop();
  motor_R2.loop();
}
