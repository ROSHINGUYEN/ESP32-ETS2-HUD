#include "usb_protocol.h"
#include "display_controller.h"
#include "emoji_controller.h"

UsbLink usb;

// =====================================================================================
// USB LINK IMPLEMENTATION
// =====================================================================================

void UsbLink::update() {
  while (Serial.available()) {
    uint8_t b = Serial.read();

    if (state == S_SYNC1 && b != USB_SYNC_1) {
      if (b == '\n') {
        asciiBuf[asciiLen] = '\0';
        processSerialLine(String(asciiBuf));
        asciiLen = 0;
      } else if (b != '\r') {
        if (asciiLen < sizeof(asciiBuf) - 1) {
          asciiBuf[asciiLen++] = (char)b;
        }
      }
      continue;
    }

    switch (state) {
      case S_SYNC1:
        if (b == USB_SYNC_1) state = S_SYNC2;
        break;
      case S_SYNC2:
        if (b == USB_SYNC_2) { state = S_TYPE; xorSum = 0; }
        else {
          state = S_SYNC1;
          if (b != USB_SYNC_1 && asciiLen < sizeof(asciiBuf) - 1) {
            asciiBuf[asciiLen++] = (char)b;
          }
        }
        break;
      case S_TYPE:
        pktType = b; xorSum ^= b; state = S_LENH;
        break;
      case S_LENH:
        pktLen = ((uint16_t)b) << 8; xorSum ^= b; state = S_LENL;
        break;
      case S_LENL:
        pktLen |= b; xorSum ^= b; rxCount = 0;
        state = (pktLen > 1040) ? S_SYNC1 : (pktLen == 0 ? S_CHKSUM : S_PAYLOAD);
        break;
      case S_PAYLOAD:
        payload[rxCount++] = b; xorSum ^= b;
        if (rxCount >= pktLen) state = S_CHKSUM;
        break;
      case S_CHKSUM:
        if (b == xorSum) handle();
        state = S_SYNC1;
        break;
    }
  }
}

void UsbLink::processJson(const char* json, size_t len) {
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, json, len);
  if (err != DeserializationError::Ok) {
    LOG_PRINTF("[JSON ERR] %s (len=%u)\n", err.c_str(), (unsigned int)len);
    return;
  }

  // 1. Uu tien tuyet doi: Kiem tra neu la goi du lieu telemetry game ETS2 / ATS tu Desktop Miu
  if (doc["game"] == "ets2" || doc.containsKey("spd") || doc.containsKey("gear") || doc.containsKey("truck")) {
    Ets2TelemetryData t;
    t.connected = doc.containsKey("conn") ? (doc["conn"].as<int>() != 0) : true;
    t.paused = doc.containsKey("pause") ? (doc["pause"].as<int>() != 0) : false;
    t.speed = doc["spd"] | 0;
    t.speedLimit = doc["lim"] | 0;
    t.cruiseSpeed = doc["cc"] | 0;
    t.cruiseOn = (doc["cc_on"].as<int>() != 0) || (t.cruiseSpeed > 0);
    t.gear = doc["gear"] | 0;
    t.rpm = doc["rpm"] | 0.0f;
    t.rpmMax = doc["rpm_max"] | 2500.0f;
    t.fuelPct = doc["fuel"] | 100;
    t.fuelWarning = (doc["fuel_warn"].as<int>() != 0);
    t.blinkerL = (doc["bl_l"].as<int>() != 0);
    t.blinkerR = (doc["bl_r"].as<int>() != 0);
    t.lightsBeamLow = (doc["low"].as<int>() != 0);
    t.lightsBeamHigh = (doc["high"].as<int>() != 0);
    t.parkBrake = (doc["park"].as<int>() != 0);
    t.airPressureWarn = (doc["air"].as<int>() != 0);
    t.etaDistanceKm = doc["eta_d"] | 0;
    t.etaTimeMin = doc["eta_t"] | 0;
    t.retarder = doc["ret"] | 0;
    t.wipers = (doc["wip"].as<int>() != 0);
    t.beacon = (doc["bcn"].as<int>() != 0);
    t.motorBrake = (doc["mbrk"].as<int>() != 0);
    t.gameHour = doc["gh"] | -1;
    t.gameMin = doc["gm"] | -1;
    if (doc.containsKey("evt")) {
      const char* ev = doc["evt"];
      if (ev && strlen(ev) > 0) {
        strncpy(t.eventText, ev, sizeof(t.eventText) - 1);
        t.eventText[sizeof(t.eventText) - 1] = '\0';
        t.eventUntilMs = millis() + (doc["evt_dur"] | 2200);
      }
    }
    t.lastUpdateMs = millis();
    display.updateGameTelemetry(t);
    return;
  }

  // 2. Kiem tra neu la goi du lieu telemetry Farming Simulator 25
  if (doc["game"] == "fs25" || doc.containsKey("fs25") || doc.containsKey("pto") || doc.containsKey("pipe") || doc.containsKey("maxSpd")) {
    JsonObject d = doc.containsKey("fs25") ? doc["fs25"].as<JsonObject>() : doc.as<JsonObject>();
    Fs25TelemetryData data;
    data.connected = true;
    data.speed = d["speed"] | 0;
    data.maxSpeed = d["maxSpd"] | 50;
    data.rpm = d["rpm"] | 0;
    data.rpmMax = d["rpmMax"] | 2200;
    data.cruiseSpeed = d["cru"] | 0;
    data.cruiseOn = d["cruOn"] | false;
    data.fuelPct = d["fuel"] | 100;
    data.fillLevelPct = d["fill"] | 0;
    data.damagePct = d["dmg"] | 0;
    data.operatingHours = d["hr"] | 0.0f;
    data.beacon = d["bcn"] | false;
    data.workLight = d["wrk"] | false;
    data.blinkerL = d["turnL"] | false;
    data.blinkerR = d["turnR"] | false;
    data.implementLowered = d["down"] | false;
    data.implementTurnedOn = d["pto"] | false;
    data.pipeUnfolded = d["pipe"] | false;
    data.motorRunning = d["motor"] | true;
    display.updateFs25Telemetry(data);
    return;
  }

  for (auto kv : doc.as<JsonObject>()) {
    uint32_t k = hashKey(kv.key().c_str());
    switch (k) {
      case CmdHash::EMOTION:
        if (kv.value().is<const char*>()) {
          emojiController.ExecuteCommand(kv.value().as<String>());
        } else if (kv.value().is<int>()) {
          roboEyes.setMood(constrain(kv.value().as<int>(), 0, 3));
        }
        break;
      case CmdHash::GESTURE:
        if (kv.value().is<int>()) {
          int g = kv.value().as<int>();
          if (g == 0) emojiController.HeadNod();
          else if (g == 1) emojiController.HeadShake();
          else if (g == 2) emojiController.LookLeft();
          else if (g == 3) emojiController.LookRight();
          else emojiController.Center();
        }
        break;
      case CmdHash::MODE:
        if (kv.value().is<const char*>()) {
          String mVal = kv.value().as<String>();
          if (mVal == "GAME" || mVal == "ETS2") {
            display.mode = DISP_GAME;
            display.gameModeLocked = true;
            display.triggerEts2Splash();
          } else if (mVal == "FS25") {
            display.mode = DISP_FS25;
            display.gameModeLocked = true;
            display.triggerFs25Splash();
          } else {
            display.gameModeLocked = false;
            uint32_t mh = hashKey(kv.value().as<const char*>());
            switch (mh) {
              case ModeHash::MODE_SLEEP:   emojiController.Sleep(); break;
              case ModeHash::MODE_CALM:    emojiController.Center(); break;
              case ModeHash::MODE_CURIOUS: emojiController.Confused(); break;
              case ModeHash::MODE_PLAYFUL: emojiController.Happy(); break;
            }
          }
        }
        break;
    }
  }

  if (doc["actions"].is<JsonArray>()) {
    String combined = "";
    JsonArray arr = doc["actions"].as<JsonArray>();
    for (JsonVariant v : arr) {
      if (combined.length() > 0) combined += ",";
      combined += v.as<String>();
    }
    async_sequent_act(combined);
  } else if (doc["act"].is<const char*>()) {
    async_sequent_act(doc["act"].as<const char*>());
  }
}

void UsbLink::handle() {
  display.notifyPresence();
  if (pktType == PKT_OLED_BITMAP && pktLen == 1024) {
    display.mode = DISP_STREAM;
    display.setStreamFrame(payload);
  } else if ((pktType == PKT_CMD_JSON || pktType == PKT_GAME_TELEMETRY || pktType == PKT_FS25_TELEMETRY) && pktLen > 0 && pktLen < sizeof(payload)) {
    payload[pktLen] = '\0';
    processJson((const char*)payload, pktLen);
  } else if (pktType == PKT_TELEMETRY_REQ) {
    uint8_t res[11] = {
      USB_SYNC_1, USB_SYNC_2, 0x84, 0x00, 0x06,
      90, 90,
      (uint8_t)roundf(display.fps), 0,
      (uint8_t)(gUnderrunCount & 0xFF), 0
    };
    res[10] = res[5] ^ res[6] ^ res[7] ^ res[8] ^ res[9];
    Serial.write(res, 11);
  }
}
