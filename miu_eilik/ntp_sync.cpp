#include "ntp_sync.h"
#include "display_controller.h"

#if ENABLE_WIFI

NtpSync ntp;

// =====================================================================================
// NTP SYNC IMPLEMENTATION
// =====================================================================================

void NtpSync::begin() {
  prefs.begin(NVS_NAMESPACE, true);
  enabled = prefs.getBool("ntp_on", false);
  String s = prefs.getString("ntp_ssid", "");
  String p = prefs.getString("ntp_pass", "");
  if (s.length() > 0) ssid = strdup(s.c_str());
  if (p.length() > 0) pass = strdup(p.c_str());
  prefs.end();

  if (enabled) {
    LOG_PRINTF("[NTP] Khoi dong che do NTP (SSID: %s)\n", ssid);
    startConnect();
  } else {
    LOG_PRINTLN(F("[NTP] NTP tat mac dinh. Gui 'NTP=ON' de bat."));
  }
}

void NtpSync::setEnabled(bool en) {
  if (en == enabled) return;
  enabled = en;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putBool("ntp_on", en);
  prefs.end();
  if (en) startConnect();
  else stopWifi();
  LOG_PRINTF("[NTP] NTP %s\n", en ? "BAT" : "TAT");
}

void NtpSync::setCredentials(const char* newSsid, const char* newPass) {
  if (newSsid) ssid = strdup(newSsid);
  if (newPass) pass = strdup(newPass);
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString("ntp_ssid", newSsid ? newSsid : "");
  prefs.putString("ntp_pass", newPass ? newPass : "");
  prefs.end();
  LOG_PRINTF("[NTP] Luu WiFi credentials: %s\n", ssid);
  if (enabled) {
    stopWifi();
    startConnect();
  }
}

void NtpSync::update() {
  if (!enabled) return;
  unsigned long now = millis();
  if (synced && (now - lastSyncMs) < 1800000UL) return;
  if (now - lastAttemptMs < 15000UL) return;
  lastAttemptMs = now;

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiState != 1) {
      startConnect();
      return;
    }
    if (now - lastAttemptMs < 15000UL) return;
  }

  if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
    time_t nowT = time(nullptr);
    if (nowT > 1735689600) {
      struct timeval tv = { .tv_sec = nowT, .tv_usec = 0 };
      settimeofday(&tv, NULL);
      lastSyncMs = millis();
      synced = true;
      failCount = 0;
      display.updateClock();
      LOG_PRINTF("[NTP] Sync thanh cong! Epoch=%lu\n", (unsigned long)nowT);
    }
  } else if (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) {
    LOG_PRINTLN(F("[NTP] Sync dang tien hanh..."));
  }
}

void NtpSync::printStatus() {
  LOG_PRINTF("[NTP] enabled=%d synced=%d wifiState=%d failCount=%d\n",
             enabled, synced, wifiState, failCount);
  if (synced) {
    unsigned long agoMs = millis() - lastSyncMs;
    LOG_PRINTF("[NTP] Last sync: %lu phut truoc\n", agoMs / 60000);
  }
}

void NtpSync::startConnect() {
  LOG_PRINTF("[NTP] Dang ket noi WiFi: %s ...\n", ssid);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, pass);
  wifiState = 0;
  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 5000) {
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiState = 1;
    LOG_PRINTF("[NTP] WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char*)ntp1);
    sntp_setservername(1, (char*)ntp2);
    sntp_set_time_sync_notification_cb(nullptr);
    sntp_init();
    LOG_PRINTLN(F("[NTP] SNTP started, doi sync..."));
  } else {
    wifiState = -1;
    failCount++;
    LOG_PRINTF("[NTP] WiFi connect that bai (lan %d)\n", failCount);
    if (failCount >= 3) {
      LOG_PRINTLN(F("[NTP] 3 lan fail -> tam dung 5 phut"));
      failCount = 0;
      lastAttemptMs = millis() + 300000UL;
    }
  }
}

void NtpSync::stopWifi() {
  sntp_stop();
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_AP);
  wifiState = -1;
  synced = false;
  LOG_PRINTLN(F("[NTP] WiFi STA da tat, duy tri SoftAP"));
}

#endif // ENABLE_WIFI
