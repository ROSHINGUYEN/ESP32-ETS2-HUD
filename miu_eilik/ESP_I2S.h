/**
 * ESP_I2S Compatibility Header for Arduino-ESP32 (ESP-IDF v4.x / v5.x)
 * High-performance Audio DMA output cho ESP32-S3 (MAX98357A I2S DAC).
 *
 * Tương thích:
 *  - Arduino-ESP32 core 2.x (ESP-IDF v4.4)
 *  - Arduino-ESP32 core 3.x (ESP-IDF v5.1+)
 *
 * Cú pháp khởi tạo chuẩn theo ESP-IDF mới (ESP32-S3):
 *   ESP_I2S i2s;
 *   i2s.setPins(BCLK, LRC, DOUT);
 *   i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
 */
#ifndef _ESP_I2S_H_
#define _ESP_I2S_H_

#include <Arduino.h>
#include <driver/i2s.h>

#ifndef I2S_MODE_STD
#define I2S_MODE_STD                0
#endif

#ifndef I2S_DATA_BIT_WIDTH_16BIT
#define I2S_DATA_BIT_WIDTH_16BIT    16
#endif

#ifndef I2S_SLOT_MODE_STEREO
#define I2S_SLOT_MODE_STEREO        2
#endif

class ESP_I2S {
private:
    i2s_port_t _port;
    int _bclk, _ws, _dout;
    bool _is_installed;
    uint32_t _sample_rate;
    int _bit_width;
    int _slot_mode;

public:
    ESP_I2S(i2s_port_t port = I2S_NUM_0)
        : _port(port), _bclk(-1), _ws(-1), _dout(-1),
          _is_installed(false), _sample_rate(44100),
          _bit_width(I2S_DATA_BIT_WIDTH_16BIT), _slot_mode(I2S_SLOT_MODE_STEREO) {}

    /**
     * Cấu hình chân GPIO cho I2S. Phải gọi trước begin().
     */
    bool setPins(int bclk, int ws, int dout, int din = -1, int mclk = -1) {
        _bclk = bclk;
        _ws = ws;
        _dout = dout;
        (void)din; (void)mclk;
        return true;
    }

    /**
     * Khởi tạo driver I2S ở chế độ Master + TX cho ESP32-S3.
     *
     * Sử dụng cú pháp ESP-IDF mới:
     *  - i2s_config_t (struct designated initializer)
     *  - i2s_driver_install()
     *  - i2s_set_pin()
     *
     * KHÔNG dùng các hàm cũ đã bị deprecated:
     *  - i2s_driver_install() (chữ ký cũ vẫn còn nhưng config cũ đã lỗi)
     *  - setSampleRate() đã deprecated trên ESP32-S3
     */
    bool begin(int mode, uint32_t sample_rate, int bit_width = 16, int slot_mode = 2) {
        _sample_rate = sample_rate;
        _bit_width = bit_width;
        _slot_mode = slot_mode;

        if (_is_installed) {
            i2s_driver_uninstall(_port);
            _is_installed = false;
        }

        // Cấu trúc i2s_config_t theo ESP-IDF mới (v4.4+ / v5.x) tương thích ESP32-S3
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = sample_rate,
            .bits_per_sample = (i2s_bits_per_sample_t)bit_width,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 8,
            .dma_buf_len = 512,
            .use_apll = false,
            .tx_desc_auto_clear = true,
            .fixed_mclk = 0
        };

        esp_err_t err = i2s_driver_install(_port, &i2s_config, 0, NULL);
        if (err != ESP_OK) return false;

        i2s_pin_config_t pin_config = {
            .bck_io_num = _bclk,
            .ws_io_num = _ws,
            .data_out_num = _dout,
            .data_in_num = I2S_PIN_NO_CHANGE
        };

        err = i2s_set_pin(_port, &pin_config);
        if (err != ESP_OK) {
            i2s_driver_uninstall(_port);
            return false;
        }

        // Xóa DMA cũ để tránh click bụp khi khởi động
        i2s_zero_dma_buffer(_port);

        _is_installed = true;
        return true;
    }

    /**
     * Ghi dữ liệu PCM ra bus I2S. Trả về số byte đã ghi thực sự.
     */
    size_t write(const uint8_t *buffer, size_t size) {
        if (!_is_installed || !buffer || size == 0) return 0;
        size_t bytes_written = 0;
        // portMAX_DELAY để chờ DMA sẵn sàng, đảm bảo zero underrun
        esp_err_t err = i2s_write(_port, buffer, size, &bytes_written, portMAX_DELAY);
        if (err != ESP_OK) return 0;
        return bytes_written;
    }

    /**
     * Tắt driver I2S.
     */
    void end() {
        if (_is_installed) {
            i2s_driver_uninstall(_port);
            _is_installed = false;
        }
    }

    /**
     * Kiểm tra driver đã được cài đặt hay chưa.
     */
    bool isReady() const { return _is_installed; }

    /**
     * Lấy sample rate hiện tại.
     */
    uint32_t getSampleRate() const { return _sample_rate; }
};

#endif // _ESP_I2S_H_