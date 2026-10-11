#ifndef NTP_SYNC_H
#define NTP_SYNC_H

#include "config.h"

#if ENABLE_WIFI
#include <WiFi.h>
#include <esp_sntp.h>

// =====================================================================================
// NTP TIME SYNC (WiFi + SNTP)
// =====================================================================================

class NtpSync {
public:
  const char* ssid     = "YOUR_WIFI_SSID";
  const char* pass     = "YOUR_WIFI_PASS";
  const char* ntp1     = "pool.ntp.org";
  const char* ntp2     = "time.nist.gov";
  const long  gmtOffset_sec = 7 * 3600;
  const int   daylightOffset_sec = 0;

  bool enabled = false;
  bool synced = false;
  unsigned long lastSyncMs = 0;
  unsigned long lastAttemptMs = 0;
  int8_t wifiState = -1;
  uint8_t failCount = 0;

  void begin();
  void setEnabled(bool en);
  void setCredentials(const char* newSsid, const char* newPass);
  void update();
  void printStatus();

private:
  Preferences prefs;
  void startConnect();
  void stopWifi();
};

extern NtpSync ntp;

#endif // ENABLE_WIFI

#endif // NTP_SYNC_H
