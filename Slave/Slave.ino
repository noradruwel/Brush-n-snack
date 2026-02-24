#include <MeMegaPi.h>

/* ==========================
   Configuration
   ========================== */
// Two LED strips on PORT_5 and PORT_6
MeRGBLed led1(PORT_5);  // Connector 1: 4 LEDs
MeRGBLed led2(PORT_6);  // Connector 2: 4 LEDs

// 4 Arm Motors (robot arm joints)
MeEncoderOnBoard armMotor1(SLOT1);
MeEncoderOnBoard armMotor2(SLOT2);
MeEncoderOnBoard armMotor3(SLOT3);
MeEncoderOnBoard armMotor4(SLOT4);

const int MAX_ARM_SPEED = 200;

// LED state
byte colors[2][3] = { {0, 0, 255}, {0, 0, 255} };       // Current
byte targetColors[2][3] = { {0, 0, 255}, {0, 0, 255} };  // For smooth transition
unsigned long lastTransitionTime = 0;
const unsigned long TRANSITION_INTERVAL = 10; // ms

/* ==========================
   Interrupt handlers (4 arm motors)
   ========================== */
void isr_Arm1() { digitalRead(armMotor1.getPortB()) ? armMotor1.pulsePosPlus() : armMotor1.pulsePosMinus(); }
void isr_Arm2() { digitalRead(armMotor2.getPortB()) ? armMotor2.pulsePosPlus() : armMotor2.pulsePosMinus(); }
void isr_Arm3() { digitalRead(armMotor3.getPortB()) ? armMotor3.pulsePosPlus() : armMotor3.pulsePosMinus(); }
void isr_Arm4() { digitalRead(armMotor4.getPortB()) ? armMotor4.pulsePosPlus() : armMotor4.pulsePosMinus(); }

/* ==========================
   Arm Motor Control
   ========================== */
MeEncoderOnBoard* getArmMotor(int slot) {
  switch (slot) {
    case 1: return &armMotor1;
    case 2: return &armMotor2;
    case 3: return &armMotor3;
    case 4: return &armMotor4;
    default: return nullptr;
  }
}

void setArmSpeed(int slot, int speed) {
  MeEncoderOnBoard* motor = getArmMotor(slot);
  if (motor) {
    speed = constrain(speed, -MAX_ARM_SPEED, MAX_ARM_SPEED);
    motor->setTarPWM(speed);
  }
}

void stopAllArms() {
  armMotor1.setTarPWM(0);
  armMotor2.setTarPWM(0);
  armMotor3.setTarPWM(0);
  armMotor4.setTarPWM(0);
}

/* ==========================
   LED Control
   ========================== */
void applyAllLeds(byte connector, byte r, byte g, byte b) {
  MeRGBLed* led = (connector == 1) ? &led1 : &led2;
  for (int i = 0; i < 4; i++) {
    led->setColorAt(i, r, g, b);
  }
  led->show();

  colors[connector - 1][0] = r;
  colors[connector - 1][1] = g;
  colors[connector - 1][2] = b;
}

void setAllLeds(byte connector, byte r, byte g, byte b) {
  applyAllLeds(connector, r, g, b);
  // Sync target so transitions don't override
  targetColors[connector - 1][0] = r;
  targetColors[connector - 1][1] = g;
  targetColors[connector - 1][2] = b;
}

void setTargetColor(byte connector, byte r, byte g, byte b) {
  targetColors[connector - 1][0] = r;
  targetColors[connector - 1][1] = g;
  targetColors[connector - 1][2] = b;
}

void updateTransitions() {
  unsigned long now = millis();
  if (now - lastTransitionTime < TRANSITION_INTERVAL) return;
  lastTransitionTime = now;

  bool updated = false;
  for (int conn = 0; conn < 2; conn++) {
    for (int c = 0; c < 3; c++) {
      if (colors[conn][c] < targetColors[conn][c]) { colors[conn][c]++; updated = true; }
      else if (colors[conn][c] > targetColors[conn][c]) { colors[conn][c]--; updated = true; }
    }
  }

  if (updated) {
    applyAllLeds(1, colors[0][0], colors[0][1], colors[0][2]);
    applyAllLeds(2, colors[1][0], colors[1][1], colors[1][2]);
  }
}

/* ==========================
   Command Processing
   Commands arrive WITHOUT the "S:" prefix (already stripped).
   ========================== */
void processCommand(const String &input) {
  if (input.length() == 0) return;

  char cmd = input.charAt(0);

  switch (cmd) {
    case 'L': {
      // Smooth LED transition: L<connector>,<R>,<G>,<B>
      int c1 = input.indexOf(',');
      int c2 = input.indexOf(',', c1 + 1);
      int c3 = input.indexOf(',', c2 + 1);
      if (c1 > 0 && c2 > c1 && c3 > c2) {
        byte connector = input.substring(1, c1).toInt();
        byte r = input.substring(c1 + 1, c2).toInt();
        byte g = input.substring(c2 + 1, c3).toInt();
        byte b = input.substring(c3 + 1).toInt();
        if (connector == 1 || connector == 2) {
          setTargetColor(connector, r, g, b);
          Serial3.println("S>OK L" + String(connector));
        }
      }
      break;
    }

    case 'D': {
      // Direct/immediate LED set: D<connector>,<R>,<G>,<B>
      int c1 = input.indexOf(',');
      int c2 = input.indexOf(',', c1 + 1);
      int c3 = input.indexOf(',', c2 + 1);
      if (c1 > 0 && c2 > c1 && c3 > c2) {
        byte connector = input.substring(1, c1).toInt();
        byte r = input.substring(c1 + 1, c2).toInt();
        byte g = input.substring(c2 + 1, c3).toInt();
        byte b = input.substring(c3 + 1).toInt();
        if (connector == 1 || connector == 2) {
          setAllLeds(connector, r, g, b);
        }
      }
      break;
    }

    case 'A': {
      // Arm motor speed: A<slot>,<speed>
      int comma = input.indexOf(',');
      if (comma > 0) {
        int slot  = input.substring(1, comma).toInt();
        int speed = input.substring(comma + 1).toInt();
        if (slot >= 1 && slot <= 4) {
          setArmSpeed(slot, speed);
          Serial3.println("S>OK A" + String(slot));
        }
      }
      break;
    }

    case 'x':
      // Stop all arm motors
      stopAllArms();
      Serial3.println("S>OK STOP");
      break;
  }
}

/* ==========================
   Setup
   ========================== */
void setup() {
  Serial.begin(115200);
  Serial3.begin(115200);
  Serial3.setTimeout(10);

  // Attach interrupts for all 4 arm motors
  attachInterrupt(armMotor1.getIntNum(), isr_Arm1, RISING);
  attachInterrupt(armMotor2.getIntNum(), isr_Arm2, RISING);
  attachInterrupt(armMotor3.getIntNum(), isr_Arm3, RISING);
  attachInterrupt(armMotor4.getIntNum(), isr_Arm4, RISING);

  // Initialize arm motor parameters
  MeEncoderOnBoard* motors[] = {&armMotor1, &armMotor2, &armMotor3, &armMotor4};
  for (int i = 0; i < 4; i++) {
    motors[i]->setPulse(360);
    motors[i]->setRatio(46.67);
    motors[i]->setPosPid(1.8, 0, 1.2);
    motors[i]->setSpeedPid(0.18, 0, 0);
  }

  // Default blue LEDs
  setAllLeds(1, 0, 0, 255);
  setAllLeds(2, 0, 0, 255);

  Serial.println("Slave ready");
}

/* ==========================
   Main Loop
   ========================== */
void loop() {
  updateTransitions();

  // ── Serial3 (shared bus) ──
  if (Serial3.available()) {
    String input = Serial3.readStringUntil('\n');
    input.trim();

    if (input.startsWith("S:")) {
      processCommand(input.substring(2));
    }
    // M: commands are for Master → ignore
    // M> responses are from Master → relay to USB for debugging
    if (input.startsWith("M>")) {
      Serial.println("[Master] " + input.substring(2));
    }
  }

  // ── USB Serial (debug console) ──
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.startsWith("M:")) {
      // Forward to master
      Serial3.println(input);
    } else if (input.startsWith("S:")) {
      processCommand(input.substring(2));
    } else {
      // No prefix → treat as slave command
      processCommand(input);
    }
  }

  // PID loops for all 4 arm motors
  armMotor1.loop();
  armMotor2.loop();
  armMotor3.loop();
  armMotor4.loop();
}
