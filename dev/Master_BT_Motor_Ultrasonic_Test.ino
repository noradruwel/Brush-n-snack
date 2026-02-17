/*
 * Master Bluetooth + Arm + Ultrasonic Test (Simple)
 *
 * Purpose:
 * - Send simple text commands to Slave over Serial3.
 * - Keep it easy to test from Serial Monitor.
 *
 * Commands:
 * - H, T, O, C, X
 * - A120, A-120
 * - D
 *
 * Command reference:
 * - H      : handshake request
 * - T      : connectivity test request
 * - O      : arm open (default PWM on slave)
 * - C      : arm close (default PWM on slave)
 * - X      : arm stop
 * - A<n>   : arm PWM setpoint, range -255..255
 * - D      : single ultrasonic distance request
 *
 * Transport:
 * - Master sends one uppercase newline-terminated command line over Serial3.
 * - Slave responds with one newline-terminated text line.
 */

/*
 * Prints quick command help to USB Serial Monitor.
 */
void printStartupInfo() {
  Serial.println("=== Master BT Arm/Ultrasonic Test ===");
  Serial.println("Type commands: H T O C X A120 A-120 D");
  Serial.println();
}

/*
 * Setup:
 * - USB serial for monitoring
 * - Serial3 for Bluetooth module
 */
void setup() {
  Serial.begin(115200);
  Serial3.begin(115200);

  delay(1000);
  printStartupInfo();
}

/*
 * Main loop:
 * 1) Read one user command from USB Serial Monitor and forward via Serial3
 * 2) Read one response line from Serial3 and print to USB Serial Monitor
 */
void loop() {
  if (Serial.available()) {
    String userInput = Serial.readStringUntil('\n');
    userInput.trim();
    userInput.toUpperCase();

    if (userInput.length() > 0) {
      Serial3.println(userInput);
      Serial.print("[SENT] ");
      Serial.println(userInput);
    }
  }

  if (Serial3.available()) {
    String response = Serial3.readStringUntil('\n');
    response.trim();

    if (response.length() > 0) {
      Serial.print("[RECEIVED] ");
      Serial.println(response);
    }
  }
}
