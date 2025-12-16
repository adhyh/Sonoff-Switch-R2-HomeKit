#include <Arduino.h>
#include <base64.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <arduino_homekit_server.h>

#define LOG_D(fmt, ...) printf_P(PSTR(fmt "\n"), ##__VA_ARGS__)

// ---------- GPIO (Sonoff Basic R2) ----------
#define PIN_RELAY   12   // GPIO12 – relay (active LOW)
#define PIN_LED     13   // GPIO13 – blue LED (active LOW)
#define PIN_BUTTON  0    // GPIO0  – button (active LOW)

// ---------- Timings ----------
#define DEBOUNCE_MS     50
#define LONGPRESS_MS 5000
#define LED_BLINK_MS  500

extern "C" {
  extern homekit_characteristic_t cha_switch_on;
  extern homekit_server_config_t config;
}


// ---------- State ----------
static bool relayOn = false;
static bool lastButtonState = HIGH;
static uint32_t buttonPressStart = 0;
static uint32_t lastDebounceTime = 0;
static uint32_t lastLedToggle = 0;
static bool ledBlinkState = false;

// ---------- Helpers ----------
void setRelay(bool on)
{
  relayOn = on;
  digitalWrite(PIN_RELAY, relayOn ? HIGH : LOW);
  //digitalWrite(PIN_LED,   relayOn ? LOW : HIGH);
}

void notifyHomeKit()
{
  cha_switch_on.value.bool_value = relayOn;
  homekit_characteristic_notify(&cha_switch_on, cha_switch_on.value);
}

// ---------- HomeKit setter ----------
void cha_switch_on_setter(const homekit_value_t value)
{
  setRelay(value.bool_value);
  cha_switch_on.value.bool_value = relayOn;
}

// ---------- Resets ----------
void full_reset()
{
  LOG_D("FULL RESET");

  digitalWrite(PIN_LED, LOW); // LED ON during reset

  homekit_storage_reset();

  WiFiManager wm;
  wm.resetSettings();

  delay(500);
  ESP.restart();
}

void wifi_only_reset()
{
  LOG_D("WiFi RESET");
  WiFiManager wm;
  wm.resetSettings();
  delay(300);
  ESP.restart();
}

// ---------- HomeKit setup ----------
void my_homekit_setup()
{
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  relayOn = cha_switch_on.value.bool_value;

  digitalWrite(PIN_RELAY, LOW); // relay off
  digitalWrite(PIN_LED, HIGH);   // LED off

  cha_switch_on.setter = cha_switch_on_setter;
  arduino_homekit_setup(&config);
}

// ---------- Button handling (debounced, no delay) ----------
void handle_button()
{
  static uint32_t pressStart = 0;
  static bool wasPressed = false;

  bool pressed = (digitalRead(PIN_BUTTON) == LOW);

  // Button just pressed
  if (pressed && !wasPressed) {
    pressStart = millis();
    wasPressed = true;
  }

  // Button released
  if (!pressed && wasPressed) {
    uint32_t dt = millis() - pressStart;
    wasPressed = false;

    // LONG press → FULL reset
    if (dt >= LONGPRESS_MS) {
      // Visual confirmation BEFORE reset
      digitalWrite(PIN_LED, LOW);   // LED ON
      delay(300);                   // short visible confirmation
      full_reset();
    }

    // SHORT press → toggle relay
    else if (dt >= DEBOUNCE_MS) {
      setRelay(!relayOn);
      notifyHomeKit();
    }
  }
}

// ---------- LED handling ----------
void handle_led()
{
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastLedToggle > LED_BLINK_MS) {
      lastLedToggle = millis();
      ledBlinkState = !ledBlinkState;
      digitalWrite(PIN_LED, ledBlinkState ? LOW : HIGH);
    }
  }
}

// ---------- Setup ----------
void setup()
{
  Serial.begin(115200);
  Serial.println("Start");

  WiFi.mode(WIFI_STA);

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  char apName[32];
  snprintf(apName, sizeof(apName), "Sonoff-Switch-%06X", ESP.getChipId());

  WiFiManager wm;
  if (!wm.autoConnect(apName)) {
    ESP.restart();
  }

  my_homekit_setup();
}

// ---------- Loop ----------
void loop()
{
  arduino_homekit_loop();
  handle_button();
  handle_led();
}
