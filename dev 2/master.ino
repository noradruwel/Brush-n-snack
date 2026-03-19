    #include <MeMegaPi.h>

    /*
    Standalone master sketch:
    - Controls drive + LEDs only.
    - Accepts commands directly from Serial3 (BT) and Serial (USB).
    */

    // ===== Hardware =====
    MeEncoderOnBoard motor_L1(SLOT2);  // Left front
    MeEncoderOnBoard motor_L2(SLOT1);  // Left rear
    MeEncoderOnBoard motor_R1(SLOT4);  // Right front
    MeEncoderOnBoard motor_R2(SLOT3);  // Right rear
    MeRGBLed led1(PORT_5);             // Left strip (4 LEDs)
    MeRGBLed led2(PORT_8);             // Right strip (4 LEDs)

    // ===== Constants =====
    const long SERIAL_BAUD = 115200;
    const int DRIVE_SPEED = 300;
    const int NUM_LEDS = 4;
    const unsigned long TURN_FADE_MS = 12;
    const int TURN_STEP = 8;
    const int TURN_MIN_BRIGHTNESS = 20;
    const unsigned long LED_FADE_MS = 30;
    const unsigned long RAINBOW_STEP_MS_DEFAULT = 25;

    // ===== State =====
    long posL = 0;
    long posR = 0;

    bool turningLeft = false;
    bool turningRight = false;
    int turnBrightness = 0;
    int turnDir = 1;
    unsigned long lastTurnFade = 0;

    byte turnColor[3] = {255, 170, 0};
    byte defaultColor[3] = {0, 0, 255};
    bool rainbowEnabled = false;
    byte rainbowHue = 0;
    unsigned long rainbowStepMs = RAINBOW_STEP_MS_DEFAULT;
    unsigned long lastRainbowStep = 0;

    byte curColor[2][3] = {{0, 0, 255}, {0, 0, 255}};
    byte tgtColor[2][3] = {{0, 0, 255}, {0, 0, 255}};
    unsigned long lastLedFade = 0;

    MeEncoderOnBoard *driveMotors[] = {&motor_L1, &motor_L2, &motor_R1, &motor_R2};

    // ===== Encoder interrupts =====
    void isr_L1() { digitalRead(motor_L1.getPortB()) == 0 ? motor_L1.pulsePosMinus() : motor_L1.pulsePosPlus(); }
    void isr_L2() { digitalRead(motor_L2.getPortB()) == 0 ? motor_L2.pulsePosMinus() : motor_L2.pulsePosPlus(); }
    void isr_R1() { digitalRead(motor_R1.getPortB()) == 0 ? motor_R1.pulsePosMinus() : motor_R1.pulsePosPlus(); }
    void isr_R2() { digitalRead(motor_R2.getPortB()) == 0 ? motor_R2.pulsePosMinus() : motor_R2.pulsePosPlus(); }

    // ===== Utility helpers =====
    long parseLongSuffix(const String &s, int start, long defaultValue = 0) {
        if (s.length() <= start) return defaultValue;
        return atol(s.substring(start).c_str());
    }

    bool parseRgb(const String &s, int start, byte out[3]) {
        int p1 = s.indexOf(',', start);
        int p2 = (p1 >= 0) ? s.indexOf(',', p1 + 1) : -1;
        if (p1 < 0 || p2 < 0) return false;
        out[0] = (byte)s.substring(start, p1).toInt();
        out[1] = (byte)s.substring(p1 + 1, p2).toInt();
        out[2] = (byte)s.substring(p2 + 1).toInt();
        return true;
    }

    void replyMaster(const String &msg) {
        Serial3.println(msg);
    }

    // ===== LED helpers =====
    void applyLed(byte side, byte r, byte g, byte b) {
        MeRGBLed &led = (side == 1) ? led1 : led2;
        for (int i = 0; i < NUM_LEDS; i++) {
            led.setColorAt(i, r, g, b);
        }
        led.show();

        curColor[side - 1][0] = r;
        curColor[side - 1][1] = g;
        curColor[side - 1][2] = b;
    }

    void setLedTarget(byte side, byte r, byte g, byte b) {
        tgtColor[side - 1][0] = r;
        tgtColor[side - 1][1] = g;
        tgtColor[side - 1][2] = b;
    }

    void setLedImmediate(byte side, byte r, byte g, byte b) {
        applyLed(side, r, g, b);
        setLedTarget(side, r, g, b);
    }

    byte scaleByBrightness(byte base, int brightness) {
        return (byte)(((int)base * brightness) / 255);
    }

    void wheelColor(byte pos, byte &r, byte &g, byte &b) {
        if (pos < 85) {
            r = 255 - pos * 3;
            g = pos * 3;
            b = 0;
        } else if (pos < 170) {
            pos -= 85;
            r = 0;
            g = 255 - pos * 3;
            b = pos * 3;
        } else {
            pos -= 170;
            r = pos * 3;
            g = 0;
            b = 255 - pos * 3;
        }
    }

    void getCruiseColor(byte side, byte &r, byte &g, byte &b) {
        if (rainbowEnabled) {
            byte hue = rainbowHue + ((side == 1) ? 0 : 64);
            wheelColor(hue, r, g, b);
            return;
        }
        r = defaultColor[0];
        g = defaultColor[1];
        b = defaultColor[2];
    }

    void setCruiseColorTargets() {
        byte r1, g1, b1;
        byte r2, g2, b2;
        getCruiseColor(1, r1, g1, b1);
        getCruiseColor(2, r2, g2, b2);
        setLedTarget(1, r1, g1, b1);
        setLedTarget(2, r2, g2, b2);
    }

    void updateLedFades() {
        if (turningLeft || turningRight) return;

        if (rainbowEnabled && (millis() - lastRainbowStep >= rainbowStepMs)) {
            lastRainbowStep = millis();
            rainbowHue++;
            setCruiseColorTargets();
        }

        if (millis() - lastLedFade < LED_FADE_MS) return;
        lastLedFade = millis();

        bool changed = false;
        for (int side = 0; side < 2; side++) {
            for (int c = 0; c < 3; c++) {
                if (curColor[side][c] < tgtColor[side][c]) {
                    curColor[side][c]++;
                    changed = true;
                } else if (curColor[side][c] > tgtColor[side][c]) {
                    curColor[side][c]--;
                    changed = true;
                }
            }
        }

        if (changed) {
            applyLed(1, curColor[0][0], curColor[0][1], curColor[0][2]);
            applyLed(2, curColor[1][0], curColor[1][1], curColor[1][2]);
        }
    }

    void updateTurnSignals(bool force) {
        if (!force && !turningLeft && !turningRight) return;

        if (turningLeft || turningRight) {
            unsigned long now = millis();
            if (!force && (now - lastTurnFade < TURN_FADE_MS)) return;

            lastTurnFade = now;
            turnBrightness += turnDir * TURN_STEP;
            if (turnBrightness >= 255) {
                turnBrightness = 255;
                turnDir = -1;
            }
            if (turnBrightness <= TURN_MIN_BRIGHTNESS) {
                turnBrightness = TURN_MIN_BRIGHTNESS;
                turnDir = 1;
            }
        }

        if (turningLeft) {
            byte baseR, baseG, baseB;
            getCruiseColor(2, baseR, baseG, baseB);
            setLedImmediate(1,
                                            scaleByBrightness(turnColor[0], turnBrightness),
                                            scaleByBrightness(turnColor[1], turnBrightness),
                                            scaleByBrightness(turnColor[2], turnBrightness));
            setLedImmediate(2, baseR, baseG, baseB);
            return;
        }

        if (turningRight) {
            byte baseR, baseG, baseB;
            getCruiseColor(1, baseR, baseG, baseB);
            setLedImmediate(1, baseR, baseG, baseB);
            setLedImmediate(2,
                                            scaleByBrightness(turnColor[0], turnBrightness),
                                            scaleByBrightness(turnColor[1], turnBrightness),
                                            scaleByBrightness(turnColor[2], turnBrightness));
            return;
        }

        // Returning to cruise mode after a turn resets pulse state.
        turnBrightness = 0;
        turnDir = 1;
        setCruiseColorTargets();
        setLedImmediate(1, tgtColor[0][0], tgtColor[0][1], tgtColor[0][2]);
        setLedImmediate(2, tgtColor[1][0], tgtColor[1][1], tgtColor[1][2]);
    }

    // ===== Drive helpers =====
    void commandDriveTargets(long leftTarget, long rightTarget, int speed) {
        posL = leftTarget;
        posR = rightTarget;
        motor_L1.moveTo(posL, speed);
        motor_L2.moveTo(posL, speed);
        motor_R1.moveTo(posR, speed);
        motor_R2.moveTo(posR, speed);
    }

    void moveDrive(long delta, int speed) {
        turningLeft = false;
        turningRight = false;
        updateTurnSignals(true);

        // Right side position is mirrored to keep forward motion aligned.
        commandDriveTargets(posL + delta, posR - delta, speed);
    }

    void turnDrive(long delta, int speed) {
        turningLeft = (delta < 0);
        turningRight = (delta > 0);
        updateTurnSignals(true);
        commandDriveTargets(posL - delta, posR - delta, speed);
    }

    void stopDrive() {
        turningLeft = false;
        turningRight = false;
        updateTurnSignals(true);
        motor_L1.setMotorPwm(0);
        motor_L2.setMotorPwm(0);
        motor_R1.setMotorPwm(0);
        motor_R2.setMotorPwm(0);
    }

    // ===== Command processing =====
    // Supported:
    // F<pos>, B<pos>, L[pos], R[pos], x
    // N<R>,<G>,<B> (default cruise color)
    // T<R>,<G>,<B> (turn indicator color)
    // Q<0|1> (rainbow off/on), P<ms> (rainbow speed, min 5)
    // Optional prefix accepted: M:
    void processCommand(const String &rawInput) {
        String in = rawInput;
        if (in.startsWith("M:")) in = in.substring(2);
        in.trim();
        if (in.length() == 0) return;

        const char cmd = in.charAt(0);

        if (cmd == 'F') {
            long dist = parseLongSuffix(in, 1, 0);
            moveDrive(dist, DRIVE_SPEED);
            replyMaster("M>OK F" + String(dist));
            return;
        }

        if (cmd == 'B') {
            long dist = parseLongSuffix(in, 1, 0);
            moveDrive(-dist, DRIVE_SPEED);
            replyMaster("M>OK B" + String(dist));
            return;
        }

        if (cmd == 'L') {
            long amount = parseLongSuffix(in, 1, 0);
            turnDrive((amount == 0) ? -360 : -amount, DRIVE_SPEED);
            replyMaster("M>OK L");
            return;
        }

        if (cmd == 'R') {
            long amount = parseLongSuffix(in, 1, 0);
            turnDrive((amount == 0) ? 360 : amount, DRIVE_SPEED);
            replyMaster("M>OK R");
            return;
        }

        if (cmd == 'x') {
            stopDrive();
            replyMaster("M>OK STOP");
            return;
        }

        if (cmd == 'N') {
            byte rgb[3];
            if (!parseRgb(in, 1, rgb)) return;
            defaultColor[0] = rgb[0];
            defaultColor[1] = rgb[1];
            defaultColor[2] = rgb[2];
            if (!rainbowEnabled && !turningLeft && !turningRight) {
                setCruiseColorTargets();
            }
            replyMaster("M>OK N" + String(defaultColor[0]) + "," + String(defaultColor[1]) + "," + String(defaultColor[2]));
            return;
        }

        if (cmd == 'T') {
            byte rgb[3];
            if (!parseRgb(in, 1, rgb)) return;
            turnColor[0] = rgb[0];
            turnColor[1] = rgb[1];
            turnColor[2] = rgb[2];
            updateTurnSignals(true);
            replyMaster("M>OK T" + String(turnColor[0]) + "," + String(turnColor[1]) + "," + String(turnColor[2]));
            return;
        }

        if (cmd == 'Q') {
            rainbowEnabled = (parseLongSuffix(in, 1, 0) != 0);
            if (!turningLeft && !turningRight) {
                setCruiseColorTargets();
            }
            replyMaster("M>OK Q" + String(rainbowEnabled ? 1 : 0));
            return;
        }

        if (cmd == 'P') {
            long step = parseLongSuffix(in, 1, (long)RAINBOW_STEP_MS_DEFAULT);
            if (step < 5) step = 5;
            rainbowStepMs = (unsigned long)step;
            replyMaster("M>OK P" + String((long)rainbowStepMs));
        }
    }

    void initPidAndTimers() {
        // Timer setup expected by Makeblock encoder control.
        TCCR1A = _BV(WGM10);
        TCCR1B = _BV(CS11) | _BV(WGM12);
        TCCR2A = _BV(WGM21) | _BV(WGM20);
        TCCR2B = _BV(CS21);

        for (int i = 0; i < 4; i++) {
            driveMotors[i]->setPulse(7);
            driveMotors[i]->setRatio(26.9);
            driveMotors[i]->setPosPid(1.8, 0, 1.2);
            driveMotors[i]->setSpeedPid(0.18, 0, 0);
        }
    }

    void pollCommandStream(Stream &stream) {
        if (!stream.available()) return;
        String line = stream.readStringUntil('\n');
        line.trim();
        processCommand(line);
    }

    void setup() {
        Serial.begin(SERIAL_BAUD);
        Serial3.begin(SERIAL_BAUD);
        Serial3.setTimeout(10);

        attachInterrupt(motor_L1.getIntNum(), isr_L1, RISING);
        attachInterrupt(motor_L2.getIntNum(), isr_L2, RISING);
        attachInterrupt(motor_R1.getIntNum(), isr_R1, RISING);
        attachInterrupt(motor_R2.getIntNum(), isr_R2, RISING);

        initPidAndTimers();

        setLedImmediate(1, defaultColor[0], defaultColor[1], defaultColor[2]);
        setLedImmediate(2, defaultColor[0], defaultColor[1], defaultColor[2]);
        Serial.println("Standalone Master ready");
    }

    void loop() {
        pollCommandStream(Serial3);
        pollCommandStream(Serial);

        updateTurnSignals(false);
        updateLedFades();

        // Required for closed-loop encoder motor control.
        for (int i = 0; i < 4; i++) {
            driveMotors[i]->loop();
        }
    }
    motor_L1.loop();
    motor_L2.loop();
    motor_R1.loop();
    motor_R2.loop();

    if (turningLeft || turningRight) {
        if (abs(motor_L1.getCurPos() - posL) < 20 &&
            abs(motor_R1.getCurPos() - posR) < 20) {
        turningLeft = false;
        turningRight = false;
        updateTurnSignals(true);
        }
    }
    }
