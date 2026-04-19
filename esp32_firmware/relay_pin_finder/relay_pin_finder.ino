/**
 * relay_pin_finder.ino
 * ────────────────────────────────────────────────────────────────
 * Flash this FIRST before the main sprinkler firmware.
 *
 * It cycles through every candidate GPIO pin one at a time,
 * holding each for 2 seconds, so you can hear/see which relay
 * clicks for each GPIO.
 *
 * Open Serial Monitor at 115200 baud to see which pin is active.
 * Note down the GPIO number for each relay position (1–8).
 * Then update RELAY_PINS[] in sprinkler_controller.ino.
 *
 * It also tests ACTIVE LOW and ACTIVE HIGH so you can confirm
 * which logic your board uses (you'll hear the relay click on
 * one and not the other).
 * ────────────────────────────────────────────────────────────────
 */

// All GPIO pins that might be used as relay outputs on these boards.
// The sketch will cycle through all of them.
const int CANDIDATE_PINS[] = {
  2, 4, 5, 12, 13, 14, 15,
  16, 17, 18, 19, 21, 22, 23,
  25, 26, 27, 32, 33
};
const int NUM_CANDIDATES = sizeof(CANDIDATE_PINS) / sizeof(CANDIDATE_PINS[0]);

// How long to hold each pin in the test state (ms)
#define HOLD_TIME_MS 2000

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("==============================================");
  Serial.println("  ESP32 Relay Pin Finder");
  Serial.println("==============================================");
  Serial.println("Initialising all candidate pins as OUTPUT LOW...");

  for (int i = 0; i < NUM_CANDIDATES; i++) {
    pinMode(CANDIDATE_PINS[i], OUTPUT);
    digitalWrite(CANDIDATE_PINS[i], HIGH); // Start HIGH (relay OFF for active-low boards)
  }

  delay(1000);

  // ── PHASE 1: ACTIVE LOW TEST ──────────────────────────────────
  Serial.println("\n--- PHASE 1: ACTIVE LOW (pulling LOW to activate) ---");
  Serial.println("If your relays click when a GPIO goes LOW, note those pin numbers.\n");

  for (int i = 0; i < NUM_CANDIDATES; i++) {
    int pin = CANDIDATE_PINS[i];
    Serial.printf(">>> Testing GPIO %d LOW (active-low ON)...\n", pin);
    digitalWrite(pin, LOW);
    delay(HOLD_TIME_MS);
    digitalWrite(pin, HIGH); // Back off
    delay(500);
  }

  // ── PHASE 2: ACTIVE HIGH TEST ─────────────────────────────────
  Serial.println("\n--- PHASE 2: ACTIVE HIGH (pulling HIGH to activate) ---");
  Serial.println("If your relays click when a GPIO goes HIGH, note those pin numbers.\n");

  // First make sure all LOW (so HIGH will be a change)
  for (int i = 0; i < NUM_CANDIDATES; i++) {
    digitalWrite(CANDIDATE_PINS[i], LOW);
  }
  delay(500);

  for (int i = 0; i < NUM_CANDIDATES; i++) {
    int pin = CANDIDATE_PINS[i];
    Serial.printf(">>> Testing GPIO %d HIGH (active-high ON)...\n", pin);
    digitalWrite(pin, HIGH);
    delay(HOLD_TIME_MS);
    digitalWrite(pin, LOW); // Back off
    delay(500);
  }

  // ── DONE ──────────────────────────────────────────────────────
  // Make everything off again
  for (int i = 0; i < NUM_CANDIDATES; i++) {
    pinMode(CANDIDATE_PINS[i], OUTPUT);
    digitalWrite(CANDIDATE_PINS[i], HIGH);
  }

  Serial.println("\n==============================================");
  Serial.println("  Scan complete. All pins set to safe state.");
  Serial.println("  Update RELAY_PINS[] in sprinkler_controller.ino");
  Serial.println("  with the GPIOs that triggered each relay.");
  Serial.println("==============================================");
}

void loop() {
  // Nothing — scan is one-shot at startup
}
