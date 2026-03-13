#include <MeMegaPi.h>

/*
 * Master LED Fade Test (standalone)
 *
 * Purpose:
 * - Verify smooth RGB fading on MegaPi LED strips without motor/turn logic.
 * - Show obvious intermediate brightness levels (no jumpy on/off effect).
 *
 * Hardware:
 * - LED strip 1 on PORT_5
 * - LED strip 2 on PORT_8
 */

MeRGBLed led1(PORT_5);
MeRGBLed led2(PORT_8);

const int NUM_LEDS = 4;
const int STEP_DELAY_MS = 35;  // bigger delay makes transitions easier to see
const int STEP_SIZE = 1;       // 1 gives maximum smoothness

void fillBoth(byte r, byte g, byte b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    led1.setColorAt(i, r, g, b);
    led2.setColorAt(i, r, g, b);
  }
  led1.show();
  led2.show();
}

void setup() {
  Serial.begin(115200);
  fillBoth(0, 0, 0);
  Serial.println("Master LED Fade Test ready");
}

void loop() {
  // Cycle 1: blue breath (0 -> 255 -> 0)
  for (int b = 0; b <= 255; b += STEP_SIZE) {
    fillBoth(0, 0, (byte)b);
    delay(STEP_DELAY_MS);
  }
  for (int b = 255; b >= 0; b -= STEP_SIZE) {
    fillBoth(0, 0, (byte)b);
    delay(STEP_DELAY_MS);
  }

  // Cycle 2: green breath (0 -> 255 -> 0)
  for (int g = 0; g <= 255; g += STEP_SIZE) {
    fillBoth(0, (byte)g, 0);
    delay(STEP_DELAY_MS);
  }
  for (int g = 255; g >= 0; g -= STEP_SIZE) {
    fillBoth(0, (byte)g, 0);
    delay(STEP_DELAY_MS);
  }

  // Cycle 3: purple mix fade in/out to test mixed-color smoothness
  for (int x = 0; x <= 255; x += STEP_SIZE) {
    fillBoth((byte)(x / 2), 0, (byte)x);
    delay(STEP_DELAY_MS);
  }
  for (int x = 255; x >= 0; x -= STEP_SIZE) {
    fillBoth((byte)(x / 2), 0, (byte)x);
    delay(STEP_DELAY_MS);
  }
}
