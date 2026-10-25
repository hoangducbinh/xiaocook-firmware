#include "application.h"
#include "assets/lang_config.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "system_reset.h"
#include "wifi_board.h"

#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if defined(LCD_TYPE_ILI9341_SERIAL)
#include "esp_lcd_ili9341.h"
#endif

#if defined(LCD_TYPE_GC9A01_SERIAL)
#include "esp_lcd_gc9a01.h"
static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0},
    {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0},
    {0xb1, (uint8_t[]){0x80}, 1, 0},
    {0xb2, (uint8_t[]){0x27}, 1, 0},
    {0xb3, (uint8_t[]){0x13}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0},
    {0xb7, (uint8_t[]){0x05}, 1, 0},
    {0xac, (uint8_t[]){0xc8}, 1, 0},
    {0xab, (uint8_t[]){0x0f}, 1, 0},
    {0x3a, (uint8_t[]){0x05}, 1, 0},
    {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x08}, 1, 0},
    {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xea, (uint8_t[]){0x02}, 1, 0},
    {0xe8, (uint8_t[]){0x2A}, 1, 0},
    {0xe9, (uint8_t[]){0x47}, 1, 0},
    {0xe7, (uint8_t[]){0x5f}, 1, 0},
    {0xc6, (uint8_t[]){0x21}, 1, 0},
    {0xc7, (uint8_t[]){0x15}, 1, 0},
    {0xf0,
    (uint8_t[]){0x1D, 0x38, 0x09, 0x4D, 0x92, 0x2F, 0x35, 0x52, 0x1E, 0x0C,
                0x04, 0x12, 0x14, 0x1f},
    14, 0},
    {0xf1,
    (uint8_t[]){0x16, 0x40, 0x1C, 0x54, 0xA9, 0x2D, 0x2E, 0x56, 0x10, 0x0D,
                0x0C, 0x1A, 0x14, 0x1E},
    14, 0},
    {0xf4, (uint8_t[]){0x00, 0x00, 0xFF}, 3, 0},
    {0xba, (uint8_t[]){0xFF, 0xFF}, 2, 0},
};
#endif
 
#define TAG "XiaoCookBoard"

class XiaoCookBoard : public WifiBoard {
private:
 
    Button boot_button_;
    LcdDisplay* display_;
    int unmuted_volume_ = 50;
#if defined(CONFIG_LCD_ST7789_240X240) || defined(CONFIG_LCD_ST7789_240X240_7PIN)
    esp_lcd_panel_handle_t recovery_panel_ = nullptr;
    TaskHandle_t lcd_recovery_task_ = nullptr;

    esp_err_t RestoreLcdState() {
        // The first parameter transaction drains queued SPI/DMA transfers.
        // LVGL must not enqueue another flush while panel commands are replayed.
        esp_err_t err = esp_lcd_panel_init(recovery_panel_);
        if (err == ESP_OK) {
            err = esp_lcd_panel_invert_color(recovery_panel_, DISPLAY_INVERT_COLOR);
        }
        if (err == ESP_OK) {
            err = esp_lcd_panel_swap_xy(recovery_panel_, DISPLAY_SWAP_XY);
        }
        if (err == ESP_OK) {
            err = esp_lcd_panel_mirror(recovery_panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        }
        if (err == ESP_OK) {
            err = esp_lcd_panel_disp_on_off(recovery_panel_, true);
        }
        return err;
    }

    void InitializeLcdRecovery() {
        // The board is a process-lifetime singleton. This task shares its lifetime.
        BaseType_t created = xTaskCreate(
            [](void* context) {
                auto* self = static_cast<XiaoCookBoard*>(context);
                for (;;) {
                    bool requested =
                        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(DISPLAY_RECOVERY_INTERVAL_MS)) != 0;
                    if (!lvgl_port_lock(100)) {
                        continue;  // Skip a busy display; never stall the application loop.
                    }
                    esp_err_t err = self->RestoreLcdState();
                    if (err == ESP_OK) {
                        lv_obj_invalidate(lv_screen_active());
                    }
                    lvgl_port_unlock();
                    if (err != ESP_OK) {
                        ESP_LOGW(TAG, "LCD state replay failed: %s; will retry",
                                 esp_err_to_name(err));
                    } else if (requested) {
                        ESP_LOGI(TAG, "LCD state replay sent; no hardware readback");
                    }
                }
            },
            "lcd_recovery", 4096, this, 1, &lcd_recovery_task_);
        if (created != pdPASS) {
            lcd_recovery_task_ = nullptr;
            ESP_LOGW(TAG, "LCD automatic recovery task could not be created");
        } else {
            ESP_LOGI(TAG, "LCD state replay every %d ms; hold BOOT for immediate replay",
                     DISPLAY_RECOVERY_INTERVAL_MS);
        }
    }
#endif

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
#if defined(LCD_TYPE_ILI9341_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_GC9A01_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds,
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
        };        
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif

        ESP_LOGI(TAG, "LCD %dx%d: SPI mode %d, 40 MHz, offset %d,%d", DISPLAY_WIDTH, DISPLAY_HEIGHT,
                 DISPLAY_SPI_MODE, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y);
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
#if defined(CONFIG_LCD_ST7789_240X240) || defined(CONFIG_LCD_ST7789_240X240_7PIN)
        // Match the reset and initialization settling time verified in screen-test.
        vTaskDelay(pdMS_TO_TICKS(150));
#endif
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
#if defined(CONFIG_LCD_ST7789_240X240) || defined(CONFIG_LCD_ST7789_240X240_7PIN)
        vTaskDelay(pdMS_TO_TICKS(150));
#endif
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
#ifdef  LCD_TYPE_GC9A01_SERIAL
        panel_config.vendor_config = &gc9107_vendor_config;
#endif
        // The robot-eye GIFs have a black canvas; match both selectable themes.
        auto& themes = LvglThemeManager::GetInstance();
        for (const char* name : {"light", "dark"}) {
            if (auto* theme = themes.GetTheme(name)) {
                theme->set_background_color(lv_color_hex(0x000000));
                theme->set_chat_background_color(lv_color_hex(0x000000));
                theme->set_text_color(lv_color_hex(0xFFFFFF));
            }
        }
#if defined(CONFIG_LCD_ST7789_240X240) || defined(CONFIG_LCD_ST7789_240X240_7PIN)
        recovery_panel_ = panel;
#endif
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
#if defined(CONFIG_LCD_ST7789_240X240) || defined(CONFIG_LCD_ST7789_240X240_7PIN)
        boot_button_.OnLongPress([this]() {
            if (lcd_recovery_task_ != nullptr) {
                xTaskNotifyGive(lcd_recovery_task_);
            }
        });
#endif
        boot_button_.OnDoubleClick([this]() {
            Application::GetInstance().Schedule([this]() {
                auto* codec = GetAudioCodec();
                if (codec->output_volume() > 0) {
                    unmuted_volume_ = codec->output_volume();
                    codec->SetOutputVolume(0);
                    display_->ShowNotification(Lang::Strings::MUTED);
                } else {
                    codec->SetOutputVolume(unmuted_volume_);
                    display_->ShowNotification(Lang::Strings::VOLUME +
                                               std::to_string(unmuted_volume_));
                }
            });
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeTools() {
        static LampController lamp(LAMP_GPIO);
    }

public:
    XiaoCookBoard() :
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializeSpi();
        InitializeLcdDisplay();
#if defined(CONFIG_LCD_ST7789_240X240) || defined(CONFIG_LCD_ST7789_240X240_7PIN)
        InitializeLcdRecovery();
#endif
        InitializeButtons();
        InitializeTools();
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
        
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
};

DECLARE_BOARD(XiaoCookBoard);
