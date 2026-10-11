#ifndef ETS2_CLIENT_H
#define ETS2_CLIENT_H

#include "config.h"

#if ENABLE_WIFI
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "display_controller.h"

// =====================================================================================
// ETS2 TELEMETRY WIFI CLIENT (Ported & Optimized from megatops/ets2_lcd_dashboard)
// =====================================================================================

class Ets2Client {
public:
  bool enabled = false;
  String serverIp = ""; // IP hoặc IP:Port (VD: 192.168.1.100:25555)
  unsigned long lastPollMs = 0;
  uint16_t pollIntervalMs = 150; // 150ms ~ 6-7 Hz qua WiFi HTTP
  uint32_t failCount = 0;

  void begin();
  void setServer(const String& ipPort, bool enable = true);
  void setEnabled(bool enable);
  void update();
  bool isConnected() const;
};

extern Ets2Client ets2Wifi;
#endif // ENABLE_WIFI

#endif // ETS2_CLIENT_H
