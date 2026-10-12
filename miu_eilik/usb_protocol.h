#ifndef USB_PROTOCOL_H
#define USB_PROTOCOL_H

#include "config.h"
#include <ArduinoJson.h>

// =====================================================================================
// USB BINARY PROTOCOL + JSON (hash-dispatched)
// =====================================================================================

class UsbLink {
public:
  enum State : uint8_t { S_SYNC1, S_SYNC2, S_TYPE, S_LENH, S_LENL, S_PAYLOAD, S_CHKSUM };
  State state = S_SYNC1;
  uint8_t pktType = 0;
  uint16_t pktLen = 0, rxCount = 0;
  uint8_t payload[1050];
  uint8_t xorSum = 0;

  char asciiBuf[1280];
  uint16_t asciiLen = 0;

  void update();
  void processJson(const char* json, size_t len);
  void handle();
};

extern UsbLink usb;

#endif // USB_PROTOCOL_H
