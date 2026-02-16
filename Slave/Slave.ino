#include <MeMegaPi.h>
#include <Wire.h>

// Two LED strips on PORT_5 and PORT_6
MeRGBLed led1(PORT_5);  // Connector 1: 4 LEDs
MeRGBLed led2(PORT_6);  // Connector 2: 4 LEDs

// Arm motor configuration
MeEncoderOnBoard motorArm(SLOT1);
const int maxMotorSpeed = 200;

// Current colors for each connector [R, G, B]
byte colors[2][3] = { {0, 0, 255}, {0, 0, 255} }; // Default blue
byte targetColors[2][3] = { {0, 0, 255}, {0, 0, 255} }; // Target colors for smooth transition
unsigned long lastTransitionTime = 0;
const unsigned long TRANSITION_INTERVAL = 10; // milliseconds

/* ==========================
   Interrupt handler for arm motor
   ========================== */
void isr_Arm() { digitalRead(motorArm.getPortB()) ? motorArm.pulsePosPlus() : motorArm.pulsePosMinus(); }

/* ==========================
   Arm Motor Control Functions
   ========================== */
void setArmSpeed(int speed) {
  // Clamp speed to valid range
  speed = constrain(speed, -maxMotorSpeed, maxMotorSpeed);
  motorArm.setTarPWM(speed);
}

void stopArm() {
  motorArm.setTarPWM(0);
}

void setup() {
  Wire.begin(0x08);                // Initialize as Slave with address 0x08
  Wire.onReceive(receiveEvent);    // Attach function for incoming data
  Serial.begin(115200);
  Serial.println("I2C Slave listening on 0x08...");
  
  // Attach arm motor interrupt handler
  attachInterrupt(motorArm.getIntNum(), isr_Arm, RISING);

  // Initialize arm motor parameters
  motorArm.setPulse(360);
  motorArm.setRatio(46.67);
  motorArm.setPosPid(1.8, 0, 1.2);
  motorArm.setSpeedPid(0.18, 0, 0);
  
  // Set default blue color on startup
  setAllLeds(1, 0, 0, 255);
  setAllLeds(2, 0, 0, 255);
}

void loop() {
  updateTransitions();
  
  // PID loop must always run
  motorArm.loop();
}

// Set all 4 LEDs on a connector to the same color
void setAllLeds(byte connector, byte r, byte g, byte b) {
  applyAllLeds(connector, r, g, b);

  Serial.print("Connector ");
  Serial.print(connector);
  Serial.print(" set to RGB: ");
  Serial.print(r); Serial.print(",");
  Serial.print(g); Serial.print(",");
  Serial.println(b);
}

void applyAllLeds(byte connector, byte r, byte g, byte b) {
  MeRGBLed* led = (connector == 1) ? &led1 : &led2;
  
  for (int i = 0; i < 4; i++) {
    led->setColorAt(i, r, g, b);
  }
  led->show();
  
  // Store current color
  colors[connector - 1][0] = r;
  colors[connector - 1][1] = g;
  colors[connector - 1][2] = b;
}

void setTargetColor(byte connector, byte r, byte g, byte b) {
  targetColors[connector - 1][0] = r;
  targetColors[connector - 1][1] = g;
  targetColors[connector - 1][2] = b;
}

void updateTransitions() {
  unsigned long currentTime = millis();
  if (currentTime - lastTransitionTime < TRANSITION_INTERVAL) {
    return;
  }
  lastTransitionTime = currentTime;

  bool updated = false;

  for (int connector = 0; connector < 2; connector++) {
    for (int c = 0; c < 3; c++) {
      if (colors[connector][c] < targetColors[connector][c]) {
        colors[connector][c]++;
        updated = true;
      } else if (colors[connector][c] > targetColors[connector][c]) {
        colors[connector][c]--;
        updated = true;
      }
    }
  }

  if (updated) {
    applyAllLeds(1, colors[0][0], colors[0][1], colors[0][2]);
    applyAllLeds(2, colors[1][0], colors[1][1], colors[1][2]);
  }
}

// This function is called when the Master sends data
// Expected format for LEDs: 'L', connector (1 or 2), R, G, B
// Expected format for Arm Motor: 'M', speed (2 bytes, int16_t for -200 to 200)
void receiveEvent(int howMany) {
  if (howMany < 2) {
    while (Wire.available()) Wire.read();
    return;
  }

  char cmd = Wire.read();
  
  if (cmd == 'L' && howMany >= 5) {
    // LED command
    byte connector = Wire.read();
    byte r = Wire.read();
    byte g = Wire.read();
    byte b = Wire.read();
    
    if (connector == 1 || connector == 2) {
      setTargetColor(connector, r, g, b);
    }
    
    // Clear any remaining bytes
    while (Wire.available()) {
      Wire.read();
    }
  } else if (cmd == 'M' && howMany >= 2) {
    // Arm motor command - read speed (2 bytes, int16_t)
    byte speedBytes[2] = {0, 0};
    for (int i = 0; i < 2 && Wire.available(); i++) {
      speedBytes[i] = Wire.read();
    }

    int16_t speed = 0;
    if (howMany >= 3) {
      // Reconstruct int16_t from bytes
      memcpy(&speed, speedBytes, sizeof(speed));
    }

    // Set arm motor speed
    setArmSpeed(speed);
    
    // Clear any remaining bytes
    while (Wire.available()) {
      Wire.read();
    }
  } else {
    // Clear any remaining bytes
    while (Wire.available()) {
      Wire.read();
    }
  }
}
