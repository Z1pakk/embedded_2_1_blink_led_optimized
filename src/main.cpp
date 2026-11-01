#include <Arduino.h>

enum class LedSwitchState {
    OFF = 0,
    ON = 1
};

enum class LedColor {
    RED = 0,
    BLUE = 1
};

constexpr uint8_t LED_RED_OUT { 15 };
constexpr uint8_t LED_BLUE_OUT { 16 };

constexpr uint16_t LED_DELAY_MS { 1000 };

struct Led {
  LedSwitchState state;
  uint8_t pin;

  void begin(uint8_t pin) {
      this->pin = pin;

      pinMode(pin, OUTPUT);
  }

  void init() {
      state = LedSwitchState::OFF;
  }

  void toggle() {
    if (state == LedSwitchState::ON) {
        setState(LedSwitchState::OFF);
    } else {
        setState(LedSwitchState::ON);
    }
  }

  void setState(LedSwitchState newState) {
      state = newState;

      if (state == LedSwitchState::ON) {
          digitalWrite(pin, HIGH);
      } else {
          digitalWrite(pin, LOW );
      }
  }
};

constexpr uint32_t BUTTON_DEBOUNCE_MS { 50 };
constexpr uint32_t BUTTON_PIN_INPUT { 0 };

struct Button {
  uint8_t pin;

  volatile bool pressed = false;   // set in ISR, read in loop()
  bool pending = false;
  uint32_t lastEdgeMs = 0;

  explicit Button() {}

  void begin(uint8_t buttonPin) {
      pin = buttonPin;
      pinMode(pin, INPUT_PULLUP);
      attachInterruptArg(digitalPinToInterrupt(pin), pressISR, this, FALLING);
  }

  // ISR: minimal, only sets the flag
  static void IRAM_ATTR pressISR(void *arg) {
      static_cast<Button *>(arg)->pressed = true;
  }

  // Call every loop(); returns true once per real press
  bool wasPressed() {
    if (pressed) {
        pressed = false;
        pending = true;
        lastEdgeMs = millis();       // every bounce restarts the wait
    }

    if (pending && millis() - lastEdgeMs >= BUTTON_DEBOUNCE_MS) {
        pending = false;
        return digitalRead(pin) == LOW;   // still pressed → real press
    }
    return false;
  }
};

struct PeriodicTimer {
  uint32_t period;
  uint32_t last;

  void start(uint32_t periodMs) {
    period = periodMs;
    last = millis();
  }

  // Returns true once per period; never blocks.
  bool tick() {
    uint32_t now = millis();
    if (now - last < period) {
        return false;
    }
    last += period;   // steady rhythm, no drift
    return true;
  }

  uint32_t elapsed() {
    return millis() - last;
  }
};

enum class LedState : uint8_t { BlinkRed, BlinkBlue, AlwaysOn, AlwaysOff };
enum class LedEvent : uint8_t { Press, Tick };

struct LedMode {
  Led &red;
  Led &blue;
  PeriodicTimer timer;
  LedState state = LedState::BlinkRed;

  LedMode(Led &redLed, Led &blueLed)
    : red(redLed), blue(blueLed) {}

  void begin() {
    enter(LedState::BlinkRed);
    timer.start(LED_DELAY_MS);
  }

  // Turn inputs into events; call every loop()
  void update(bool buttonPressed) {
    if (buttonPressed) {
        handle(LedEvent::Press);
    }
    if (timer.tick()) {
        handle(LedEvent::Tick);
    }
  }

  // Transition table: (state, event) → next state
  void handle(LedEvent e) {
    switch (state) {
      case LedState::BlinkRed:
        if (e == LedEvent::Tick)  enter(LedState::BlinkBlue);
        if (e == LedEvent::Press) enter(LedState::AlwaysOn);
        break;
      case LedState::BlinkBlue:
        if (e == LedEvent::Tick)  enter(LedState::BlinkRed);
        if (e == LedEvent::Press) enter(LedState::AlwaysOn);
        break;
      case LedState::AlwaysOn:
        if (e == LedEvent::Press) enter(LedState::AlwaysOff);
        break;
      case LedState::AlwaysOff:
        if (e == LedEvent::Press) {
            enter(LedState::BlinkRed);
            timer.start(LED_DELAY_MS);   // fresh rhythm when blinking resumes
        }
        break;
    }
  }

  // Entry actions: what each state looks like
  void enter(LedState s) {
    state = s;
    switch (s) {
      case LedState::BlinkRed:  red.setState(LedSwitchState::ON);  blue.setState(LedSwitchState::OFF); break;
      case LedState::BlinkBlue: red.setState(LedSwitchState::OFF); blue.setState(LedSwitchState::ON);  break;
      case LedState::AlwaysOn:  red.setState(LedSwitchState::ON);  blue.setState(LedSwitchState::ON);  break;
      case LedState::AlwaysOff: red.setState(LedSwitchState::OFF); blue.setState(LedSwitchState::OFF); break;
    }
  }
};

Led redLed;
Led blueLed;

Button button;

PeriodicTimer blinkTimer;

LedMode ledMode(redLed, blueLed);

uint32_t sumUs = 0;
uint32_t loopCount = 0;

void setup() {
  Serial.begin(115200);

  // Wait for the serial connection to be established
  delay(500);

  redLed.begin(LED_RED_OUT);
  blueLed.begin(LED_BLUE_OUT);

  button.begin(BUTTON_PIN_INPUT);

  blinkTimer.start(LED_DELAY_MS);
}

void loop() {
  uint32_t startUs = micros();

  ledMode.update(button.wasPressed());

  // measure the time taken for one loop iteration and accumulate it
  sumUs += micros() - startUs;
  loopCount++;

  if (loopCount == 10000) {
    Serial.printf("avg loop: %.3f us\n", sumUs / 10000.0f);
    loopCount = 0;
    sumUs = 0;
  }
}