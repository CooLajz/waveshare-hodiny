#include "WifiOnboarding.h"

#include "FirmwareBuild.h"

#if FIRMWARE_RELEASE

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
#include <lvgl.h>

#include "ClockFonts.h"
#include "DisplayDriver.h"
#include "ImprovSerialService.h"
#include "WifiProvisioning.h"

namespace {
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
constexpr uint32_t RESTART_DELAY_MS = 1800;
constexpr uint8_t MAX_VISIBLE_NETWORKS = 20;
constexpr uint8_t ACCESS_POINT_PASSWORD_LENGTH = 8;
constexpr char PORTAL_IP[] = "192.168.4.1";

const char PORTAL_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="cs"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Nastavení Wi-Fi · Waveshare Hodiny</title><style>
:root{color-scheme:dark;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;background:#05080b;color:#f6f6f6}*{box-sizing:border-box}body{margin:0;min-height:100vh;display:grid;place-items:center;padding:18px;background:radial-gradient(circle at top,#123040 0,#05080b 55%)}main{width:min(100%,440px);background:#101820;border:1px solid #29404e;border-radius:22px;padding:24px;box-shadow:0 20px 55px #0008}h1{font-size:26px;margin:0 0 8px}p{color:#b5c4cc;line-height:1.45;margin:0 0 22px}label{display:block;font-weight:650;margin:16px 0 7px}select,input,button{width:100%;min-height:48px;border-radius:12px;border:1px solid #385465;background:#081117;color:#fff;padding:0 13px;font:inherit}button{margin-top:22px;border:0;background:#4ccbec;color:#00151c;font-weight:800;cursor:pointer}button:disabled{opacity:.55}.hint{font-size:13px;margin-top:8px;color:#8fa6b2}.error{color:#ff7272}.hidden{display:none}</style></head>
<body><main><h1>Nastavení Wi-Fi</h1><p>Vyberte domácí síť a zadejte její heslo. Hodiny údaje uloží, restartují se a připojení ověří při novém startu.</p>
<form id="form"><label for="network">Dostupná síť</label><select id="network" disabled><option>Vyhledávám sítě…</option></select><div class="hint" id="scanState">Probíhá vyhledávání 2,4GHz sítí.</div>
<label for="ssid">Název sítě (SSID)</label><input id="ssid" name="ssid" maxlength="32" autocomplete="off" required>
<label for="password">Heslo</label><input id="password" name="password" type="password" maxlength="64" autocomplete="new-password"><div class="hint">U otevřené sítě ponechte pole prázdné.</div>
<button id="save" type="submit">Uložit a restartovat hodiny</button><div class="hint" id="status" role="status"></div></form></main>
<script>
const select=document.getElementById('network'),ssid=document.getElementById('ssid'),state=document.getElementById('scanState'),status=document.getElementById('status'),save=document.getElementById('save');
function load(){fetch('/api/networks',{cache:'no-store'}).then(r=>r.json()).then(data=>{if(data.scanning){setTimeout(load,900);return}select.innerHTML='<option value="">— zadejte síť ručně —</option>';for(const n of data.networks){const o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+'  '+n.rssi+' dBm'+(n.secure?'  🔒':'');select.appendChild(o)}select.disabled=false;state.textContent=data.networks.length?'Nalezeno sítí: '+data.networks.length:'Žádná síť nebyla nalezena. SSID lze zadat ručně.'}).catch(()=>{state.textContent='Vyhledávání se nezdařilo, zkouším znovu…';setTimeout(load,1500)})}
select.addEventListener('change',()=>{if(select.value)ssid.value=select.value});
document.getElementById('form').addEventListener('submit',async e=>{e.preventDefault();save.disabled=true;status.className='hint';status.textContent='Ukládám nastavení…';const body=new URLSearchParams({ssid:ssid.value,password:document.getElementById('password').value});try{const r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});const data=await r.json();if(!r.ok||!data.ok)throw new Error(data.error||'Nastavení se nepodařilo uložit.');status.textContent='Uloženo. Hodiny se nyní restartují…'}catch(err){status.className='hint error';status.textContent=err.message;save.disabled=false}});load();
</script></body></html>)HTML";

DNSServer *dnsServer = nullptr;
WebServer *portalServer = nullptr;
String accessPointSsid;
String accessPointPassword;
unsigned long restartAt = 0;
bool restartScreenShown = false;

String jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t index = 0; index < value.length(); ++index) {
    const char character = value[index];
    if (character == '\\' || character == '"') escaped += '\\';
    if (static_cast<uint8_t>(character) >= 0x20) escaped += character;
  }
  return escaped;
}

void styleScreen() {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x05080B), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
}

lv_obj_t *makeCenteredLabel(const char *text, const lv_font_t *font,
                            lv_color_t color, lv_coord_t y,
                            lv_coord_t width = 440) {
  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_obj_set_width(label, width);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_label_set_text(label, text);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
  return label;
}

void showConnectingScreen() {
  styleScreen();
  makeCenteredLabel("Waveshare Hodiny", &lv_font_montserrat_28,
                    lv_color_hex(0xF6F6F6), 150, 360);
  makeCenteredLabel("Pripojuji Wi-Fi...", &clock_czech_20,
                    lv_color_hex(0x4CCBEC), 205, 320);
  displayDriverRefresh();
}

void showRestartScreen() {
  if (restartScreenShown) return;
  restartScreenShown = true;
  styleScreen();
  makeCenteredLabel("Wi-Fi ulozena", &clock_czech_20,
                    lv_color_hex(0x66D622), 175, 300);
  makeCenteredLabel("Restartuji...", &clock_czech_20,
                    lv_color_hex(0xF6F6F6), 225, 280);
  displayDriverRefresh();
}

void makeAccessPointCredentials() {
  const uint64_t chipId = ESP.getEfuseMac();
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X",
           static_cast<unsigned int>(chipId & 0xFFFF));
  accessPointSsid = "Waveshare-Hodiny-";
  accessPointSsid += suffix;

  static constexpr char ALPHABET[] =
      "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  accessPointPassword.reserve(ACCESS_POINT_PASSWORD_LENGTH + 1);
  for (uint8_t index = 0; index < ACCESS_POINT_PASSWORD_LENGTH; ++index) {
    accessPointPassword += ALPHABET[esp_random() % (sizeof(ALPHABET) - 1)];
  }
}

void showPortalScreen() {
  styleScreen();
  makeCenteredLabel("Nastaveni Wi-Fi", &clock_czech_20,
                    lv_color_hex(0xF6F6F6), 12, 220);

  String qrPayload = "WIFI:T:WPA;S:";
  qrPayload += accessPointSsid;
  qrPayload += ";P:";
  qrPayload += accessPointPassword;
  qrPayload += ";;";
  lv_obj_t *qr = lv_qrcode_create(lv_scr_act(), 214, lv_color_black(),
                                  lv_color_white());
  lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 52);
  lv_qrcode_update(qr, qrPayload.c_str(), qrPayload.length());

  makeCenteredLabel("Naskenujte QR kod", &clock_czech_18,
                    lv_color_hex(0x4CCBEC), 283, 340);
  String networkText = "Sit: ";
  networkText += accessPointSsid;
  makeCenteredLabel(networkText.c_str(), &clock_czech_16,
                    lv_color_hex(0xF6F6F6), 321, 380);
  String passwordText = "Heslo: ";
  passwordText += accessPointPassword;
  makeCenteredLabel(passwordText.c_str(), &clock_czech_16,
                    lv_color_hex(0xF6F6F6), 351, 370);
  makeCenteredLabel("Kdyz se web neotevre:\n192.168.4.1",
                    &clock_czech_16, lv_color_hex(0xB5B5B5), 393,
                    280);
  displayDriverRefresh();
}

void sendPortalPage() {
  portalServer->sendHeader("Cache-Control", "no-store");
  portalServer->send_P(200, PSTR("text/html; charset=utf-8"), PORTAL_PAGE);
}

void redirectToPortal() {
  portalServer->sendHeader("Location", String("http://") + PORTAL_IP + "/",
                           true);
  portalServer->send(302, PSTR("text/plain"), "");
}

void startNetworkScan() {
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return;
  WiFi.scanDelete();
  WiFi.scanNetworks(true, true);
}

void handleNetworkList() {
  const int16_t count = WiFi.scanComplete();
  if (count == WIFI_SCAN_FAILED) {
    startNetworkScan();
    portalServer->send(200, PSTR("application/json; charset=utf-8"),
                       F("{\"scanning\":true}"));
    return;
  }
  if (count == WIFI_SCAN_RUNNING) {
    portalServer->send(200, PSTR("application/json; charset=utf-8"),
                       F("{\"scanning\":true}"));
    return;
  }

  portalServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  portalServer->send(200, PSTR("application/json; charset=utf-8"), "");
  portalServer->sendContent(F("{\"scanning\":false,\"networks\":["));
  uint8_t emitted = 0;
  for (int16_t index = 0;
       index < count && emitted < MAX_VISIBLE_NETWORKS; ++index) {
    const String ssid = WiFi.SSID(index);
    if (ssid.isEmpty()) continue;
    bool duplicate = false;
    for (int16_t previous = 0; previous < index; ++previous) {
      if (WiFi.SSID(previous) == ssid) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;
    String item;
    item.reserve(ssid.length() + 55);
    if (emitted > 0) item += ',';
    item += F("{\"ssid\":\"");
    item += jsonEscape(ssid);
    item += F("\",\"rssi\":");
    item += WiFi.RSSI(index);
    item += F(",\"secure\":");
    item += WiFi.encryptionType(index) == WIFI_AUTH_OPEN ? F("false")
                                                         : F("true");
    item += '}';
    portalServer->sendContent(item);
    ++emitted;
  }
  portalServer->sendContent(F("]}"));
  portalServer->sendContent("");
  WiFi.scanDelete();
}

void handleWifiSave() {
  if (restartAt != 0) {
    portalServer->send(409, PSTR("application/json; charset=utf-8"),
                       F("{\"ok\":false,\"error\":\"Restart již probíhá.\"}"));
    return;
  }
  const String ssid = portalServer->arg("ssid");
  const String password = portalServer->arg("password");
  if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 64) {
    portalServer->send(400, PSTR("application/json; charset=utf-8"),
                       F("{\"ok\":false,\"error\":\"SSID nebo heslo nemá platnou délku.\"}"));
    return;
  }
  if (!wifiProvisioningSavePendingForRestart(ssid, password)) {
    portalServer->send(500, PSTR("application/json; charset=utf-8"),
                       F("{\"ok\":false,\"error\":\"Údaje se nepodařilo uložit.\"}"));
    return;
  }
  portalServer->sendHeader("Cache-Control", "no-store");
  portalServer->send(200, PSTR("application/json; charset=utf-8"),
                     F("{\"ok\":true}"));
  restartAt = millis() + RESTART_DELAY_MS;
  showRestartScreen();
}

void startPortalServer() {
  portalServer = new WebServer(80);
  dnsServer = new DNSServer();
  portalServer->on("/", HTTP_GET, sendPortalPage);
  portalServer->on("/api/networks", HTTP_GET, handleNetworkList);
  portalServer->on("/api/wifi", HTTP_POST, handleWifiSave);
  portalServer->on("/generate_204", HTTP_GET, redirectToPortal);
  portalServer->on("/gen_204", HTTP_GET, redirectToPortal);
  portalServer->on("/hotspot-detect.html", HTTP_GET, sendPortalPage);
  portalServer->on("/library/test/success.html", HTTP_GET, sendPortalPage);
  portalServer->on("/ncsi.txt", HTTP_GET, redirectToPortal);
  portalServer->on("/connecttest.txt", HTTP_GET, redirectToPortal);
  portalServer->onNotFound(redirectToPortal);
  portalServer->begin();
  dnsServer->start(53, "*", WiFi.softAPIP());
  startNetworkScan();
}

void runPortal() {
  wifiProvisioningPauseStartupRetries();
  WiFi.mode(WIFI_AP_STA);
  makeAccessPointCredentials();
  const IPAddress portalAddress(192, 168, 4, 1);
  const IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(portalAddress, portalAddress, subnet);
  WiFi.softAP(accessPointSsid.c_str(), accessPointPassword.c_str(), 1, false,
              2);
  showPortalScreen();
  startPortalServer();

  while (true) {
    improvSerialServiceLoop();
    wifiProvisioningLoop();
    dnsServer->processNextRequest();
    portalServer->handleClient();
    displayDriverLoop();

    if (restartAt == 0 && wifiProvisioningReadyForNormalStart()) {
      restartAt = millis() + RESTART_DELAY_MS;
      showRestartScreen();
    }
    if (restartAt != 0 && static_cast<long>(millis() - restartAt) >= 0) {
      ESP.restart();
    }
    delay(5);
  }
}
}  // namespace

void wifiOnboardingRequireConnection() {
  showConnectingScreen();
  const unsigned long startedAt = millis();
  while (wifiProvisioningHasCredentials() &&
         millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
    improvSerialServiceLoop();
    wifiProvisioningLoop();
    displayDriverLoop();
    if (wifiProvisioningReadyForNormalStart()) {
      lv_obj_clean(lv_scr_act());
      return;
    }
    delay(5);
  }
  runPortal();
}

#else

void wifiOnboardingRequireConnection() {}

#endif
