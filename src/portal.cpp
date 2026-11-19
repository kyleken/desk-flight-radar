#include "portal.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "config.h"
#include "settings.h"

namespace portal {
namespace {

WebServer* s_web = nullptr;
DNSServer* s_dns = nullptr;
bool s_active = false;
bool s_hasSub = false;
Submission s_sub;
char s_ap[24] = "";

String esc(const String& in) {
  String o;
  o.reserve(in.length() + 8);
  for (char c : in) {
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      default: o += c;
    }
  }
  return o;
}

const char PAGE_HEAD[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Flight Radar setup</title>
<style>
body{margin:0;background:#0A1420;color:#E8EEF4;font:16px -apple-system,Segoe UI,Roboto,sans-serif}
main{max-width:440px;margin:0 auto;padding:20px}
h1{font-size:22px;margin:4px 0 4px}p{color:#8FA6BD;margin:0 0 18px}
label{display:block;font-weight:600;margin:14px 0 6px}
input{width:100%;box-sizing:border-box;padding:12px;border-radius:10px;border:1px solid #24405E;background:#0E1C2D;color:#fff;font-size:16px}
.row{display:flex;gap:10px}.row>div{flex:1}
fieldset{border:1px solid #1B3048;border-radius:12px;padding:6px 14px 16px;margin:18px 0}
legend{color:#8CC8FF;font-weight:700;padding:0 6px}
small{color:#8FA6BD}
button{margin-top:22px;width:100%;padding:14px;border:0;border-radius:24px;background:#3DDC84;color:#062012;font-size:18px;font-weight:700}
</style></head><body><main>
)HTML";

void handleRoot() {
  String h;
  h.reserve(3000);
  h += FPSTR(PAGE_HEAD);
  h += F("<h1>&#9992; Flight Radar setup</h1><p>Connect the radar to your Wi-Fi and set where it should look.</p>");
  h += F("<form method='POST' action='/save'><fieldset><legend>Wi-Fi (2.4 GHz)</legend>");
  h += F("<label>Network name</label><input name='ssid' list='nets' required maxlength='32' value='");
  h += esc(g_set.ssid);
  h += F("'><datalist id='nets'>");
  int n = WiFi.scanComplete();
  for (int i = 0; i < n && i < 20; ++i) {
    h += F("<option value='");
    h += esc(WiFi.SSID(i));
    h += F("'>");
  }
  h += F("</datalist><label>Password</label><input name='pass' type='password' maxlength='64' placeholder='leave blank for open networks'>");
  h += F("</fieldset><fieldset><legend>Location</legend><label>US ZIP code</label>"
         "<input name='zip' inputmode='numeric' pattern='[0-9]{5}' maxlength='5' value='");
  h += esc(g_set.zip);
  h += F("'><small>Or enter coordinates instead:</small><div class='row'><div><label>Latitude</label>"
         "<input name='lat' inputmode='decimal' placeholder='41.8781'></div><div><label>Longitude</label>"
         "<input name='lon' inputmode='decimal' placeholder='-87.6298'></div></div></fieldset>"
         "<button type='submit'>Save &amp; connect</button></form></main></body></html>");
  s_web->send(200, "text/html", h);
}

void handleSave() {
  Submission s{};
  strncpy(s.ssid, s_web->arg("ssid").c_str(), sizeof(s.ssid) - 1);
  strncpy(s.pass, s_web->arg("pass").c_str(), sizeof(s.pass) - 1);
  String zip = s_web->arg("zip");
  zip.trim();
  if (zip.length() == 5) strncpy(s.zip, zip.c_str(), sizeof(s.zip) - 1);
  String la = s_web->arg("lat"), lo = s_web->arg("lon");
  la.trim();
  lo.trim();
  if (la.length() && lo.length()) {
    s.lat = la.toDouble();
    s.lon = lo.toDouble();
    s.hasLatLon = fabs(s.lat) <= 90 && fabs(s.lon) <= 180 && !(s.lat == 0 && s.lon == 0);
  }
  if (!s.ssid[0]) {
    s_web->send(400, "text/plain", "Network name is required");
    return;
  }
  // Keep the saved password if the field was left blank for the same network.
  if (!s.pass[0] && strcmp(s.ssid, g_set.ssid) == 0) strncpy(s.pass, g_set.pass, sizeof(s.pass) - 1);
  s_sub = s;
  s_hasSub = true;
  String h = FPSTR(PAGE_HEAD);
  h += F("<h1>Saved!</h1><p>The radar is connecting to <b>");
  h += esc(s.ssid);
  h += F("</b>. Watch its screen &mdash; planes appear within a few seconds once it is online. "
         "You can close this page and reconnect your phone to your normal Wi-Fi.</p></main></body></html>");
  s_web->send(200, "text/html", h);
}

void handleRedirect() {
  s_web->sendHeader("Location", "http://192.168.4.1/", true);
  s_web->send(302, "text/plain", "");
}

}  // namespace

const char* apName() {
  if (!s_ap[0]) {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(s_ap, sizeof(s_ap), "%s%02X%02X", cfg::AP_PREFIX, mac[4], mac[5]);
  }
  return s_ap;
}

void start() {
  if (s_active) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(apName());
  if (!s_dns) s_dns = new DNSServer();
  s_dns->setErrorReplyCode(DNSReplyCode::NoError);
  s_dns->start(53, "*", IPAddress(192, 168, 4, 1));
  if (!s_web) {
    s_web = new WebServer(80);
    s_web->on("/", HTTP_GET, handleRoot);
    s_web->on("/save", HTTP_POST, handleSave);
    s_web->onNotFound(handleRedirect);  // captive-portal probes land here
  }
  s_web->begin();
  WiFi.scanNetworks(true);
  s_active = true;
  Serial.printf("portal: started %s\n", apName());
}

void stop() {
  if (!s_active) return;
  s_web->stop();
  s_dns->stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  s_active = false;
  Serial.println("portal: stopped");
}

bool active() { return s_active; }

void loop() {
  if (!s_active) return;
  s_dns->processNextRequest();
  s_web->handleClient();
}

bool take(Submission& out) {
  if (!s_hasSub) return false;
  out = s_sub;
  s_hasSub = false;
  return true;
}

}  // namespace portal
