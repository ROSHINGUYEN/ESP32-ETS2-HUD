/**
 * @file emoji_controller.h
 * @brief 表情控制模块头文件 - Taibai Emoji Controller
 * 
 * Port trực tiếp từ repo: https://github.com/TranBinh2311/xiaozhi-deskemoji-taibai
 * File: main/boards/esp32-s3n16r8-emoji/emoji_controller.h
 */

#pragma once

#include "board_config.h"
#include "display_controller.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// 动画类型枚举 - Chuẩn Taibai gốc
enum class AnimationType {
    BLINK,        // 眨眼 (Chớp mắt)
    HAPPY,        // 开心 (Vui sướng)
    SAD,          // 悲伤 (Buồn bã)
    ANGER,        // 愤怒 (Tức giận)
    SURPRISE,     // 惊讶 (Ngạc nhiên)
    WAKEUP,       // 唤醒 (Thức giấc)
    SLEEP,        // 睡眠 (Ngủ say)
    LOOK_LEFT,    // 向左看 (Nhìn sang trái)
    LOOK_RIGHT,   // 向右看 (Nhìn sang phải)
    HEAD_NOD,     // 点头 (Gật đầu)
    HEAD_SHAKE,   // 摇头 (Lắc đầu)
    HEAD_ROLL,    // 头部转动 (Đảo đầu)
    CONFUSED,     // 困惑 (Bối rối)
    LAUGHING,     // 大笑 (Cười lớn)
    CRY,          // 哭泣 (Khóc)
    LOVING,       // 喜爱 (Yêu thích)
    THINKING,     // 思考 (Suy nghĩ)
    RANDOM        // 随机动画 (Ngẫu nhiên tự hành)
};

struct AnimationMessage {
    AnimationType type;
    int param;
};

class EmojiController {
public:
    EmojiController();
    ~EmojiController();

    void Initialize();
    void begin() { Initialize(); }

    void PlayAnimation(AnimationType type, int param = 0);
    void ExecuteCommand(String cmd);
    void AsyncExecute(String cmd);

    void Blink();
    void Happy();
    void Sad();
    void Anger();
    void Surprise();
    void Wakeup();
    void Sleep();
    void LookLeft();
    void LookRight();
    void HeadNod();
    void HeadShake();
    void HeadRoll();
    void Confused();
    void Laughing();
    void Loving();
    void Center();

    void RandomAct();
    void Update();

    bool IsAutoEnabled() const { return auto_enabled_; }
    void SetAutoEnabled(bool en) { auto_enabled_ = en; last_act_time_ = millis(); }
    uint32_t GetAutoInterval() const { return auto_interval_ms_; }
    void SetAutoInterval(uint32_t ms) { auto_interval_ms_ = ms; }
    bool IsBusy() const { return is_busy_; }
    bool isPlaying() const { return is_busy_; }

private:
    QueueHandle_t animation_queue_ = nullptr;
    TaskHandle_t animation_task_handle_ = nullptr;
    bool auto_enabled_ = true;
    uint32_t auto_interval_ms_ = 20000UL; // 20s quay 1 lần theo yêu cầu
    unsigned long last_act_time_ = 0;
    volatile bool is_busy_ = false;

    static void AnimationTask(void* pvParameters);
};

extern EmojiController emojiController;

// Globals tương thích với web_server và usb_protocol
extern bool enable_act;
extern uint32_t act_interval;
extern unsigned long last_act_time;

void async_sequent_act(String actions);
void sequent_act(String actions);
void random_act();
void speech_act();
