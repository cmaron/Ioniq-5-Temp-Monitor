// Pro Trinket 5V (ATmega328P @ 16 MHz)
// TPL5110 + 2-channel relay: 1) power-cycle hotspot, 2) hold power button.
// Debug LED blink codes are included on pin 13 (onboard LED).

// ---------------- Pin Map ----------------
const uint8_t RELAY_PWR = 5;   // IN1 - power cut relay (use NC contact for default ON)
const uint8_t RELAY_BTN = 6;   // IN2 - button press relay (COM/NO across button pads)
const uint8_t TPL_DONE  = 3;   // TPL5110 DONE (assert HIGH to cut power)
const uint8_t LED_PIN   = 13;  // Onboard LED

// ------------- Relay Behavior -----------
const bool RELAY_ACTIVE_LOW = true;  // set false if your module is active-HIGH

// ------------- Timings (ms) -------------
const unsigned long SETTLE_MS      = 300;    // settle after power-up
const unsigned long POWER_CUT_MS   = 3000;   // power-off duration
const unsigned long BOOT_WAIT_MS   = 30000;  // wait after power restore
const unsigned long BUTTON_HOLD_MS = 5000;   // press & hold the button
const unsigned long DONE_PULSE_MS  = 500;    // allow TPL to latch DONE

// ------------- LED Blink Helper ---------
void debugBlink(int count, int duration = 200) {
  for (int i = 0; i < count; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(duration);
    digitalWrite(LED_PIN, LOW);
    delay(duration);
  }
}

// Relay write that abstracts active-LOW vs active-HIGH inputs
inline void relayWrite(uint8_t pin, bool on) {
  digitalWrite(pin, (RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW)));
}

void setup() {
  // IO setup first to avoid any brief chatter
  pinMode(RELAY_PWR, OUTPUT);
  pinMode(RELAY_BTN, OUTPUT);
  pinMode(TPL_DONE,  OUTPUT);
  pinMode(LED_PIN,   OUTPUT);

  // Force idle states for safety
  relayWrite(RELAY_PWR, false);    // power relay OFF (NC path closed -> hotspot ON)
  relayWrite(RELAY_BTN, false);    // button relay OFF
  digitalWrite(TPL_DONE, LOW);     // keep TPL power on during our run

  // ---- Blink codes ----
  // 3 quick blinks: boot
  debugBlink(3, 150);

  delay(SETTLE_MS);

  // 1 long blink: about to CUT POWER
  debugBlink(1, 500);

  // --- 1) CUT POWER to hotspot briefly (opens NC path) ---
  relayWrite(RELAY_PWR, true);     // energize power relay -> cut 5V
  delay(POWER_CUT_MS);
  relayWrite(RELAY_PWR, false);    // restore power (NC reconnects)

  // 2 slow blinks: power restored, entering boot wait
  debugBlink(2, 300);

  // --- 2) WAIT for hotspot to boot ---
  delay(BOOT_WAIT_MS);

  // 2 medium blinks: about to PRESS & HOLD button
  debugBlink(2, 200);

  // --- 3) PRESS & HOLD hotspot power button ---
  relayWrite(RELAY_BTN, true);
  delay(BUTTON_HOLD_MS);
  relayWrite(RELAY_BTN, false);

  // 3 medium blinks: sequence finished, signaling DONE
  debugBlink(3, 200);

  // --- 4) Tell TPL5110 we are DONE (TPL will cut power) ---
  delay(100);
  digitalWrite(TPL_DONE, HIGH);
  delay(DONE_PULSE_MS);            // give TPL time to latch DONE
}

void loop() {
  // Not reached (TPL removes power). If it is, do nothing.
}
