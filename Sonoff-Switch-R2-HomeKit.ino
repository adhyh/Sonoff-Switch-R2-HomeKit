// ==================================================
// Hardware selection
// ==================================================
//#define SONOFF_R4_MINI
#define SONOFF_R4_BASIC

#define FW_VERSION "1.0.0"

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <WiFiManager.h>
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
// Hostname / AP name
// ==================================================
static String buildHostname() {
  uint64_t mac = ESP.getEfuseMac();
  char suf[7];
  snprintf(suf, sizeof(suf), "%02X%02X%02X",
           (uint8_t)(mac >> 16),
           (uint8_t)(mac >> 8),
           (uint8_t)(mac));
#if defined(SONOFF_R4_MINI)
  return String("Sonoff Mini R4-") + suf;
#elif defined(SONOFF_R4_BASIC)
  return String("Sonoff Basic R4-") + suf;
#else
  return String("Sonoff Unknown-") + suf;
#endif
}

static const char* provisioningAPName() {
#if defined(SONOFF_R4_MINI)
  return "Sonoff-Mini-R4-Setup";
#elif defined(SONOFF_R4_BASIC)
  return "Sonoff-Basic-R4-Setup";
#else
  return "Sonoff-R4-Setup";
#endif
}

// ==================================================
// Config load/save
// ==================================================
static bool loadConfig(String &ssid, String &pass, String &pin,
                       bool &toggle, bool &invert) {

  prefs.begin(PREF_NS, true);
  ssid   = prefs.getString(K_SSID, "");
  pass   = prefs.getString(K_PASS, "");
  pin    = prefs.getString(K_PIN, "");

  toggle = (hw.switchPin != -1)
             ? prefs.getBool(K_TOGGLE, false)
             : false;

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

  if (hw.switchPin != -1)
    prefs.putBool(K_TOGGLE, toggle);

  prefs.putBool(K_INVERT, invert);
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
// Provisioning
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
  wm.setEnableConfigPortal(true);

  std::vector<const char*> menu = {
    "wifi",
    "update",
    "exit"
  };
  wm.setMenu(menu);

  wm.setSaveParamsCallback([&]() {
    if (hw.switchPin != -1)
      g_toggleFromPortal = wm.server->hasArg("toggle");

    g_invertFromPortal = wm.server->hasArg("invert");
  });

  // HomeKit PIN
  String hkHtml =
    "<div style='padding:12px;border:2px solid #444;border-radius:8px;text-align:center'>"
    "<div>HomeKit pairing code</div>"
    "<div style='font-size:26px;font-weight:bold'>" +
    formatHKPin(pin) +
    "</div></div>";

  wm.addParameter(new WiFiManagerParameter(hkHtml.c_str()));

  // Options
  String optionsHtml =
    "<div style='margin:14px 0;padding:12px;border:1px solid #ddd;border-radius:8px'>";

  if (hw.switchPin != -1)
    optionsHtml +=
      "<label><input type='checkbox' name='toggle'> Toggle switch mode</label><br>";

  optionsHtml +=
    "<label><input type='checkbox' name='invert'> Invert relay state</label>"
    "</div>"
    "<div class='fw-version'>Firmware version " FW_VERSION "</div>";

  wm.addParameter(new WiFiManagerParameter(optionsHtml.c_str()));

  wm.startConfigPortal(provisioningAPName());

  while (true) {
    wm.process();
    hw.poll();
    delay(5);

    if (WiFi.status() == WL_CONNECTED) {
      saveConfig(WiFi.SSID(), WiFi.psk(), pin,
                 g_toggleFromPortal,
                 g_invertFromPortal);
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

  WiFi.setHostname(buildHostname().c_str());

  strlcpy(hkPin, pin.c_str(), sizeof(hkPin));
  homeSpan.setPairingCode(hkPin);
  homeSpan.setWifiCredentials(ssid.c_str(), pass.c_str());

  homeSpan.setPairCallback([](boolean paired) {
    if (!paired) g_resetRequested = true;
  });

  new SpanAccessory();
  {
    new Service::AccessoryInformation();
    new Characteristic::Identify();
    new Characteristic::Manufacturer("Sonoff");
    new Characteristic::Model("Sonoff R4");
    new Characteristic::FirmwareRevision(FW_VERSION);
    hk = new SwitchHomeKit(hw.logicalState);
  }

  homeSpan.begin(Category::Switches, buildHostname().c_str());

#if defined(SONOFF_R4_BASIC)
  // ESP32-C3: HomeKit over IP requires IPv6
  WiFi.enableIPv6();
#endif

  hw.homekitActive = true;
}


// ==================================================
// Arduino lifecycle
// ==================================================
void setup() {

  Serial.begin(115200);
  delay(300);

  hw.begin();   // switchPin is now known

  hw.onToggle = [](bool on) {
    if (hk) hk->sync(on);
  };
  hw.onLongPress = factoryReset;

  String ssid, pass, pin;
  bool toggle = false, invert = false;

  if (!loadConfig(ssid, pass, pin, toggle, invert))
    runProvisioning();

  hw.toggleMode  = (hw.switchPin != -1) ? toggle : false;
  hw.invertRelay = invert;

  runHomeSpan(ssid, pass, pin);
}

void loop() {
  if (g_resetRequested) {
    g_resetRequested = false;
    factoryReset();
  }
  hw.poll();
  homeSpan.poll();
}
