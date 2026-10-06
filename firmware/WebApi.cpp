/**
 * WebApi.cpp — SoftAP + Captive Portal + REST API
 *
 * Cách đăng ký route đúng với ESPAsyncWebServer-esphome v3.4.0:
 *   1 arg  (request):  server.on("/path", METHOD, [](ArRequestHandlerFunction){});
 *   3 args (req+upload+body): dùng onRequestBody riêng thay vì 5-arg on()
 *   5 args (req+upload+body): KHÔNG tồn tại trong phiên bản này
 */
#include "WebApi.h"
#include "config.h"
#include <ArduinoJson.h>
#include <DNSServer.h>

static DNSServer g_dns;

WebApi::WebApi()
  : server(80), _head(nullptr), _emotion(nullptr),
    _gest(nullptr), _auto(nullptr), apRunning(false), lastWifiRetryMs(0) {}

void WebApi::begin(HeadServo* h, Emotion* e, Gestures* g, Autonomy* a) {
  _head = h; _emotion = e; _gest = g; _auto = a;

  if (!LittleFS.begin(true)) {
    Serial0.println(F("[WARN] LittleFS mount failed"));
  } else {
    Serial0.println(F("[OK]   LittleFS mounted"));
  }

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL,
                         WIFI_AP_HIDDEN, WIFI_AP_MAX_CLIENTS);
  if (ok) {
    apRunning = true;
    Serial0.printf("[OK]   AP %s  IP=%s\n",
                   WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
  } else {
    Serial0.println(F("[FAIL] AP start failed"));
  }
  g_dns.start(53, "*", WiFi.softAPIP());

  setupRoutes();
  server.begin();
}

void WebApi::tick() {
  g_dns.processNextRequest();
  if (!apRunning && millis() - lastWifiRetryMs > 30000) {
    lastWifiRetryMs = millis();
    if (WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL,
                     0, WIFI_AP_MAX_CLIENTS)) {
      apRunning = true;
      g_dns.start(53, "*", WiFi.softAPIP());
      Serial0.println(F("[AP] restarted"));
    }
  }
}

void WebApi::sendCors(AsyncWebServerResponse* resp) {
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

void WebApi::sendJson(AsyncWebServerRequest* req, const String& json, int code) {
  auto resp = req->beginResponse(code, "application/json", json);
  sendCors(resp);
  req->send(resp);
}

// =================== BODY BUFFER (static, shared) ===================
static struct {
  char  buf[512];
  size_t pos;
  bool  overflow;
} _bodyCtx;

static void bodyReset() { _bodyCtx.pos = 0; _bodyCtx.overflow = false; }
static void bodyAppend(const uint8_t* d, size_t l) {
  if (_bodyCtx.overflow) return;
  if (_bodyCtx.pos + l < sizeof(_bodyCtx.buf) - 1) {
    memcpy(_bodyCtx.buf + _bodyCtx.pos, d, l);
    _bodyCtx.pos += l;
  } else { _bodyCtx.overflow = true; }
}
static bool bodyDone() { _bodyCtx.buf[_bodyCtx.pos] = '\0'; return _bodyCtx.pos > 0 && !_bodyCtx.overflow; }

// =================== ROUTES ===================
void WebApi::setupRoutes() {
  // Static files
  server.serveStatic("/", LittleFS, "/")
    .setDefaultFile("index.html")
    .setCacheControl("max-age=3600");

  // ---- HEAD: request-only handler → body qua onRequestBody ----
  server.on("/api/head", HTTP_POST, [this](AsyncWebServerRequest* r) {
    if (!_head) { sendJson(r, "{\"ok\":false,\"err\":\"no_head\"}", 500); return; }
    if (!bodyDone()) { sendJson(r, "{\"ok\":false,\"err\":\"no_body\"}", 400); return; }
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, _bodyCtx.buf)) { sendJson(r, "{\"ok\":false}", 400); return; }
    float pan  = doc["pan"]  | PAN_CENTER_DEG;
    float tilt = doc["tilt"] | TILT_CENTER_DEG;
    _head->setTarget(constrain(pan, PAN_MIN_DEG, PAN_MAX_DEG),
                     constrain(tilt, TILT_MIN_DEG, TILT_MAX_DEG));
    if (_auto) _auto->notifyManual();
    sendJson(r, "{\"ok\":true}");
  });
  server.onRequestBody([](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
    (void)r;
    if (i == 0) bodyReset();
    bodyAppend(d, l);
  });

  // ---- NUDGE ----
  server.on("/api/nudge", HTTP_POST, [this](AsyncWebServerRequest* r) {
    if (_head) _head->nudge(5.0f, 0);
    if (_auto) _auto->notifyManual();
    sendJson(r, "{\"ok\":true}");
  });

  // ---- CENTER ----
  server.on("/api/center", HTTP_POST, [this](AsyncWebServerRequest* r) {
    if (_head) _head->center();
    if (_auto) _auto->notifyManual();
    sendJson(r, "{\"ok\":true}");
  });

  // ---- EMOTION ----
  server.on("/api/emotion", HTTP_POST, [this](AsyncWebServerRequest* r) {
    if (!bodyDone()) { sendJson(r, "{\"ok\":false}", 400); return; }
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, _bodyCtx.buf)) { sendJson(r, "{\"ok\":false}", 400); return; }
    const char* v = doc["value"] | "NEUTRAL";
    Mood m = MOOD_NEUTRAL;
    if      (!strcmp(v, "HAPPY"))    m = MOOD_HAPPY;
    else if (!strcmp(v, "ANGRY"))    m = MOOD_ANGRY;
    else if (!strcmp(v, "SLEEPY"))   m = MOOD_SLEEPY;
    else if (!strcmp(v, "CURIOUS"))  m = MOOD_CURIOUS;
    else if (!strcmp(v, "CONFUSED")) m = MOOD_CONFUSED;
    else if (!strcmp(v, "WAKE_UP"))  m = MOOD_WAKE_UP;
    if (_emotion) _emotion->setMood(m);
    if (_auto) _auto->notifyManual();
    sendJson(r, "{\"ok\":true}");
  });

  // ---- GESTURE ----
  server.on("/api/gesture", HTTP_POST, [this](AsyncWebServerRequest* r) {
    if (!bodyDone()) { sendJson(r, "{\"ok\":false}", 400); return; }
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, _bodyCtx.buf)) { sendJson(r, "{\"ok\":false}", 400); return; }
    const char* g = doc["value"] | "";
    GestureId gid = GES_NONE;
    if      (!strcmp(g, "YES"))        gid = GES_YES;
    else if (!strcmp(g, "NO"))         gid = GES_NO;
    else if (!strcmp(g, "LOOK_LEFT"))  gid = GES_LOOK_LEFT;
    else if (!strcmp(g, "LOOK_RIGHT")) gid = GES_LOOK_RIGHT;
    else if (!strcmp(g, "LOOK_UP"))    gid = GES_LOOK_UP;
    else if (!strcmp(g, "LOOK_DOWN"))  gid = GES_LOOK_DOWN;
    else if (!strcmp(g, "CURIOUS"))    gid = GES_CURIOUS;
    else if (!strcmp(g, "CONFUSED"))   gid = GES_CONFUSED;
    else if (!strcmp(g, "SLEEP"))      gid = GES_SLEEP;
    else if (!strcmp(g, "WAKE"))       gid = GES_WAKE;
    else if (!strcmp(g, "HAPPY"))     gid = GES_HAPPY;
    else if (!strcmp(g, "ANGRY"))     gid = GES_ANGRY;
    else if (!strcmp(g, "BREATH"))     gid = GES_BREATH;
    if (gid != GES_NONE && _gest) _gest->play(gid);
    if (_auto) _auto->notifyManual();
    sendJson(r, "{\"ok\":true}");
  });

  // ---- MODE ----
  server.on("/api/mode", HTTP_POST, [this](AsyncWebServerRequest* r) {
    if (!bodyDone()) { sendJson(r, "{\"ok\":false}", 400); return; }
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, _bodyCtx.buf)) { sendJson(r, "{\"ok\":false}", 400); return; }
    const char* m = doc["value"] | "CALM";
    AutonomyMode am = AM_CALM;
    if      (!strcmp(m, "SLEEP"))    am = AM_SLEEP;
    else if (!strcmp(m, "CALM"))     am = AM_CALM;
    else if (!strcmp(m, "CURIOUS")) am = AM_CURIOUS;
    else if (!strcmp(m, "PLAYFUL")) am = AM_PLAYFUL;
    if (_auto) _auto->setMode(am);
    sendJson(r, "{\"ok\":true}");
  });

  // ---- CALIB ----
  server.on("/api/calib", HTTP_POST, [this](AsyncWebServerRequest* r) {
    if (!_head) { sendJson(r, "{\"ok\":false}", 500); return; }
    if (!bodyDone()) { sendJson(r, "{\"ok\":false}", 400); return; }
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, _bodyCtx.buf)) { sendJson(r, "{\"ok\":false}", 400); return; }
    int8_t po = constrain((int8_t)(doc["pan_off"]  | 0), (int8_t)-30, (int8_t)30);
    int8_t to = constrain((int8_t)(doc["tilt_off"] | 0), (int8_t)-30, (int8_t)30);
    _head->saveCalibration(po, to);
    sendJson(r, "{\"ok\":true}");
  });

  // ---- STATE ----
  server.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest* r) {
    StaticJsonDocument<512> doc;
    if (_head) {
      doc["pan"]    = roundf(_head->getCurrentPan());
      doc["tilt"]   = roundf(_head->getCurrentTilt());
      doc["panT"]  = roundf(_head->getTargetPan());
      doc["tiltT"] = roundf(_head->getTargetTilt());
      doc["panOff"]  = _head->getOffsetPan();
      doc["tiltOff"] = _head->getOffsetTilt();
      doc["moving"] = _head->isMoving();
    }
    if (_emotion) doc["mood"] = _emotion->name();
    if (_auto)   doc["mode"] = (int)_auto->getMode();
    doc["uptime_s"] = millis() / 1000;
    doc["heap"]     = ESP.getFreeHeap();
    doc["min_heap"] = ESP.getMinFreeHeap();
    doc["clients"]  = clients();
    doc["ssid"]     = WIFI_AP_SSID;
    doc["ip"]       = apIp().toString();
    doc["fw"]       = FIRMWARE_VERSION;
    String out;
    serializeJson(doc, out);
    sendJson(r, out);
  });

  // ---- CAPTIVE PORTAL REDIRECT ----
  auto redir = [](AsyncWebServerRequest* r) {
    r->redirect("http://" + WiFi.softAPIP().toString() + "/");
  };
  server.on("/connecttest.txt",    HTTP_GET, redir);
  server.on("/generate_204",     HTTP_GET, redir);
  server.on("/fwlink",           HTTP_GET, redir);
  server.on("/hotspot-detect.html",HTTP_GET, redir);
  server.on("/ncsi.txt",         HTTP_GET, redir);

  // ---- FALLBACK ----
  server.onNotFound([this](AsyncWebServerRequest* r) {
    String host = r->header("Host");
    IPAddress apIP = WiFi.softAPIP();
    char apHost[24];
    snprintf(apHost, sizeof(apHost), "%d.%d.%d.%d", apIP[0], apIP[1], apIP[2], apIP[3]);
    bool captive = (host.indexOf(apHost) == -1 && host.indexOf("localhost") == -1);
    if (captive) {
      char buf[64];
      snprintf(buf, sizeof(buf), "http://%s/", apHost);
      r->redirect(buf);
    } else {
      r->redirect("/");
    }
  });
}
