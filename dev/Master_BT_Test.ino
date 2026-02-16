/*
 * Master Bluetooth Communication Test
 * 
 * Purpose: Send simple text commands to Slave via Bluetooth serial module
 * Hardware: MegaPi with onboard Bluetooth module (no external wiring required)
 * 
 * Sources:
 * - Arduino Serial Documentation: https://www.arduino.cc/reference/en/language/functions/communication/serial/
 * - Bridge User Guide: https://sites.google.com/view/communication-utilities/bridge-user-guide
 * - MeMegaPi Wiki: https://github.com/Makeblock-official/MeMegaPi
 * 
 * Wiring:
 * - No external Bluetooth wiring needed.
 * - The board's onboard Bluetooth interface is used directly.
 * 
 * Commands to send (via Serial Monitor):
 * - "HELLO" → sends "HELLO" to Slave
 * - "ARM100" → sends "ARM100" (example for arm speed 100)
 * - "STOP" → sends "STOP"
 * - "TEST" → sends "TEST"
 */

void printStartupInfo() {
  Serial.println("=== Master Bluetooth Test ===");
  Serial.println("Bluetooth serial initialized at 115200 baud");
  Serial.println("Type commands in Serial Monitor to send via Bluetooth");
  Serial.println();
}

void setup() {
  Serial.begin(115200);    // USB Serial for debugging
  Serial3.begin(115200);    // Onboard Bluetooth serial interface

  delay(1000);
  printStartupInfo();
}

void loop() {
  // Check for USB Serial input (from user typing in Monitor)
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.length() > 0) {
      // Send to Bluetooth
      Serial3.println(command);
      
      Serial.print("[SENT] ");
      Serial.println(command);
    }
  }

  // Check for Bluetooth response from Slave
  if (Serial3.available()) {
    String response = Serial3.readStringUntil('\n');
    response.trim();
    
    if (response.length() > 0) {
      Serial.print("[RECEIVED] ");
      Serial.println(response);
    }
  }
}
