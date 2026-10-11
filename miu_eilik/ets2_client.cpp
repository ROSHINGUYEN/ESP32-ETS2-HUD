#include "ets2_client.h"
#include "logger.h"

#if ENABLE_WIFI

Ets2Client ets2Wifi;

void Ets2Client::begin() {
  Preferences p;
  p.begin("ets2", true);
  serverIp = p.getString("ip", "");
  enabled = p.getBool("en", false);
  p.end();

  if (serverIp.length() > 0) {
    LOG_PRINTF("[ETS2 WIFI] Da nap cau hinh IP PC: %s (enabled=%d)\n", serverIp.c_str(), enabled);
  }
}

void Ets2Client::setServer(const String& ipPort, bool enable) {
  serverIp = ipPort;
  serverIp.trim();
  // Neu chua co port, tu dong them :25555
  if (serverIp.length() > 0 && serverIp.indexOf(':') < 0) {
    serverIp += ":25555";
  }
  enabled = enable;

  Preferences p;
  p.begin("ets2", false);
  p.putString("ip", serverIp);
  p.putBool("en", enabled);
  p.end();

  LOG_PRINTF("[ETS2 WIFI] Da cap nhat IP PC: %s, enabled=%d\n", serverIp.c_str(), enabled);
}

void Ets2Client::setEnabled(bool enable) {
  enabled = enable;
  Preferences p;
  p.begin("ets2", false);
  p.putBool("en", enabled);
  p.end();
  LOG_PRINTF("[ETS2 WIFI] Da %s che do keo telemetry truc tiep qua WiFi\n", enabled ? "BAT" : "TAT");
}

bool Ets2Client::isConnected() const {
  return enabled && (failCount < 5) && (display.gameTelem.connected);
}

void Ets2Client::update() {
  if (!enabled || serverIp.length() == 0) return;
  if (WiFi.status() != WL_CONNECTED) return;

  unsigned long now = millis();
  if (now - lastPollMs < pollIntervalMs) return;
  lastPollMs = now;

  // Neu USB Serial dang truyen du lieu lien tuc o FPS cao, uu tien USB va tam dung poll WiFi
  if (display.gameLastPacketMs > 0 && (now - display.gameLastPacketMs < 250)) {
    return;
  }

  HTTPClient http;
  String url = "http://" + serverIp + "/api/ets2/telemetry";
  http.begin(url);
  http.setConnectTimeout(150);
  http.setTimeout(250);

  int httpCode = http.GET();
  if (httpCode == 200) {
    failCount = 0;
    StaticJsonDocument<256> filter;
    filter["game"]["connected"] = true;
    filter["game"]["paused"] = true;
    filter["truck"]["speed"] = true;
    filter["truck"]["displayedGear"] = true;
    filter["truck"]["gear"] = true;
    filter["truck"]["engineRpm"] = true;
    filter["truck"]["engineRpmMax"] = true;
    filter["truck"]["fuel"] = true;
    filter["truck"]["fuelCapacity"] = true;
    filter["truck"]["fuelWarningOn"] = true;
    filter["truck"]["blinkerLeftActive"] = true;
    filter["truck"]["blinkerRightActive"] = true;
    filter["truck"]["lightsBeamLowOn"] = true;
    filter["truck"]["lightsBeamHighOn"] = true;
    filter["truck"]["parkBrakeOn"] = true;
    filter["truck"]["cruiseControlSpeed"] = true;
    filter["truck"]["cruiseControlOn"] = true;
    filter["navigation"]["speedLimit"] = true;
    filter["navigation"]["estimatedDistance"] = true;

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    if (!err) {
      Ets2TelemetryData t;
      t.connected = doc["game"]["connected"] | false;
      t.paused = doc["game"]["paused"] | false;
      t.speed = (int)roundf(doc["truck"]["speed"] | 0.0f);
      t.speedLimit = doc["navigation"]["speedLimit"] | 0;
      t.cruiseSpeed = (int)roundf(doc["truck"]["cruiseControlSpeed"] | 0.0f);
      t.cruiseOn = doc["truck"]["cruiseControlOn"] | (t.cruiseSpeed > 0);

      int g = doc["truck"]["displayedGear"] | 0;
      if (g == 0) g = doc["truck"]["gear"] | 0;
      t.gear = g;

      t.rpm = doc["truck"]["engineRpm"] | 0.0f;
      t.rpmMax = doc["truck"]["engineRpmMax"] | 2500.0f;

      float fuel = doc["truck"]["fuel"] | 0.0f;
      float cap = doc["truck"]["fuelCapacity"] | 1.0f;
      t.fuelPct = (cap > 0) ? constrain((int)roundf((fuel / cap) * 100.0f), 0, 100) : 100;
      t.fuelWarning = doc["truck"]["fuelWarningOn"] | false;

      t.blinkerL = doc["truck"]["blinkerLeftActive"] | false;
      t.blinkerR = doc["truck"]["blinkerRightActive"] | false;
      t.lightsBeamLow = doc["truck"]["lightsBeamLowOn"] | false;
      t.lightsBeamHigh = doc["truck"]["lightsBeamHighOn"] | false;
      t.parkBrake = doc["truck"]["parkBrakeOn"] | false;
      t.etaDistanceKm = (int)((doc["navigation"]["estimatedDistance"] | 0) / 1000);
      t.lastUpdateMs = now;

      display.updateGameTelemetry(t);
    }
  } else {
    failCount++;
  }
  http.end();
}

#endif // ENABLE_WIFI
