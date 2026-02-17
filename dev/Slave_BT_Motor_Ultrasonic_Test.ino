/*
 * Slave Bluetooth + Arm Motor (Open/Close) + Ultrasonic Test
 *
 * Purpose:
 * - Receive Bluetooth commands from Master.
 * - Control the arm motor as OPEN/CLOSE/STOP using MeMegaPi motor interface.
 * - Provide basic ultrasonic distance support for future expansion.
 *
 * Upload target: Slave board
 * Serial monitor baud: 115200
 * Bluetooth serial: Serial3 @ 115200
 *
 * Command protocol over Serial3 (text, one line per command):
 * - H             -> handshake reply
 * - T             -> health reply
 * - O             -> arm open direction (fixed PWM)
 * - C             -> arm close direction (fixed PWM)
 * - X             -> arm stop
 * - A<number>     -> direct arm PWM (-255..255)
 * - D             -> one distance sample in cm
 *
 * Notes:
 * - Ultrasonic uses MeUltrasonicSensor on RJ25 port (ULTRASONIC_PORT).
 * - Change ULTRASONIC_PORT to match your hardware connection.
 * - Arm control follows Makeblock PWM examples: setTarPWM(...) + armMotor.loop().
 * - No setPulse/setRatio/PID tuning is required for this simple PWM control test.
 */

#include <MeMegaPi.h>

MeEncoderOnBoard armMotor(SLOT1);
const int ARM_DEFAULT_PWM = 140;

const uint8_t ULTRASONIC_PORT = PORT_7;
MeUltrasonicSensor ultrasonicSensor(ULTRASONIC_PORT);

int currentArmPwm = 0;

/*
 * Prints startup information to USB Serial Monitor.
 */
void printStartupInfo() {
  Serial.println("=== Slave BT Motor/Ultrasonic Test ===");
  Serial.println("Waiting for Bluetooth commands...");
  Serial.println("Supported cmds: H,T,O,C,X,A<n>,D");
  Serial.println();
}

/*
 * Clamps any requested PWM value to the valid MeEncoderOnBoard range.
 */
int clampPwm(int pwmValue) {
  if (pwmValue > 255) {
    return 255;
  }
  if (pwmValue < -255) {
    return -255;
  }
  return pwmValue;
}

/*
 * Applies target PWM to the arm motor and stores last applied value.
 */
void setArmPwm(int pwmValue) {
  currentArmPwm = clampPwm(pwmValue);
  armMotor.setTarPWM(currentArmPwm);
}

/*
 * Convenience helper: open direction at default PWM.
 */
void openArm() {
  setArmPwm(ARM_DEFAULT_PWM);
}

/*
 * Convenience helper: close direction at default PWM.
 */
void closeArm() {
  setArmPwm(-ARM_DEFAULT_PWM);
}

/*
 * Stops arm motor immediately.
 */
void stopArm() {
  setArmPwm(0);
}

/*
 * Reads distance from Makeblock ultrasonic sensor object.
 * Returns centimeters as long for easy text formatting.
 */
long readDistanceCm() {
  double distanceValue = ultrasonicSensor.distanceCm();
  return (long)distanceValue;
}

/*
 * Sends one line response over Bluetooth and mirrors it to USB Serial.
 */
void sendTextResponse(String message) {
  Serial3.println(message);
  Serial.print("[SENT] ");
  Serial.println(message);
}

/*
 * Parses one text command and returns response text.
 * Supported commands:
 * - H, T, O, C, X, D
 * - A<number>  (example: A120, A-80)
 */
String processCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  if (cmd.length() == 0) {
    return "";
  }

  if (cmd == "H") {
    return "HELLO received";
  }

  if (cmd == "T") {
    return "Test received - BT OK";
  }

  if (cmd == "O") {
    openArm();
    return "OK OPEN";
  }

  if (cmd == "C") {
    closeArm();
    return "OK CLOSE";
  }

  if (cmd == "X") {
    stopArm();
    return "OK STOP";
  }

  if (cmd == "D") {
    long distanceCm = readDistanceCm();
    String reply = "DIST ";
    reply += distanceCm;
    return reply;
  }

  if (cmd.startsWith("A")) {
    int requested = 0;
    if (cmd.length() > 1) {
      requested = cmd.substring(1).toInt();
    }

    setArmPwm(requested);

    String reply = "OK ARM ";
    reply += currentArmPwm;
    return reply;
  }

  return "ERR Unknown: " + cmd;
}

/*
 * Setup sequence:
 * 1) Attach encoder interrupt callback
 * 2) Stop motor for safe startup
 * 3) Start USB + Bluetooth serial
 */
void setup() {
  attachInterrupt(armMotor.getIntNum(), isr_Arm, RISING);

  stopArm();

  Serial.begin(115200);
  Serial3.begin(115200);

  delay(1000);
  printStartupInfo();
}

/*
 * Encoder interrupt service routine for arm motor.
 */
void isr_Arm() {
  digitalRead(armMotor.getPortB()) ? armMotor.pulsePosPlus() : armMotor.pulsePosMinus();
}

/*
 * Reads one newline-terminated command from Bluetooth and processes it.
 */
void handleIncomingBluetooth() {
  if (!Serial3.available()) {
    return;
  }

  String command = Serial3.readStringUntil('\n');
  command.trim();
  command.toUpperCase();

  if (command.length() == 0) {
    return;
  }

  Serial.print("[RECEIVED] ");
  Serial.println(command);

  String response = processCommand(command);
  if (response.length() > 0) {
    sendTextResponse(response);
  }
}

/*
 * Main loop:
 * - process incoming Bluetooth commands
 * - run motor control loop continuously
 */
void loop() {
  handleIncomingBluetooth();
  armMotor.loop();
}
