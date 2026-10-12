#include "emoji_controller.h"
#include "config.h"
#include <ArduinoJson.h>

EmojiController emojiController;

bool enable_act = false;
uint32_t act_interval = 20000UL;
unsigned long last_act_time = 0;

EmojiController::EmojiController() {
}

EmojiController::~EmojiController() {
    if (animation_task_handle_) {
        vTaskDelete(animation_task_handle_);
    }
    if (animation_queue_) {
        vQueueDelete(animation_queue_);
    }
}

void EmojiController::Initialize() {
    animation_queue_ = xQueueCreate(10, sizeof(AnimationMessage));
    last_act_time_ = millis();
    last_act_time = millis();

    xTaskCreate(
        AnimationTask,
        "TaibaiAnimTask",
        4096,
        this,
        2,
        &animation_task_handle_
    );

    Serial.println(F("[EMOJI] Khoi tao Animation Task thanh cong (Robot Eyes + Visualizer)"));
}

void EmojiController::AnimationTask(void* pvParameters) {
    EmojiController* self = static_cast<EmojiController*>(pvParameters);
    AnimationMessage msg;

    while (true) {
        if (xQueueReceive(self->animation_queue_, &msg, portMAX_DELAY) == pdTRUE) {
            self->is_busy_ = true;

            switch (msg.type) {
                case AnimationType::BLINK:
                    roboEyes.blink();
                    break;

                case AnimationType::HAPPY:
                    roboEyes.setMood(MOOD_HAPPY);
                    roboEyes.anim_laugh();
                    break;

                case AnimationType::SAD:
                    roboEyes.setMood(MOOD_TIRED);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;

                case AnimationType::ANGER:
                    roboEyes.setMood(MOOD_ANGRY);
                    break;

                case AnimationType::SURPRISE:
                    roboEyes.open();
                    roboEyes.anim_confused();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;

                case AnimationType::WAKEUP:
                    roboEyes.open();
                    roboEyes.blink();
                    break;

                case AnimationType::SLEEP:
                    roboEyes.setMood(MOOD_TIRED);
                    roboEyes.close();
                    break;

                case AnimationType::LOOK_LEFT:
                    roboEyes.setPosition(7);         // West (Trái)
                    vTaskDelay(pdMS_TO_TICKS(800));  // Mắt nhìn trái trong 800ms
                    roboEyes.setPosition(0);         // Center
                    break;

                case AnimationType::LOOK_RIGHT:
                    roboEyes.setPosition(3);          // East (Phải)
                    vTaskDelay(pdMS_TO_TICKS(800));
                    roboEyes.setPosition(0);          // Center
                    break;

                case AnimationType::HEAD_NOD:
                    roboEyes.setPosition(5);          // South
                    vTaskDelay(pdMS_TO_TICKS(300));
                    roboEyes.setPosition(1);          // North
                    vTaskDelay(pdMS_TO_TICKS(300));
                    roboEyes.setPosition(0);
                    break;

                case AnimationType::HEAD_SHAKE:
                    roboEyes.setPosition(7);          // West
                    vTaskDelay(pdMS_TO_TICKS(250));
                    roboEyes.setPosition(3);          // East
                    vTaskDelay(pdMS_TO_TICKS(250));
                    roboEyes.setPosition(0);
                    break;

                case AnimationType::HEAD_ROLL:
                    roboEyes.setPosition(1);
                    vTaskDelay(pdMS_TO_TICKS(150));
                    roboEyes.setPosition(3);
                    vTaskDelay(pdMS_TO_TICKS(150));
                    roboEyes.setPosition(5);
                    vTaskDelay(pdMS_TO_TICKS(150));
                    roboEyes.setPosition(7);
                    vTaskDelay(pdMS_TO_TICKS(150));
                    roboEyes.setPosition(0);
                    break;

                case AnimationType::CONFUSED:
                    roboEyes.anim_confused();
                    vTaskDelay(pdMS_TO_TICKS(400));
                    break;

                case AnimationType::LAUGHING:
                    roboEyes.setMood(MOOD_HAPPY);
                    roboEyes.anim_laugh();
                    break;

                case AnimationType::CRY:
                    roboEyes.setMood(MOOD_TIRED);
                    break;

                case AnimationType::LOVING:
                    roboEyes.setMood(MOOD_HAPPY);
                    break;

                case AnimationType::THINKING:
                    roboEyes.setPosition(8); // North-West
                    vTaskDelay(pdMS_TO_TICKS(600));
                    roboEyes.setPosition(0);
                    break;

                case AnimationType::RANDOM:
                    self->RandomAct();
                    break;

                default:
                    break;
            }

            self->is_busy_ = false;
            self->last_act_time_ = millis();
            last_act_time = millis();
        }
    }
}

void EmojiController::PlayAnimation(AnimationType type, int param) {
    if (!animation_queue_) return;
    AnimationMessage msg = { type, param };
    xQueueSend(animation_queue_, &msg, 0);
}

void EmojiController::Blink()     { PlayAnimation(AnimationType::BLINK); }
void EmojiController::Happy()     { PlayAnimation(AnimationType::HAPPY); }
void EmojiController::Sad()       { PlayAnimation(AnimationType::SAD); }
void EmojiController::Anger()     { PlayAnimation(AnimationType::ANGER); }
void EmojiController::Surprise()  { PlayAnimation(AnimationType::SURPRISE); }
void EmojiController::Wakeup()    { PlayAnimation(AnimationType::WAKEUP); }
void EmojiController::Sleep()     { PlayAnimation(AnimationType::SLEEP); }
void EmojiController::LookLeft()  { PlayAnimation(AnimationType::LOOK_LEFT); }
void EmojiController::LookRight() { PlayAnimation(AnimationType::LOOK_RIGHT); }
void EmojiController::HeadNod()   { PlayAnimation(AnimationType::HEAD_NOD); }
void EmojiController::HeadShake() { PlayAnimation(AnimationType::HEAD_SHAKE); }
void EmojiController::HeadRoll()  { PlayAnimation(AnimationType::HEAD_ROLL); }
void EmojiController::Confused()  { PlayAnimation(AnimationType::CONFUSED); }
void EmojiController::Laughing()  { PlayAnimation(AnimationType::LAUGHING); }
void EmojiController::Loving()    { PlayAnimation(AnimationType::LOVING); }
void EmojiController::Center()    { roboEyes.setPosition(0); roboEyes.setMood(MOOD_DEFAULT); }

void EmojiController::RandomAct() {
    int choice = random(5);
    if (choice == 0) {
        // Nhìn sang trái
        roboEyes.setPosition(7);
        vTaskDelay(pdMS_TO_TICKS(800));
        roboEyes.setPosition(0);
    } else if (choice == 1) {
        // Nhìn sang phải
        roboEyes.setPosition(3);
        vTaskDelay(pdMS_TO_TICKS(800));
        roboEyes.setPosition(0);
    } else if (choice == 2) {
        // Ngước lên
        roboEyes.setPosition(1);
        vTaskDelay(pdMS_TO_TICKS(800));
        roboEyes.setPosition(0);
    } else if (choice == 3) {
        // Cúi xuống
        roboEyes.setPosition(5);
        vTaskDelay(pdMS_TO_TICKS(800));
        roboEyes.setPosition(0);
    } else {
        // Chớp mắt tự nhiên
        roboEyes.blink();
    }
}

void EmojiController::Update() {
    if (!auto_enabled_ || is_busy_) return;
    if (display.mode == DISP_GAME) return;

    if (millis() - last_act_time_ >= auto_interval_ms_) {
        last_act_time_ = millis();
        RandomAct();
    }
}

void EmojiController::ExecuteCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd == "happy" || cmd == "eye_happy") {
        Happy();
    } else if (cmd == "sad" || cmd == "eye_sad") {
        Sad();
    } else if (cmd == "anger" || cmd == "angry" || cmd == "eye_anger") {
        Anger();
    } else if (cmd == "surprise" || cmd == "surprised" || cmd == "eye_surprise") {
        Surprise();
    } else if (cmd == "blink" || cmd == "eye_blink") {
        Blink();
    } else if (cmd == "sleep" || cmd == "eye_sleep") {
        Sleep();
    } else if (cmd == "laugh" || cmd == "eye_laugh" || cmd == "laughing") {
        Laughing();
    } else if (cmd == "confused" || cmd == "eye_confused") {
        Confused();
    } else if (cmd == "loving" || cmd == "love") {
        Loving();
    } else if (cmd == "wakeup" || cmd == "wake") {
        Wakeup();
    } else if (cmd == "left" || cmd == "look_left") {
        LookLeft();
    } else if (cmd == "right" || cmd == "look_right") {
        LookRight();
    } else if (cmd == "up" || cmd == "look_up") {
        roboEyes.setPosition(1);
    } else if (cmd == "down" || cmd == "look_down") {
        roboEyes.setPosition(5);
    } else if (cmd == "nod" || cmd == "head_nod") {
        HeadNod();
    } else if (cmd == "shake" || cmd == "head_shake") {
        HeadShake();
    } else if (cmd == "roll" || cmd == "head_roll") {
        HeadRoll();
    } else if (cmd == "disp_eyes" || cmd == "eyes" || cmd == "MODE=EYES" || cmd == "mode=eyes") {
        display.mode = DISP_EYES;
        display.gameModeLocked = false;
        display.gameTelem.loading = false;
        display.gameTelem.connected = false;
        display.ets2SplashUntilMs = 0;
        display.notifyPresence();
        roboEyes.setWidth(36, 36);
        roboEyes.setHeight(36, 36);
        roboEyes.open();
        roboEyes.setMood(MOOD_DEFAULT);
        Serial.println(F("[DISPLAY] Da chuyen sang Mat Robot (RoboEyes)"));
    } else if (cmd == "disp_clock" || cmd == "clock") {
        display.mode = DISP_CLOCK;
        display.gameModeLocked = false;
        Serial.println(F("[DISPLAY] Da chuyen sang Dong ho"));
    } else if (cmd == "disp_game" || cmd == "game") {
        display.mode = DISP_GAME;
        display.gameModeLocked = true;
        display.triggerEts2Splash();
        Serial.println(F("[DISPLAY] Da chuyen sang ETS2 Game Dashboard (Kem Splash Intro)"));
    } else if (cmd == "eye_center" || cmd == "center" || cmd == "head_center" || cmd == "look_center") {
        Center();
    } else if (cmd == "thinking") {
        PlayAnimation(AnimationType::THINKING);
    } else if (cmd == "help" || cmd == "HELP" || cmd == "?") {
        LOG_PRINTLN(F("====================================================="));
        LOG_PRINTLN(F("   DANH SACH LENH SERIAL ROSHI MIU EILIK             "));
        LOG_PRINTLN(F("====================================================="));
        LOG_PRINTLN(F(" 1. Che do Man hinh OLED:"));
        LOG_PRINTLN(F("    MODE=GAME / game       - Bat ETS2 Sim Dashboard"));
        LOG_PRINTLN(F("    MODE=EYES / eyes       - Bat bieu cam Mat Robot"));
        LOG_PRINTLN(F("    MODE=CLOCK / clock     - Bat Dong ho gio & ngay"));
        LOG_PRINTLN(F(" 2. Telemetry Game:"));
        LOG_PRINTLN(F("    ETS2={...}             - Truyen du lieu ETS2"));
        LOG_PRINTLN(F("    ETS2? / ETS2IP=<IP>    - WiFi REST client"));
        LOG_PRINTLN(F(" 3. Bieu cam Mat (RoboEyes):"));
        LOG_PRINTLN(F("    eye_happy, eye_sad, eye_anger, eye_blink, eye_center"));
        LOG_PRINTLN(F(" 4. Tu dong & He thong:"));
        LOG_PRINTLN(F("    AUTO=ON / AUTO=OFF     - Che do hanh dong tu do"));
        LOG_PRINTLN(F("    TIME=HH:MM:SS          - Dong bo gio"));
        LOG_PRINTLN(F("    reboot / restart       - Khoi dong lai ESP32"));
        LOG_PRINTLN(F("====================================================="));
    } else if (cmd == "AUTO=ON" || cmd == "AUTO=1" || cmd == "on") {
        SetAutoEnabled(true);
        enable_act = true;
        LOG_PRINTLN(F("[AUTO] Da bat che do tu hanh"));
    } else if (cmd == "AUTO=OFF" || cmd == "AUTO=0" || cmd == "off") {
        SetAutoEnabled(false);
        enable_act = false;
        LOG_PRINTLN(F("[AUTO] Da tat che do tu hanh"));
    } else if (cmd == "reboot" || cmd == "restart") {
        ESP.restart();
    } else {
        LOG_PRINTF("[CMD] Lenh khong hop le: '%s'. Go 'help' de xem danh sach lenh.\n", cmd.c_str());
    }
}

void sequent_act(String actions) {
    if (actions.isEmpty()) return;
    int startIndex = 0;
    while (startIndex < (int)actions.length()) {
        int commaIndex = actions.indexOf(',', startIndex);
        String cmd;
        if (commaIndex != -1) {
            cmd = actions.substring(startIndex, commaIndex);
            startIndex = commaIndex + 1;
        } else {
            cmd = actions.substring(startIndex);
            startIndex = actions.length();
        }
        cmd.trim();
        if (cmd.length() > 0) {
            emojiController.ExecuteCommand(cmd);
        }
    }
}

static volatile bool s_actTaskRunning = false;

void async_sequent_act(String actions) {
    if (actions.isEmpty()) return;
    if (s_actTaskRunning) {
        LOG_PRINTLN(F("[ACT] Robot dang thuc hien hanh dong!"));
        return;
    }
    xTaskCreate(
        [](void* parameter) {
            s_actTaskRunning = true;
            String* actStr = static_cast<String*>(parameter);
            sequent_act(*actStr);
            delete actStr;
            s_actTaskRunning = false;
            vTaskDelete(NULL);
        },
        "SeqActTask", 4096, new String(actions), 2, NULL
    );
}

void random_act() {
    emojiController.RandomAct();
}

void speech_act() {
    emojiController.RandomAct();
}
