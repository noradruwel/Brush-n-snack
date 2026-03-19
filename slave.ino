    #include <MeMegaPi.h>

    /*
    Standalone slave sketch:
    - Controls arm + gripper only.
    - Accepts commands directly from Serial3 (BT) and Serial (USB).
    */

    // ===== Hardware =====
    MeEncoderOnBoard arm1(SLOT1);  // Base rotation
    MeEncoderOnBoard arm2(SLOT2);  // Shoulder
    MeEncoderOnBoard arm3(SLOT3);  // Elbow
    MeMegaPiDCMotor gripper(PORT4A);

    // ===== Constants =====
    const long SERIAL_BAUD = 115200;
    const int ARM_SPEED = 200;
    const int GRIPPER_SPEED = 100;
    const unsigned long GRIPPER_MAX_RUN_MS = 5000;

    // ===== State =====
    bool gripperRunning = false;
    unsigned long gripperStartMs = 0;
    long armTargets[3] = {0, 0, 0};

    MeEncoderOnBoard *arms[] = {&arm1, &arm2, &arm3};

    // ===== Encoder interrupts =====
    void isr_Arm1() { digitalRead(arm1.getPortB()) == 0 ? arm1.pulsePosMinus() : arm1.pulsePosPlus(); }
    void isr_Arm2() { digitalRead(arm2.getPortB()) == 0 ? arm2.pulsePosMinus() : arm2.pulsePosPlus(); }
    void isr_Arm3() { digitalRead(arm3.getPortB()) == 0 ? arm3.pulsePosMinus() : arm3.pulsePosPlus(); }

    // ===== Utility helpers =====
    bool isValidArmSlot(int slot) {
        return slot >= 1 && slot <= 3;
    }

    void replySlave(const String &msg) {
        Serial3.println(msg);
    }

    // ===== Arm / gripper control =====
    void runGripper(int speed) {
        gripper.run(speed);
        gripperRunning = true;
        gripperStartMs = millis();
    }

    void gripperOpen() {
        runGripper(-GRIPPER_SPEED);
    }

    void gripperClose() {
        runGripper(GRIPPER_SPEED);
    }

    void gripperStop() {
        gripper.stop();
        gripperRunning = false;
    }

    void armMoveTo(int slot, long targetPos, int speed) {
        if (!isValidArmSlot(slot)) return;
        armTargets[slot - 1] = targetPos;
        arms[slot - 1]->moveTo(targetPos, (float)speed);
    }

    void armMoveBy(int slot, long delta, int speed) {
        if (!isValidArmSlot(slot)) return;
        armMoveTo(slot, armTargets[slot - 1] + delta, speed);
    }

    void stopAll() {
        for (int i = 0; i < 3; i++) {
            arms[i]->setMotorPwm(0);
        }
        gripperStop();
    }

    // ===== Command processing =====
    // Supported:
    // A<slot><delta>   (example: A1100, A2-150)
    // Go, Gc, Gs
    // x
    // Optional prefix accepted: S:
    void processCommand(const String &rawInput) {
        String in = rawInput;
        if (in.startsWith("S:")) in = in.substring(2);
        in.trim();
        if (in.length() == 0) return;

        const char cmd = in.charAt(0);

        if (cmd == 'A') {
            if (in.length() < 3) return;
            int slot = in.charAt(1) - '0';
            if (!isValidArmSlot(slot)) return;
            long delta = atol(in.substring(2).c_str());
            armMoveBy(slot, delta, ARM_SPEED);
            replySlave("S>OK A" + String(slot));
            return;
        }

        if (cmd == 'G' && in.length() > 1) {
            const char action = in.charAt(1);
            if (action == 'o') {
                gripperOpen();
                replySlave("S>OK Go");
                return;
            }
            if (action == 'c') {
                gripperClose();
                replySlave("S>OK Gc");
                return;
            }
            if (action == 's') {
                gripperStop();
                replySlave("S>OK Gs");
                return;
            }
        }

        if (cmd == 'x') {
            stopAll();
            replySlave("S>OK STOP");
        }
    }

    void initPidAndTimers() {
        // Timer setup expected by Makeblock encoder control.
        TCCR1A = _BV(WGM10);
        TCCR1B = _BV(CS11) | _BV(WGM12);
        TCCR2A = _BV(WGM21) | _BV(WGM20);
        TCCR2B = _BV(CS21);

        for (int i = 0; i < 3; i++) {
            arms[i]->setPulse(7);
            arms[i]->setRatio(26.9);
            arms[i]->setPosPid(1.8, 0, 1.2);
            arms[i]->setSpeedPid(0.18, 0, 0);
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

        attachInterrupt(arm1.getIntNum(), isr_Arm1, RISING);
        attachInterrupt(arm2.getIntNum(), isr_Arm2, RISING);
        attachInterrupt(arm3.getIntNum(), isr_Arm3, RISING);

        initPidAndTimers();
        Serial.println("Standalone Slave ready");
    }

    void loop() {
        pollCommandStream(Serial3);
        pollCommandStream(Serial);

        // Required for closed-loop encoder motor control.
        for (int i = 0; i < 3; i++) {
            arms[i]->loop();
        }

        // Safety: stop the gripper if command holds it too long.
        if (gripperRunning && (millis() - gripperStartMs >= GRIPPER_MAX_RUN_MS)) {
            gripperStop();
            Serial.println("[Standalone Slave] Gripper auto-stopped");
        }
    }
