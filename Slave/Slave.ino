#include <MeMegaPi.h>

/* ==========================
   Configuration
   ========================== */
// Two LED strips on PORT_5 and PORT_6
MeRGBLed led1(PORT_5);  // Connector 1: 4 LEDs
MeRGBLed led2(PORT_6);  // Connector 2: 4 LEDs

// 3 Arm Motors (robot arm joints, encoder-based)
MeEncoderOnBoard armMotor1(SLOT1);
MeEncoderOnBoard armMotor2(SLOT2);
MeEncoderOnBoard armMotor3(SLOT3);

// Gripper DC motor (no encoder, just open/close)
MeMegaPiDCMotor gripper(PORT4A);

const int MAX_ARM_SPEED = 200;
const int GRIPPER_SPEED = 100;

// LED state
byte colors[2][3] = { {0, 0, 255}, {0, 0, 255} };       // Current
byte targetColors[2][3] = { {0, 0, 255}, {0, 0, 255} };  // For smooth transition
unsigned long lastTransitionTime = 0;
const unsigned long TRANSITION_INTERVAL = 10; // ms

/* ==========================
   Interrupt handlers (3 arm encoder motors)
   ========================== */
void isr_Arm1()
{
  if(digitalRead(armMotor1.getPortB()) == 0)
    armMotor1.pulsePosMinus();
  else
    armMotor1.pulsePosPlus();
}

void isr_Arm2()
{
  if(digitalRead(armMotor2.getPortB()) == 0)
    armMotor2.pulsePosMinus();
  else
    armMotor2.pulsePosPlus();
}

void isr_Arm3()
{
  if(digitalRead(armMotor3.getPortB()) == 0)
    armMotor3.pulsePosMinus();
  else
    armMotor3.pulsePosPlus();
}

/* ==========================
   Arm Motor Control
   ========================== */
MeEncoderOnBoard* getArmMotor(int slot) {
  switch (slot) {
    case 1: return &armMotor1;
    case 2: return &armMotor2;
    case 3: return &armMotor3;
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
  gripper.stop();
}

/* ==========================
   Gripper Control
   ========================== */
void gripperOpen() {
  gripper.run(GRIPPER_SPEED);
}

void gripperClose() {
  gripper.run(-GRIPPER_SPEED);
}

void gripperStop() {
  gripper.stop();
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
      // Arm motor speed: A<slot>,<speed>  (slots 1-3 only)
      int comma = input.indexOf(',');
      if (comma > 0) {
        int slot  = input.substring(1, comma).toInt();
        int speed = input.substring(comma + 1).toInt();
        if (slot >= 1 && slot <= 3) {
          setArmSpeed(slot, speed);
          Serial3.println("S>OK A" + String(slot));
        }
      }
      break;
    }

    case 'G': {
      // Gripper: Go = open, Gc = close, Gs = stop
      if (input.length() > 1) {
        char action = input.charAt(1);
        if (action == 'o') {
          gripperOpen();
          Serial3.println("S>OK Go");
        } else if (action == 'c') {
          gripperClose();
          Serial3.println("S>OK Gc");
        } else if (action == 's') {
          gripperStop();
          Serial3.println("S>OK Gs");
        }
      }
      break;
    }

    case 'x':
      // Stop all arm motors + gripper
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

  // Attach interrupts for 3 arm encoder motors
  attachInterrupt(armMotor1.getIntNum(), isr_Arm1, RISING);
  attachInterrupt(armMotor2.getIntNum(), isr_Arm2, RISING);
  attachInterrupt(armMotor3.getIntNum(), isr_Arm3, RISING);

  //Set PWM 8KHz
  TCCR1A = _BV(WGM10);
  TCCR1B = _BV(CS11) | _BV(WGM12);
  TCCR2A = _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS21);

  // Initialize arm encoder motor parameters
  MeEncoderOnBoard* motors[] = {&armMotor1, &armMotor2, &armMotor3};
  for (int i = 0; i < 3; i++) {
    motors[i]->setPulse(7);
    motors[i]->setRatio(26.9);
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

  // PID loops for 3 arm encoder motors
  armMotor1.loop();
  armMotor2.loop();
  armMotor3.loop();
}
