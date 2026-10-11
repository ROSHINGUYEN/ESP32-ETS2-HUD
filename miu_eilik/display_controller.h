#ifndef DISPLAY_CONTROLLER_H
#define DISPLAY_CONTROLLER_H

#include "config.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Guard macro RoboEyes va Adafruit
#undef DEFAULT
#undef TIRED
#undef ANGRY
#undef HAPPY
#include "FluxGarage_RoboEyes.h"
#undef DEFAULT
#undef TIRED
#undef ANGRY
#undef HAPPY
#undef ON
#undef OFF
#undef N
#undef S
#undef E
#undef W
#undef NE
#undef SE
#undef SW
#undef NW

constexpr uint8_t MOOD_DEFAULT = 0;
constexpr uint8_t MOOD_TIRED   = 1;
constexpr uint8_t MOOD_ANGRY   = 2;
constexpr uint8_t MOOD_HAPPY   = 3;

// =====================================================================================
// TIME CONVERSION HELPERS (Tránh phụ thuộc mktime & timezone)
// =====================================================================================

time_t tmToEpoch(int y, int mo, int d, int h, int mi, int s);
void epochToTm(time_t epoch, int& y, int& mo, int& d, int& h, int& mi, int& s);

// =====================================================================================
// ETS2 / SIM RACING TELEMETRY DATA STRUCTURE
// =====================================================================================

struct Ets2TelemetryData {
  bool connected = false;
  bool paused = false;
  int speed = 0;              // km/h
  int speedLimit = 0;         // km/h (0: khong gioi han)
  int cruiseSpeed = 0;        // km/h
  bool cruiseOn = false;
  int gear = 0;               // >0: D1..D16, 0: N, <0: R1..R2
  float rpm = 0.0f;
  float rpmMax = 2500.0f;
  int fuelPct = 100;          // 0..100%
  bool fuelWarning = false;
  bool blinkerL = false;
  bool blinkerR = false;
  bool lightsBeamLow = false;
  bool lightsBeamHigh = false;
  bool parkBrake = false;
  bool airPressureWarn = false;
  int etaDistanceKm = 0;      // km
  int etaTimeMin = 0;         // phut
  int retarder = 0;           // 0..5 (retarder brake steps)
  bool wipers = false;        // gat mua
  bool beacon = false;        // den chop xoay noc
  bool motorBrake = false;    // phanh dong co
  int gameHour = -1;          // gio dong ho trong game (-1: chua co)
  int gameMin = -1;           // phut dong ho trong game
  char eventText[32] = "";    // thong bao toast: "-550 EUR", "RETARDER: 2", "GAT MUA: BAT"
  unsigned long eventUntilMs = 0;
  unsigned long lastUpdateMs = 0;
};

// =====================================================================================
// FARMING SIMULATOR 25 (FS25) TELEMETRY DATA STRUCTURE
// =====================================================================================

struct Fs25TelemetryData {
  bool connected = false;
  int speed = 0;               // km/h
  int maxSpeed = 50;          // km/h (cho thanh vach toc do)
  int rpm = 0;                // RPM
  int rpmMax = 2200;
  int cruiseSpeed = 0;        // km/h
  bool cruiseOn = false;      // Ga tu dong dang bat
  int fuelPct = 100;          // 0..100% nhien lieu diesel
  int fillLevelPct = 0;       // 0..100% thung chua lua/nong san/phan bon
  int damagePct = 0;          // 0..100% hao mon may moc
  float operatingHours = 0.0f;// gio may chay (h)

  // Trang thai cac chuc nang dang bat (Active Functions)
  bool beacon = false;            // Den chop xoay vang (Rotating beacon)
  bool workLight = false;         // Den lam viec truoc/sau (Work lights)
  bool highBeam = false;          // Den pha xa
  bool blinkerL = false;          // Xi-nhan trai
  bool blinkerR = false;          // Xi-nhan phai
  bool implementLowered = false;  // Ha dan nong cu (cay, bua, gieo)
  bool implementTurnedOn = false; // Bat dan may hoat dong (PTO, guong gat)
  bool pipeUnfolded = false;      // Mo ong xa hat ra
  bool motorRunning = true;       // No may

  unsigned long lastUpdateMs = 0;
};

// =====================================================================================
// DISPLAY MANAGER (OLED SSD1306 128x64 - Đồng hồ, RoboEyes, Visualizer, Stream, Game)
// =====================================================================================

class DisplayManager {
public:
  DisplayMode mode = DISP_EYES;
  VizStyle viz = VIZ_BEAT_EYES;
  unsigned long lastFrame = 0;
  float fps = 0;
  uint16_t frameCount = 0;
  unsigned long fpsTimer = 0;
  uint8_t streamBuf[2][1024];
  volatile uint8_t streamWriteIdx = 0;
  volatile uint8_t streamReadIdx  = 1;
  volatile bool streamNew = false;
  unsigned long streamLastMs = 0;
  unsigned long lastPresence = 0;
  unsigned long bootMs = 0;
  float tempC = 27.5f;

  // ETS2 Game Telemetry
  Ets2TelemetryData gameTelem;
  unsigned long gameLastPacketMs = 0;
  bool gameModeLocked = false;

  // FS25 Game Telemetry
  Fs25TelemetryData fs25Telem;
  unsigned long fs25LastPacketMs = 0;

  int clockYear  = 2026;
  int clockMonth = 10;
  int clockDay   = 9;
  int clockHour  = 20;
  int clockMin   = 0;
  int clockSec   = 0;
  int clockWday  = 5;

  void begin();
  void drawBootLogo(const char* name = "ROSHI", int durationMs = 4500);
  static bool isLeap(int y);
  static int daysInMonth(int y, int m);
  void initClock();
  void setTime(int h, int m, int s);
  void setDate(int d, int m, int y);
  void updateClock();
  void notifyPresence();

  void drawSun(int16_t cx, int16_t cy);
  void drawMoon(int16_t cx, int16_t cy);
  void drawStars(int16_t x, int16_t y);
  void drawThermometer(int16_t x, int16_t y);
  void drawClock();
  void drawEyes();
  void drawVisual();
  void drawStream();
  // Game Splash Screen
  unsigned long ets2SplashUntilMs = 0;
  unsigned long fs25SplashUntilMs = 0;
  void triggerEts2Splash();
  void triggerFs25Splash();
  void drawEts2SplashScreen();
  void drawFs25SplashScreen();

  void drawGameDashboard();
  void updateGameTelemetry(const Ets2TelemetryData& data);
  void triggerEvent(const char* text, unsigned long durationMs = 2000);
  void drawEventToast();
  void drawFs25Dashboard();
  void updateFs25Telemetry(const Fs25TelemetryData& data);
  void setStreamFrame(const uint8_t* bmp);
  void update();
  const char* modeStr() const;
};

extern Adafruit_SSD1306 oled;
extern RoboEyes<Adafruit_SSD1306> roboEyes;
extern DisplayManager display;

#endif // DISPLAY_CONTROLLER_H
