/*
 * Slave Bluetooth Communication Test
 * 
 * Purpose: Receive text commands from Master via Bluetooth serial module
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
 * Expected commands from Master:
 * - "HELLO" → Echo back "HELLO received"
 * - "ARM100" → Parse and echo "ARM speed: 100"
 * - "STOP" → Echo back "Motor stopped"
 * - "TEST" → Echo back "Test received"
 */

void printStartupInfo() {
  Serial.println("=== Slave Bluetooth Test ===");
  Serial.println("Bluetooth serial initialized at 115200 baud");
  Serial.println("Waiting for commands from Master...");
  Serial.println();
}

void setup() {
  Serial.begin(115200);    // USB Serial for debugging
  Serial3.begin(115200);    // Onboard Bluetooth serial interface

  delay(1000);
  printStartupInfo();
}

void loop() {
  // Check for Bluetooth commands from Master
  if (Serial3.available()) {
    String command = Serial3.readStringUntil('\n');
    command.trim();
    
    if (command.length() > 0) {
      Serial.print("[RECEIVED] ");
      Serial.println(command);
      
      // Process command
      String response = processCommand(command);
      
      // Send response back to Master
      Serial3.println(response);
      Serial.print("[SENT] ");
      Serial.println(response);
    }
  }
}

String processCommand(String cmd) {
  // Simple echo test
  if (cmd == "HELLO") {
    return "HELLO received";
  }
  
  // Arm motor test
  if (cmd.startsWith("ARM")) {
    String speedStr = cmd.substring(3);  // Get everything after "ARM"
    int speed = speedStr.toInt();
    
    String response = "ARM speed set to: ";
    response += speed;
    return response;
  }
  
  // Stop command
  if (cmd == "STOP") {
    return "Motor stopped";
  }
  
  // Test command
  if (cmd == "TEST") {
    return "Test received - Bluetooth OK!";
  }
  
  // Unknown command
  return "Unknown: " + cmd;
}
