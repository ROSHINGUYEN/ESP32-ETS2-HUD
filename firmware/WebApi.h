/**
 * =====================================================================================
 *  WebApi.h — SoftAP + Captive Portal + REST API + LittleFS
 *  Endpoints: /api/head, /api/nudge, /api/center, /api/emotion,
 *             /api/gesture, /api/mode, /api/calib, /api/state
 * =====================================================================================
 */
#pragma once
#include <Arduino.h>

#ifdef N
  #undef N
#endif
#ifdef S
  #undef S
#endif
#ifdef E
  #undef E
#endif
#ifdef W
  #undef W
#endif
#ifdef NE
  #undef NE
#endif
#ifdef NW
  #undef NW
#endif
#ifdef SE
  #undef SE
#endif
#ifdef SW
  #undef SW
#endif

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "HeadServo.h"
#include "Emotion.h"
#include "Gestures.h"
#include "Autonomy.h"

class WebApi {
public:
  WebApi();
  void begin(HeadServo* h, Emotion* e, Gestures* g, Autonomy* a);
  void tick();
  bool isApRunning() const { return apRunning; }
  IPAddress apIp() const { return WiFi.softAPIP(); }
  uint8_t clients() const { return WiFi.softAPgetStationNum(); }
  // Body buffer size (public)
  static constexpr size_t MAX_BODY = 512;

private:
  AsyncWebServer server;
  HeadServo* _head;
  Emotion*   _emotion;
  Gestures*  _gest;
  Autonomy*  _auto;
  bool       apRunning;
  unsigned long lastWifiRetryMs;

  void setupRoutes();
  void handleGetState(AsyncWebServerRequest* req);
  void handleApi(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void sendCors(AsyncWebServerResponse* resp);
  void sendJson(AsyncWebServerRequest* req, const String& json, int code = 200);
  void sendOk(AsyncWebServerRequest* req) { sendJson(req, "{\"ok\":true}"); }
};
