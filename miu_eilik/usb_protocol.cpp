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
  static StaticJsonDocument<2048> doc;
  doc.clear();
  DeserializationError err = deserializeJson(doc, json, len);
  if (err != DeserializationError::Ok) {
    LOG_PRINTF("[JSON ERR] %s (len=%u)\n", err.c_str(), (unsigned int)len);
    return;
  }

  // 1. Uu tien tuyet doi: Kiem tra neu la goi du lieu telemetry game ETS2 / ATS tu Desktop Miu
  if (doc["game"] == "ets2" || doc.containsKey("spd") || doc.containsKey("gear") || doc.containsKey("truck")) {
    Ets2TelemetryData t;
    t.connected = doc.containsKey("conn") ? (doc["conn"].as<int>() != 0) : false;
    t.paused = doc.containsKey("pause") ? (doc["pause"].as<int>() != 0) : false;
    t.loading = doc.containsKey("load") ? (doc["load"].as<int>() != 0) : false;
    t.loadPct = doc["load_pct"] | 0;
    t.speed = doc["spd"] | 0;
    t.speedLimit = doc["lim"] | 0;
    t.cruiseSpeed = doc["cc"] | 0;
    t.cruiseOn = (doc["cc_on"].as<int>() != 0) || (t.cruiseSpeed > 0);
    t.gear = doc["gear"] | 0;
    if (doc.containsKey("g_str")) {
      const char* gs = doc["g_str"];
      if (gs && strlen(gs) > 0) {
        strncpy(t.gearStr, gs, sizeof(t.gearStr) - 1);
        t.gearStr[sizeof(t.gearStr) - 1] = '\0';
      }
    } else {
      if (t.gear > 0) snprintf(t.gearStr, sizeof(t.gearStr), "D%d", t.gear);
      else if (t.gear == 0) snprintf(t.gearStr, sizeof(t.gearStr), "N");
      else snprintf(t.gearStr, sizeof(t.gearStr), "R%d", abs(t.gear));
    }
    if (doc.containsKey("g_type")) {
      const char* gt = doc["g_type"];
      if (gt && strlen(gt) > 0) t.gearType = gt[0];
    } else {
      t.gearType = (t.gear > 0) ? 'D' : ((t.gear == 0) ? 'N' : 'R');
    }
    t.rpm = doc["rpm"] | 0.0f;
    t.rpmMax = doc["rpm_max"] | 2500.0f;
    t.fuelPct = doc["fuel"] | 100;
    t.fuelLiters = doc["fuel_l"] | 0;
    t.fuelCapacity = doc["fuel_cap"] | 0;
    t.fuelWarning = (doc["fuel_warn"].as<int>() != 0);
    t.fuelRate = doc["fuel_r"] | 0.0f;
    t.truckWearPct = doc.containsKey("trk_w") ? doc["trk_w"].as<int>() : (int)round(doc["wear"].as<float>() * 100.0f);
    t.trailerWearPct = doc.containsKey("trl_w") ? doc["trl_w"].as<int>() : -1;
    t.cargoWearPct = doc.containsKey("crg_w") ? doc["crg_w"].as<int>() : -1;
    t.cargoWeight = doc["crg_wgt"] | 0.0f;
    t.trailerAttached = (doc["trl_on"].as<int>() != 0);
    t.odometerKm = doc["odo"] | 0;
    t.blinkerL = (doc["bl_l"].as<int>() != 0);
    t.blinkerR = (doc["bl_r"].as<int>() != 0);
    t.lightsBeamLow = (doc["low"].as<int>() != 0);
    t.lightsBeamHigh = (doc["high"].as<int>() != 0);
    t.lightsParking = (doc["park_l"].as<int>() != 0);
    t.parkBrake = (doc["park"].as<int>() != 0);
    t.airPressureWarn = (doc["air"].as<int>() != 0);
    t.airPressure = doc["air_p"] | 0.0f;
    t.oilTemp = doc["oil_t"] | 0.0f;
    t.waterTemp = doc["water_t"] | 0.0f;
    t.brakeTemp = doc["brake_t"] | 0.0f;
    t.diffLock = (doc["diff_l"].as<int>() != 0);
    t.liftAxle = (doc["lift_a"].as<int>() != 0);
    t.brakeLights = (doc["brk_l"].as<int>() != 0);
    t.reverseLights = (doc["rev_l"].as<int>() != 0);
    t.etaDistanceKm = doc["eta_d"] | 0;
    t.etaTimeMin = doc["eta_t"] | 0;
    t.retarder = doc["ret"] | 0;
    t.wipers = (doc["wip"].as<int>() != 0);
    t.wiperLevel = doc["wip_lvl"] | (t.wipers ? 1 : 0);
    t.horn = (doc["horn"].as<int>() != 0);
    t.engineOn = doc.containsKey("eng") ? (doc["eng"].as<int>() != 0) : true;
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
          } else if (mVal == "EYES" || mVal == "ROBOEYES") {
            display.mode = DISP_EYES;
            display.gameModeLocked = false;
            display.gameTelem.loading = false;
            display.gameTelem.connected = false;
            display.ets2SplashUntilMs = 0;
            emojiController.Center();
          } else {
            display.gameModeLocked = false;
            display.gameTelem.loading = false;
            display.gameTelem.connected = false;
            display.ets2SplashUntilMs = 0;
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
  } else if ((pktType == PKT_CMD_JSON || pktType == PKT_GAME_TELEMETRY) && pktLen > 0 && pktLen < sizeof(payload)) {
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
