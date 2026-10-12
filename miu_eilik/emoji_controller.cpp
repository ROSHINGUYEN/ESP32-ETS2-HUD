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
                    vTaskDelay(pdMS_TO_TICKS(600));
                    break;

                case AnimationType::HAPPY:
                    // Mắt cười híp cong lưỡi liềm + rung rinh cười vui
                    roboEyes.setMood(MOOD_HAPPY);
                    roboEyes.anim_laugh();
                    vTaskDelay(pdMS_TO_TICKS(1800));
                    roboEyes.blink();
                    vTaskDelay(pdMS_TO_TICKS(800));
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::WINK:
                    // Nháy mắt phải (mắt trái mở cười, mắt phải nháy một bên tinh nghịch)
                    roboEyes.setMood(MOOD_HAPPY);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    roboEyes.blink(false, true); // CHỈ CHỚP MẮT PHẢI
                    vTaskDelay(pdMS_TO_TICKS(1600));
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::SURPRISE:
                    // Mắt tròn xoe to tướng ngơ ngác
                    roboEyes.setWidth(42, 42);
                    roboEyes.setHeight(42, 42);
                    roboEyes.setBorderradius(18, 18);
                    vTaskDelay(pdMS_TO_TICKS(2200));
                    roboEyes.blink();
                    roboEyes.setWidth(36, 36);
                    roboEyes.setHeight(36, 36);
                    roboEyes.setBorderradius(8, 8);
                    vTaskDelay(pdMS_TO_TICKS(400));
                    break;

                case AnimationType::CURIOUS:
                    // Tò mò: mắt to mắt nhỏ liếc trái rồi liếc phải
                    roboEyes.setCuriosity(true);
                    roboEyes.setPosition(7); // Trái (West) -> mắt trái to hơn hẳn
                    vTaskDelay(pdMS_TO_TICKS(1300));
                    roboEyes.setPosition(3); // Phải (East) -> mắt phải to hơn hẳn
                    vTaskDelay(pdMS_TO_TICKS(1300));
                    roboEyes.setPosition(0);
                    roboEyes.setCuriosity(false);
                    vTaskDelay(pdMS_TO_TICKS(400));
                    break;

                case AnimationType::CONFUSED:
                    // Toát mồ hôi rơi trên trán + rung bối rối nhìn xuống
                    roboEyes.setSweat(true);
                    roboEyes.anim_confused();
                    roboEyes.setPosition(6); // SW
                    vTaskDelay(pdMS_TO_TICKS(2500));
                    roboEyes.setSweat(false);
                    roboEyes.setPosition(0);
                    vTaskDelay(pdMS_TO_TICKS(400));
                    break;

                case AnimationType::ANGER:
                    // Giận dỗi đáng yêu: mí mắt quắc chéo liếc ngang
                    roboEyes.setMood(MOOD_ANGRY);
                    roboEyes.setPosition(3); // Liếc xéo phải
                    vTaskDelay(pdMS_TO_TICKS(1800));
                    roboEyes.setPosition(0);
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(400));
                    break;

                case AnimationType::SAD:
                    // Buồn bã: mí mắt cụp mệt mỏi nhìn xuống
                    roboEyes.setMood(MOOD_TIRED);
                    roboEyes.setPosition(5); // South
                    vTaskDelay(pdMS_TO_TICKS(1800));
                    roboEyes.setPosition(0);
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(400));
                    break;

                case AnimationType::THINKING:
                    // Suy nghĩ: nheo mắt nhìn lên góc trên trái
                    roboEyes.setPosition(8); // NW
                    roboEyes.setHeight(24, 24);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    roboEyes.setHeight(36, 36);
                    roboEyes.blink();
                    roboEyes.setPosition(0);
                    vTaskDelay(pdMS_TO_TICKS(400));
                    break;

                case AnimationType::EXCITED:
                    // Hứng khởi: mắt cười mở to nhìn lên cười rạng rỡ
                    roboEyes.open();
                    roboEyes.setMood(MOOD_HAPPY);
                    roboEyes.anim_laugh();
                    roboEyes.setPosition(1); // North
                    vTaskDelay(pdMS_TO_TICKS(1200));
                    roboEyes.anim_laugh();
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    roboEyes.setPosition(0);
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(400));
                    break;

                case AnimationType::LOVING:
                    // Yêu thích: mắt cười chớp kép âu yếm
                    roboEyes.setMood(MOOD_HAPPY);
                    roboEyes.blink(true, true);
                    vTaskDelay(pdMS_TO_TICKS(350));
                    roboEyes.blink(true, true);
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(400));
                    break;

                case AnimationType::SHY:
                    // Ngại ngùng: nhìn xuống trái, chớp mắt nhẹ
                    roboEyes.setMood(MOOD_TIRED);
                    roboEyes.setPosition(6); // SW
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    roboEyes.blink();
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    roboEyes.setPosition(0);
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(400));
                    break;

                case AnimationType::STRETCH:
                    // Vươn vai: nhìn lên, kéo dài mắt rồi quét ngang
                    roboEyes.setPosition(1); // N
                    roboEyes.setHeight(40, 40);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    roboEyes.setPosition(7); // W
                    vTaskDelay(pdMS_TO_TICKS(500));
                    roboEyes.setPosition(3); // E
                    vTaskDelay(pdMS_TO_TICKS(500));
                    roboEyes.setHeight(36, 36);
                    roboEyes.setPosition(0);
                    roboEyes.blink();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;

                case AnimationType::WAKEUP:
                    roboEyes.open();
                    roboEyes.setWidth(36, 36);
                    roboEyes.setHeight(36, 36);
                    roboEyes.setBorderradius(8, 8);
                    roboEyes.setMood(MOOD_DEFAULT);
                    roboEyes.blink();
                    vTaskDelay(pdMS_TO_TICKS(600));
                    break;

                case AnimationType::SLEEP:
                    roboEyes.setMood(MOOD_TIRED);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    roboEyes.close();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    break;

                case AnimationType::LOOK_LEFT:
                    roboEyes.setPosition(7); // West
                    vTaskDelay(pdMS_TO_TICKS(1200));
                    roboEyes.setPosition(0);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::LOOK_RIGHT:
                    roboEyes.setPosition(3); // East
                    vTaskDelay(pdMS_TO_TICKS(1200));
                    roboEyes.setPosition(0);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::HEAD_NOD:
                    roboEyes.setPosition(5); // South
                    vTaskDelay(pdMS_TO_TICKS(300));
                    roboEyes.setPosition(1); // North
                    vTaskDelay(pdMS_TO_TICKS(300));
                    roboEyes.setPosition(0);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::HEAD_SHAKE:
                    roboEyes.setPosition(7); // West
                    vTaskDelay(pdMS_TO_TICKS(250));
                    roboEyes.setPosition(3); // East
                    vTaskDelay(pdMS_TO_TICKS(250));
                    roboEyes.setPosition(0);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::HEAD_ROLL:
                    {
                        const byte dirs[] = {1, 2, 3, 4, 5, 6, 7, 8, 0};
                        for (byte d : dirs) {
                            roboEyes.setPosition(d);
                            vTaskDelay(pdMS_TO_TICKS(180));
                        }
                    }
                    roboEyes.blink();
                    vTaskDelay(pdMS_TO_TICKS(400));
                    break;

                case AnimationType::LAUGHING:
                    roboEyes.setMood(MOOD_HAPPY);
                    roboEyes.anim_laugh();
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::CRY:
                    roboEyes.setMood(MOOD_TIRED);
                    roboEyes.setPosition(5);
                    roboEyes.setSweat(true);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    roboEyes.setSweat(false);
                    roboEyes.setPosition(0);
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::WINK_LEFT:
                    // Nháy mắt trái (mắt phải mở cười, mắt trái nháy một bên tinh nghịch)
                    roboEyes.setMood(MOOD_HAPPY);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    roboEyes.blink(true, false); // CHỈ CHỚP MẮT TRÁI
                    vTaskDelay(pdMS_TO_TICKS(1600));
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::DIZZY:
                    // Chóng mặt: rung ngang ngửa, đảo vòng rồi toét mắt bối rối
                    roboEyes.setHFlicker(true, 4);
                    roboEyes.anim_confused();
                    {
                        const byte dirs[] = {1, 3, 5, 7, 1, 3, 5, 7};
                        for (byte d : dirs) {
                            roboEyes.setPosition(d);
                            vTaskDelay(pdMS_TO_TICKS(140));
                        }
                    }
                    roboEyes.setHFlicker(false);
                    roboEyes.setPosition(0);
                    roboEyes.setMood(MOOD_TIRED);
                    vTaskDelay(pdMS_TO_TICKS(1200));
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::HEART_PULSE:
                    // Trái tim đập thình thịch / Yêu say đắm: mắt co giãn phập phồng
                    roboEyes.setMood(MOOD_HAPPY);
                    for (int i = 0; i < 3; i++) {
                        roboEyes.setWidth(42, 42);
                        roboEyes.setHeight(42, 42);
                        roboEyes.setBorderradius(18, 18);
                        vTaskDelay(pdMS_TO_TICKS(280));
                        roboEyes.setWidth(32, 32);
                        roboEyes.setHeight(32, 32);
                        roboEyes.setBorderradius(8, 8);
                        vTaskDelay(pdMS_TO_TICKS(220));
                    }
                    roboEyes.setWidth(36, 36);
                    roboEyes.setHeight(36, 36);
                    roboEyes.setBorderradius(8, 8);
                    roboEyes.blink(true, true);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::SKEPTICAL:
                    // Nghi ngờ: mắt trái nheo nhỏ, mắt phải mở to nhìn liếc xéo
                    roboEyes.setWidth(24, 38);
                    roboEyes.setHeight(16, 38);
                    roboEyes.setPosition(3); // Liếc sang phải
                    vTaskDelay(pdMS_TO_TICKS(1600));
                    roboEyes.setPosition(7); // Liếc sang trái
                    vTaskDelay(pdMS_TO_TICKS(800));
                    roboEyes.setWidth(36, 36);
                    roboEyes.setHeight(36, 36);
                    roboEyes.setPosition(0);
                    roboEyes.blink();
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::SNEAKY:
                    // Lén lút / Rình rập: nheo mắt lủi xuống đáy, liếc qua lại rồi nhô lên nhìn trộm
                    roboEyes.setHeight(18, 18);
                    roboEyes.setPosition(5); // South (đáy màn hình)
                    vTaskDelay(pdMS_TO_TICKS(800));
                    roboEyes.setPosition(6); // SW
                    vTaskDelay(pdMS_TO_TICKS(600));
                    roboEyes.setPosition(4); // SE
                    vTaskDelay(pdMS_TO_TICKS(600));
                    roboEyes.setPosition(1); // Thò lên North nhìn trộm
                    vTaskDelay(pdMS_TO_TICKS(700));
                    roboEyes.setHeight(36, 36);
                    roboEyes.setPosition(0);
                    roboEyes.blink();
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::BORED:
                    // Chán nản ngao ngán: đảo mắt lên trời thở dài rồi cụp xuống
                    roboEyes.setMood(MOOD_TIRED);
                    roboEyes.setPosition(1); // Đảo mắt lên trần nhà
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    roboEyes.setPosition(5); // Cụp xuống thở dài
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    roboEyes.setPosition(0);
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::SMUG:
                    // Tự đắc: nhìn xếch lên đắc thắng, cười khẩy rồi nháy mắt tự mãn
                    roboEyes.setMood(MOOD_HAPPY);
                    roboEyes.setPosition(2); // NE (nhìn xếch lên)
                    roboEyes.anim_laugh();
                    vTaskDelay(pdMS_TO_TICKS(1400));
                    roboEyes.blink(false, true); // Nháy mắt phải tự mãn
                    vTaskDelay(pdMS_TO_TICKS(1200));
                    roboEyes.setPosition(0);
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::PANIC:
                    // Hoảng loạn cực độ: mắt to tròn 42x42 + rung giật cả 2 chiều + toát mồ hôi
                    roboEyes.setWidth(42, 42);
                    roboEyes.setHeight(42, 42);
                    roboEyes.setBorderradius(18, 18);
                    roboEyes.setSweat(true);
                    roboEyes.setHFlicker(true, 3);
                    roboEyes.setVFlicker(true, 3);
                    roboEyes.anim_confused();
                    vTaskDelay(pdMS_TO_TICKS(2200));
                    roboEyes.setHFlicker(false);
                    roboEyes.setVFlicker(false);
                    roboEyes.setSweat(false);
                    roboEyes.setWidth(36, 36);
                    roboEyes.setHeight(36, 36);
                    roboEyes.setBorderradius(8, 8);
                    roboEyes.blink();
                    vTaskDelay(pdMS_TO_TICKS(400));
                    break;

                case AnimationType::SLEEPY_NOD:
                    // Gật gù ngủ gật: cụp mắt gục đầu, nhắm tịt rồi giật mình mở to bừng tỉnh!
                    roboEyes.setMood(MOOD_TIRED);
                    roboEyes.setPosition(5); // Gục đầu xuống
                    vTaskDelay(pdMS_TO_TICKS(800));
                    roboEyes.close();        // Mắt nhắm tịt ngủ quên
                    vTaskDelay(pdMS_TO_TICKS(1400));
                    // Bừng tỉnh giật mình!
                    roboEyes.open();
                    roboEyes.setWidth(40, 40);
                    roboEyes.setHeight(40, 40);
                    roboEyes.setPosition(0);
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    roboEyes.setWidth(36, 36);
                    roboEyes.setHeight(36, 36);
                    roboEyes.blink();
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::DANCE:
                    // Nhảy múa lắc lư: nhún nhảy 4 hướng nhịp nhàng + cười vui
                    roboEyes.setMood(MOOD_HAPPY);
                    for (int i = 0; i < 2; i++) {
                        roboEyes.setPosition(7); // Trái
                        vTaskDelay(pdMS_TO_TICKS(220));
                        roboEyes.setPosition(1); // Lên
                        vTaskDelay(pdMS_TO_TICKS(220));
                        roboEyes.setPosition(3); // Phải
                        vTaskDelay(pdMS_TO_TICKS(220));
                        roboEyes.setPosition(5); // Xuống
                        vTaskDelay(pdMS_TO_TICKS(220));
                    }
                    roboEyes.anim_laugh();
                    roboEyes.setPosition(0);
                    vTaskDelay(pdMS_TO_TICKS(1200));
                    roboEyes.setMood(MOOD_DEFAULT);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;

                case AnimationType::RADAR:
                    // Quét radar sci-fi: mắt phẳng dẹt quét từ trái sang phải như màn quét
                    roboEyes.setHeight(16, 16);
                    roboEyes.setWidth(40, 40);
                    roboEyes.setBorderradius(4, 4);
                    roboEyes.setPosition(7); // Quét từ cực Tây (Trái)
                    vTaskDelay(pdMS_TO_TICKS(700));
                    roboEyes.setPosition(0); // Quét qua giữa
                    vTaskDelay(pdMS_TO_TICKS(500));
                    roboEyes.setPosition(3); // Quét tới cực Đông (Phải)
                    vTaskDelay(pdMS_TO_TICKS(700));
                    roboEyes.setPosition(0);
                    roboEyes.setHeight(36, 36);
                    roboEyes.setWidth(36, 36);
                    roboEyes.setBorderradius(8, 8);
                    roboEyes.blink();
                    vTaskDelay(pdMS_TO_TICKS(400));
                    break;

                case AnimationType::YAWN:
                    // Ngáp dài buồn ngủ: mắt trợn to ngước nhìn lên cao, rung rinh rồi từ từ nhắm
                    roboEyes.setPosition(1); // Ngước nhìn lên
                    roboEyes.setWidth(38, 38);
                    roboEyes.setHeight(42, 42);
                    vTaskDelay(pdMS_TO_TICKS(1200));
                    roboEyes.setVFlicker(true, 2);
                    vTaskDelay(pdMS_TO_TICKS(600));
                    roboEyes.setVFlicker(false);
                    roboEyes.close();
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    roboEyes.open();
                    roboEyes.setWidth(36, 36);
                    roboEyes.setHeight(24, 24);
                    roboEyes.setMood(MOOD_TIRED);
                    roboEyes.setPosition(0);
                    vTaskDelay(pdMS_TO_TICKS(1200));
                    roboEyes.setHeight(36, 36);
                    roboEyes.setMood(MOOD_DEFAULT);
                    roboEyes.blink();
                    vTaskDelay(pdMS_TO_TICKS(300));
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

void EmojiController::WinkLeft()   { PlayAnimation(AnimationType::WINK_LEFT); }
void EmojiController::Dizzy()      { PlayAnimation(AnimationType::DIZZY); }
void EmojiController::HeartPulse() { PlayAnimation(AnimationType::HEART_PULSE); }
void EmojiController::Skeptical()  { PlayAnimation(AnimationType::SKEPTICAL); }
void EmojiController::Sneaky()     { PlayAnimation(AnimationType::SNEAKY); }
void EmojiController::Bored()      { PlayAnimation(AnimationType::BORED); }
void EmojiController::Smug()       { PlayAnimation(AnimationType::SMUG); }
void EmojiController::Panic()      { PlayAnimation(AnimationType::PANIC); }
void EmojiController::SleepyNod()  { PlayAnimation(AnimationType::SLEEPY_NOD); }
void EmojiController::Dance()      { PlayAnimation(AnimationType::DANCE); }
void EmojiController::Radar()      { PlayAnimation(AnimationType::RADAR); }
void EmojiController::Yawn()       { PlayAnimation(AnimationType::YAWN); }

void EmojiController::RandomAct() {
    // 19 slot hành động desktop pet với xác suất phân bổ hợp lý
    int choice = random(19);

    if (choice < 3) {
        // --- Blink tự nhiên (xác suất ~15%) ---
        roboEyes.blink();

    } else if (choice < 5) {
        // --- Nhìn trái ---
        roboEyes.setPosition(7);
        vTaskDelay(pdMS_TO_TICKS(700));
        roboEyes.setPosition(0);

    } else if (choice < 7) {
        // --- Nhìn phải ---
        roboEyes.setPosition(3);
        vTaskDelay(pdMS_TO_TICKS(700));
        roboEyes.setPosition(0);

    } else if (choice == 7) {
        // --- Nhìn lên ---
        roboEyes.setPosition(1);
        vTaskDelay(pdMS_TO_TICKS(600));
        roboEyes.setPosition(0);

    } else if (choice == 8) {
        // --- Nhìn xuống (ngượng/tư lự) ---
        roboEyes.setMood(MOOD_TIRED);
        roboEyes.setPosition(5);
        vTaskDelay(pdMS_TO_TICKS(700));
        roboEyes.setPosition(0);
        roboEyes.setMood(MOOD_DEFAULT);

    } else if (choice == 9) {
        // --- Gật đầu ---
        roboEyes.setPosition(5);
        vTaskDelay(pdMS_TO_TICKS(250));
        roboEyes.setPosition(1);
        vTaskDelay(pdMS_TO_TICKS(250));
        roboEyes.setPosition(0);

    } else if (choice == 10) {
        // --- Lắc đầu ---
        roboEyes.setPosition(7);
        vTaskDelay(pdMS_TO_TICKS(200));
        roboEyes.setPosition(3);
        vTaskDelay(pdMS_TO_TICKS(200));
        roboEyes.setPosition(0);

    } else if (choice == 11) {
        // --- Bối rối ---
        roboEyes.anim_confused();
        vTaskDelay(pdMS_TO_TICKS(350));
        roboEyes.setPosition(0);

    } else if (choice == 12) {
        // --- Suy nghĩ: nhìn lên trái rồi blink ---
        roboEyes.setPosition(8); // NW
        vTaskDelay(pdMS_TO_TICKS(500));
        roboEyes.blink();
        vTaskDelay(pdMS_TO_TICKS(200));
        roboEyes.setPosition(0);

    } else if (choice == 13) {
        // --- Cười sảng khoái ---
        roboEyes.setMood(MOOD_HAPPY);
        roboEyes.anim_laugh();
        vTaskDelay(pdMS_TO_TICKS(300));
        roboEyes.setMood(MOOD_DEFAULT);

    } else if (choice == 14) {
        // --- WINK: mood vui + nhìn nghiêng + blink ---
        roboEyes.setMood(MOOD_HAPPY);
        roboEyes.setPosition(3);
        vTaskDelay(pdMS_TO_TICKS(100));
        roboEyes.blink();
        vTaskDelay(pdMS_TO_TICKS(150));
        roboEyes.setPosition(0);
        roboEyes.setMood(MOOD_DEFAULT);

    } else if (choice == 15) {
        // --- EXCITED: mở to + cười to ---
        roboEyes.open();
        roboEyes.setMood(MOOD_HAPPY);
        vTaskDelay(pdMS_TO_TICKS(200));
        roboEyes.anim_laugh();
        vTaskDelay(pdMS_TO_TICKS(400));
        roboEyes.setMood(MOOD_DEFAULT);

    } else if (choice == 16) {
        // --- CURIOUS: quét mắt NW -> NE -> center ---
        roboEyes.setPosition(8); // NW
        vTaskDelay(pdMS_TO_TICKS(400));
        roboEyes.setPosition(2); // NE
        vTaskDelay(pdMS_TO_TICKS(300));
        roboEyes.setPosition(0);

    } else if (choice == 17) {
        // --- SHY: nhìn xuống trái + blink nhẹ ---
        roboEyes.setMood(MOOD_TIRED);
        roboEyes.setPosition(6); // SW
        vTaskDelay(pdMS_TO_TICKS(300));
        roboEyes.blink();
        vTaskDelay(pdMS_TO_TICKS(400));
        roboEyes.setPosition(0);
        roboEyes.setMood(MOOD_DEFAULT);

    } else {
        // --- STRETCH: vươn lên -> quét trái/phải -> center + blink ---
        roboEyes.setPosition(1); // N
        vTaskDelay(pdMS_TO_TICKS(300));
        roboEyes.setPosition(7); // W
        vTaskDelay(pdMS_TO_TICKS(200));
        roboEyes.setPosition(3); // E
        vTaskDelay(pdMS_TO_TICKS(200));
        roboEyes.setPosition(0);
        roboEyes.blink();
    }
}

void EmojiController::Update() {
    if (!auto_enabled_ || is_busy_) return;
    if (display.mode == DISP_GAME) return;

    if (millis() - last_act_time_ >= auto_interval_ms_) {
        last_act_time_ = millis();
        // Dung queue de dam bao thread-safe voi AnimationTask (tranh race condition tren I2C)
        PlayAnimation(AnimationType::RANDOM);
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
    } else if (cmd == "wink" || cmd == "eye_wink") {
        PlayAnimation(AnimationType::WINK);
    } else if (cmd == "excited" || cmd == "eye_excited") {
        PlayAnimation(AnimationType::EXCITED);
    } else if (cmd == "curious" || cmd == "eye_curious") {
        PlayAnimation(AnimationType::CURIOUS);
    } else if (cmd == "shy" || cmd == "eye_shy") {
        PlayAnimation(AnimationType::SHY);
    } else if (cmd == "stretch" || cmd == "eye_stretch") {
        PlayAnimation(AnimationType::STRETCH);
    } else if (cmd == "wink_left" || cmd == "eye_wink_left") {
        WinkLeft();
    } else if (cmd == "dizzy" || cmd == "eye_dizzy") {
        Dizzy();
    } else if (cmd == "heart" || cmd == "heart_pulse" || cmd == "love_pulse") {
        HeartPulse();
    } else if (cmd == "skeptical" || cmd == "doubt" || cmd == "eye_skeptical") {
        Skeptical();
    } else if (cmd == "sneaky" || cmd == "peek" || cmd == "eye_sneaky") {
        Sneaky();
    } else if (cmd == "bored" || cmd == "sigh" || cmd == "eye_bored") {
        Bored();
    } else if (cmd == "smug" || cmd == "proud" || cmd == "eye_smug") {
        Smug();
    } else if (cmd == "panic" || cmd == "freak" || cmd == "eye_panic") {
        Panic();
    } else if (cmd == "sleepy_nod" || cmd == "nod_sleep") {
        SleepyNod();
    } else if (cmd == "dance" || cmd == "party" || cmd == "eye_dance") {
        Dance();
    } else if (cmd == "radar" || cmd == "scan" || cmd == "eye_radar") {
        Radar();
    } else if (cmd == "yawn" || cmd == "eye_yawn") {
        Yawn();
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
        LOG_PRINTLN(F(" 3. Bieu cam Mat (24 bieu cam RoboEyes):"));
        LOG_PRINTLN(F("    happy, wink, surprise, curious, confused, excited, thinking, anger, loving, roll, stretch, shy"));
        LOG_PRINTLN(F("    wink_left, dizzy, heart, skeptical, sneaky, bored, smug, panic, sleepy_nod, dance, radar, yawn"));
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
