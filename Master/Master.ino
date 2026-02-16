#include <MeMegaPi.h>
#include <Wire.h>

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
const int ARM_SPEED_LIMIT = 200;

// LED colors for each connector: [R, G, B]
byte ledColors[2][3] = { {0, 0, 255}, {0, 0, 255} }; // Default blue
bool turningLeft = false;
bool turningRight = false;
unsigned long lastFlickerTime = 0;
bool flickerState = false;
const unsigned long FLICKER_INTERVAL = 200; // milliseconds

/* ==========================
   Interrupt handlers
   ========================== */
void isr_L1() { digitalRead(motor_L1.getPortB()) ? motor_L1.pulsePosPlus() : motor_L1.pulsePosMinus(); }
void isr_L2() { digitalRead(motor_L2.getPortB()) ? motor_L2.pulsePosPlus() : motor_L2.pulsePosMinus(); }
void isr_R1() { digitalRead(motor_R1.getPortB()) ? motor_R1.pulsePosPlus() : motor_R1.pulsePosMinus(); }
void isr_R2() { digitalRead(motor_R2.getPortB()) ? motor_R2.pulsePosPlus() : motor_R2.pulsePosMinus(); }

/* ==========================
   LED Communication (I2C)
   ========================== */
void sendLedColor(byte connector, byte r, byte g, byte b) {
  Wire.beginTransmission(0x08); // Slave address
  Wire.write('L');
  Wire.write(connector); // 1 or 2
  Wire.write(r);
  Wire.write(g);
  Wire.write(b);
  Wire.endTransmission();
}

void sendArmSpeedCommand(int armSpeed) {
  Wire.beginTransmission(0x08); // Slave address
  Wire.write('M');

  int16_t speed = constrain(armSpeed, -ARM_SPEED_LIMIT, ARM_SPEED_LIMIT);
  byte* speedBytes = (byte*)&speed;
  for (unsigned int i = 0; i < sizeof(speed); i++) {
    Wire.write(speedBytes[i]);
  }

  Wire.endTransmission();
}

void updateLeds() {
  // Set colors based on turning state
  if (turningLeft || turningRight) {
    // Blink yellow while turning
    unsigned long currentTime = millis();
    if (currentTime - lastFlickerTime >= FLICKER_INTERVAL) {
      flickerState = !flickerState;
      lastFlickerTime = currentTime;
    }
    
    if (turningLeft) {
      if (flickerState) {
        ledColors[0][0] = 255; ledColors[0][1] = 255; ledColors[0][2] = 0;
      } else {
        ledColors[0][0] = 0; ledColors[0][1] = 0; ledColors[0][2] = 0;
      }
      ledColors[1][0] = 0; ledColors[1][1] = 0; ledColors[1][2] = 255;
    } else if (turningRight) {
      if (flickerState) {
        ledColors[1][0] = 255; ledColors[1][1] = 255; ledColors[1][2] = 0;
      } else {
        ledColors[1][0] = 0; ledColors[1][1] = 0; ledColors[1][2] = 0;
      }
      ledColors[0][0] = 0; ledColors[0][1] = 0; ledColors[0][2] = 255;
    }
  } else {
    // Blue when not turning
    ledColors[0][0] = 0; ledColors[0][1] = 0; ledColors[0][2] = 255;
    ledColors[1][0] = 0; ledColors[1][1] = 0; ledColors[1][2] = 255;
  }

  // Send color commands to slave
  sendLedColor(1, ledColors[0][0], ledColors[0][1], ledColors[0][2]);
  sendLedColor(2, ledColors[1][0], ledColors[1][1], ledColors[1][2]);
}

/* ==========================
   Motion Logic
   ========================== */
void moveMeters(float meters) {
  turningLeft = false;
  turningRight = false;
  updateLeds();

  long targetPulses = (meters * 1000.0 / (WHEEL_DIAMETER * PI_VALUE)) * ENCODER_PULSES * REDUCTION_RATIO;

  motor_L1.move(-targetPulses, DRIVE_SPEED);
  motor_L2.move(-targetPulses, DRIVE_SPEED);
  motor_R1.move(targetPulses, DRIVE_SPEED);
  motor_R2.move(targetPulses, DRIVE_SPEED);
}

void rotateDegrees(float degrees) {
  // Update turning state for LED behavior
  turningLeft = (degrees < 0);
  turningRight = (degrees > 0);
  updateLeds();

  float arcLength = (abs(degrees) * PI_VALUE * TRACK_WIDTH) / 360.0;
  long targetPulses = (arcLength / (WHEEL_DIAMETER * PI_VALUE)) * ENCODER_PULSES * REDUCTION_RATIO;

  if (degrees < 0) {
    // Turn left
    motor_L1.move(targetPulses, DRIVE_SPEED);
    motor_L2.move(targetPulses, DRIVE_SPEED);
    motor_R1.move(targetPulses, DRIVE_SPEED);
    motor_R2.move(targetPulses, DRIVE_SPEED);
  } else {
    // Turn right
    motor_L1.move(-targetPulses, DRIVE_SPEED);
    motor_L2.move(-targetPulses, DRIVE_SPEED);
    motor_R1.move(-targetPulses, DRIVE_SPEED);
    motor_R2.move(-targetPulses, DRIVE_SPEED);
  }
}

void stop() {
  turningLeft = false;
  turningRight = false;
  updateLeds();
  
  motor_L1.setTarPWM(0); motor_L2.setTarPWM(0);
  motor_R1.setTarPWM(0); motor_R2.setTarPWM(0);
}

/* ==========================
   Setup
   ========================== */
void setup() {
  Serial.begin(115200);
  Serial3.begin(115200);
  Wire.begin(); // I2C Master
  
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

  // Send default blue color to LEDs on startup
  updateLeds();
}

/* ==========================
   Main Loop
   ========================== */
void loop() {
  // Update LEDs continuously for flickering effect
  if (turningLeft || turningRight) {
    updateLeds();
  }

  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() > 0) {
      char cmd = input.charAt(0);
      
      float val = 0;
      if (input.length() > 1) {
        val = input.substring(1).toFloat(); 
      }

      switch (cmd) {
        case 'F': // Forward X meters
          moveMeters(val);
          break;
        case 'B': // Backward X meters
          moveMeters(-val);
          break;
        case 'L': // Rotate Left (90 default or custom)
          rotateDegrees(val == 0 ? -90 : -val);
          break;
        case 'R': // Rotate Right (90 default or custom)
          rotateDegrees(val == 0 ? 90 : val);
          break;
        case 'x': // Immediate stop
          stop();
          break;
        case 'C': // Configure LED color: C1,R,G,B or C2,R,G,B
          if (input.length() > 2) {
            int connector = input.charAt(1) - '0';
            int firstComma = input.indexOf(',');
            int secondComma = input.indexOf(',', firstComma + 1);
            int thirdComma = input.indexOf(',', secondComma + 1);
            if ((connector == 1 || connector == 2) && firstComma > 0 && secondComma > firstComma && thirdComma > secondComma) {
              byte r = input.substring(firstComma + 1, secondComma).toInt();
              byte g = input.substring(secondComma + 1, thirdComma).toInt();
              byte b = input.substring(thirdComma + 1).toInt();
              sendLedColor(connector, r, g, b);
              Serial.print("Set LED "); Serial.print(connector);
              Serial.print(" to RGB: "); Serial.print(r); Serial.print(","); Serial.print(g); Serial.print(","); Serial.println(b);
            }
          }
          break;
        case 'A': // Control Slave Arm Motor: A<speed> (-200 to 200)
          if (input.length() > 1) {
            int armSpeed = input.substring(1).toInt();
            sendArmSpeedCommand(armSpeed);
            Serial.print("Sent arm motor speed: ");
            Serial.println(armSpeed);
          }
          break;
      }
    }
  }

  // PID loop must always run
  motor_L1.loop();
  motor_L2.loop();
  motor_R1.loop();
  motor_R2.loop();
}
