// ==================================================
// Hardware selection
// ==================================================
#define SONOFF_R4_MINI
// #define SONOFF_R4_BASIC

#define FW_VERSION "1.0.0"

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <Update.h>
#include <HomeSpan.h>

#include "hw/hw_select.h"
#include "homekit/switch_homekit.h"

// ==================================================
// Globals
// ==================================================
Preferences prefs;
DeviceHardware hw;
SwitchHomeKit *hk = nullptr;
static char hkPin[9];
static volatile bool g_resetRequested = false;

// ==================================================
// Preferences keys
// ==================================================
static const char *PREF_NS   = "cfg";
static const char *K_SSID   = "ssid";
static const char *K_PASS   = "pass";
static const char *K_PIN    = "hkpin";
static const char *K_TOGGLE = "toggle";
static const char *K_INVERT = "invert";

// ==================================================
// Helpers
// ==================================================
static String generateHKPin() {
  String pin;
  do {
    pin = "";
    for (int i = 0; i < 8; i++) pin += char('0' + random(0,10));
  } while (
    pin == "00000000" ||
    pin == "11111111" ||
    pin == "12345678" ||
    pin == "87654321"
  );
  return pin;
}

static String formatHKPin(const String &pin) {
  return pin.substring(0,3) + "-" +
         pin.substring(3,5) + "-" +
         pin.substring(5,8);
}

// ==================================================
// Identity helpers
// ==================================================
static void macSuffix(char *out, size_t len) {
  uint64_t mac = ESP.getEfuseMac();
  snprintf(out, len, "%02X%02X%02X",
           (uint8_t)(mac >> 16),
           (uint8_t)(mac >> 8),
           (uint8_t)(mac));
}

static String buildHostname() {
  char suf[7]; macSuffix(suf, sizeof(suf));
#if defined(SONOFF_R4_MINI)
  return String("Sonoff Mini R4-") + suf;
#elif defined(SONOFF_R4_BASIC)
  return String("Sonoff Basic R4-") + suf;
#else
  return String("Sonoff Unknown-") + suf;
#endif
}

static String modelName() {
#if defined(SONOFF_R4_MINI)
  return "Sonoff Mini R4";
#elif defined(SONOFF_R4_BASIC)
  return "Sonoff Basic R4";
#else
  return "Sonoff R4 (Unknown)";
#endif
}

// ==================================================
// Config load/save
// ==================================================
static bool loadConfig(String &ssid, String &pass, String &pin, bool &toggle, bool &invert) {
  prefs.begin(PREF_NS, true);
  ssid   = prefs.getString(K_SSID, "");
  pass   = prefs.getString(K_PASS, "");
  pin    = prefs.getString(K_PIN, "");
  toggle = prefs.getBool(K_TOGGLE, false);
  invert = prefs.getBool(K_INVERT, false);
  prefs.end();
  return (ssid.length() && pin.length() == 8);
}

static void saveConfig(const String &ssid,
                       const String &pass,
                       const String &pin,
                       bool toggle,
                       bool invert) {
  prefs.begin(PREF_NS, false);
  prefs.putString(K_SSID, ssid);
  prefs.putString(K_PASS, pass);
  prefs.putString(K_PIN,  pin);
  prefs.putBool  (K_TOGGLE, toggle);
  prefs.putBool  (K_INVERT, invert);
  prefs.end();
}

// ==================================================
// Factory reset
// ==================================================
static void factoryReset() {
  Preferences p;
  p.begin(PREF_NS, false);    p.clear(); p.end();
  p.begin("homeSpan", false); p.clear(); p.end();
  homeSpan.processSerialCommand("F");
  WiFi.disconnect(true, true);
  delay(200);
  ESP.restart();
}

// ==================================================
// Provisioning (non-blocking, handbediening actief)
// ==================================================
static bool g_toggleFromPortal = false;
static bool g_invertFromPortal = false;

static void runProvisioning() {

  randomSeed(esp_random());
  String pin = generateHKPin();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(200);

  WiFiManager wm;
  wm.setConfigPortalTimeout(0);

  // ---------- Custom CSS + FW footer ----------
  String headHtml =
    String(R"rawliteral(
<style>
  .opt-group {
    margin:14px 0;
    padding:14px;
    border:1px solid #ddd;
    border-radius:10px;
    background:#fafafa;
  }
  .opt-row {
    display:flex;
    justify-content:space-between;
    align-items:center;
    margin:12px 0;
    font-size:16px;
  }
  /* iOS style switch */
  .ios-switch {
    position: relative;
    width: 52px;
    height: 32px;
  }
  .ios-switch input {
    display:none;
  }
  .slider {
    position:absolute;
    cursor:pointer;
    inset:0;
    background:#ccc;
    border-radius:32px;
    transition:.25s;
  }
  .slider:before {
    content:"";
    position:absolute;
    height:26px;
    width:26px;
    left:3px;
    top:3px;
    background:white;
    border-radius:50%;
    transition:.25s;
    box-shadow:0 1px 3px rgba(0,0,0,.4);
  }
  .ios-switch input:checked + .slider {
    background:#34C759;
  }
  .ios-switch input:checked + .slider:before {
    transform:translateX(20px);
  }
  .fw-footer {
    position: fixed;
    bottom: 6px;
    left: 0;
    right: 0;
    text-align: center;
    font-size: 12px;
    color: #666;
    pointer-events: none;
  }
</style>
<div class="fw-footer">Firmware version )rawliteral")
    + FW_VERSION +
    R"rawliteral(</div>)rawliteral";

  wm.setCustomHeadElement(headHtml.c_str());

  wm.setSaveParamsCallback([&]() {
    g_toggleFromPortal = wm.server->hasArg("toggle");
    g_invertFromPortal = wm.server->hasArg("invert");
  });

  // ---------- HomeKit PIN ----------
  String hkHtml =
    "<div style='padding:12px;border:2px solid #444;border-radius:10px;text-align:center'>"
    "<div>HomeKit pairing code</div>"
    "<div style='font-size:26px;font-weight:bold'>" +
    formatHKPin(pin) +
    "</div></div>";
  WiFiManagerParameter hkInfo("", "", hkHtml.c_str(), 0);
  wm.addParameter(&hkInfo);

  // ---------- iOS style switches ----------
  const char *optionsHtml = R"rawliteral(
<div class="opt-group">
  <div class="opt-row">
    <span>Toggle switch mode</span>
    <label class="ios-switch">
      <input type="checkbox" name="toggle">
      <span class="slider"></span>
    </label>
  </div>
  <div class="opt-row">
    <span>Invert relay state</span>
    <label class="ios-switch">
      <input type="checkbox" name="invert">
      <span class="slider"></span>
    </label>
  </div>
</div>
)rawliteral";
  WiFiManagerParameter pOptions("", "", optionsHtml, 0);
  wm.addParameter(&pOptions);

  wm.startConfigPortal("Sonoff-R4-Setup");

  while (true) {
    wm.process();
    hw.poll();        // handbediening altijd
    delay(5);

    if (WiFi.status() == WL_CONNECTED) {
      saveConfig(WiFi.SSID(), WiFi.psk(), pin, g_toggleFromPortal, g_invertFromPortal);
      ESP.restart();
    }
  }
}

// ==================================================
// HomeSpan
// ==================================================
static void runHomeSpan(const String &ssid,
                        const String &pass,
                        const String &pin) {

  String hostname = buildHostname();
  WiFi.setHostname(hostname.c_str());

  strlcpy(hkPin, pin.c_str(), sizeof(hkPin));
  homeSpan.setPairingCode(hkPin);
  homeSpan.setWifiCredentials(ssid.c_str(), pass.c_str());
  homeSpan.enableOTA(false);

  homeSpan.setPairCallback([](boolean paired) {
    if (!paired) g_resetRequested = true;
  });

  new SpanAccessory();
  {
    new Service::AccessoryInformation();
    new Characteristic::Identify();
    new Characteristic::Manufacturer("Sonoff");
    new Characteristic::Model(modelName().c_str());

    char sn[7]; macSuffix(sn, sizeof(sn));
    new Characteristic::SerialNumber(sn);
    new Characteristic::FirmwareRevision(FW_VERSION);

    hk = new SwitchHomeKit(hw.logicalState);
  }

  homeSpan.begin(Category::Switches, hostname.c_str());
  hw.homekitActive = true;
}

// ==================================================
// Arduino lifecycle
// ==================================================
void setup() {

  Serial.begin(115200);
  delay(300);

  hw.begin();   // hardware altijd actief

  hw.onToggle = [](bool on) {
    if (hk) hk->sync(on);
  };

  hw.onLongPress = []() {
    factoryReset();
  };

  String ssid, pass, pin;
  bool toggle = false;
  bool invert = false;

  if (!loadConfig(ssid, pass, pin, toggle, invert)) {
    runProvisioning();   // keert alleen terug via reboot
  }

  hw.toggleMode  = toggle;
  hw.invertRelay = invert;

  runHomeSpan(ssid, pass, pin);
}

void loop() {

  if (g_resetRequested) {
    g_resetRequested = false;
    factoryReset();
  }

  hw.poll();        // altijd actief
  homeSpan.poll();  // HomeKit / netwerk
}
