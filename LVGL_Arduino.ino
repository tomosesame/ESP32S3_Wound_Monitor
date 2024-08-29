#include <lvgl.h>
#include <TFT_eSPI.h>
#include "CST816S.h"
#include "fonts/futur_120.c"  // 包含自訂字體
#include "fonts/futur_30.c"   // 包含自訂字體
#include <BluetoothSerial.h>  // 包含藍牙庫

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2
#define BUTTON_PIN 15 // 定義按鈕引腳

static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT實例 */
CST816S touch(6, 7, 13, 5); // sda, scl, rst, irq

lv_obj_t *label;
lv_obj_t *info_label;  // 新增info_label來顯示數字含義
lv_obj_t *bluetooth_anim; // 藍牙動畫對象
int wound_severity = 0;
lv_color_t base_bg_color = lv_color_white(); // 初始背景色

BluetoothSerial SerialBT;  // Bluetooth Serial 對象

#if LV_USE_LOG != 0
/* Serial除錯 */
void my_print(const char * buf) {
    Serial.printf(buf);
    Serial.flush();
}
#endif

/* 顯示刷新 */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

void example_increase_lvgl_tick(void *arg) {
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    bool touched = touch.available();
    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touch.data.x;
        data->point.y = touch.data.y;
    }
}

/* 檢測按鈕按下狀態 */
void IRAM_ATTR button_isr_handler() {
    static unsigned long last_interrupt_time = 0;
    unsigned long interrupt_time = millis();

    if (interrupt_time - last_interrupt_time > 200) {
        // Debounce, 當前按鈕按下超過200ms
        lv_obj_set_hidden(bluetooth_anim, false);  // 顯示藍牙動畫
        SerialBT.begin("ESP32_BT");  // 啟動藍牙
        Serial.println("Bluetooth started, now detecting devices...");
        // 此處添加藍牙偵測代碼
    }
    last_interrupt_time = interrupt_time;
}

/* 呼吸燈效果的回調函數 */
void breathing_light_callback(lv_timer_t * timer) {
    // 定義亮度數列
    static const int brightness_values[] = {255, 253, 251, 249, 247, 249, 251, 253};
    static int index = 0;  // 當前索引

    // 獲取當前亮度值
    int brightness = brightness_values[index];

    // 根據基礎顏色調整亮度
    lv_color_t bgColor = lv_color_mix(base_bg_color, lv_color_black(), brightness); 
    lv_obj_set_style_bg_color(lv_scr_act(), bgColor, LV_PART_MAIN);  // 設置背景顏色

    // 更新索引以循環亮度數列
    index = (index + 1) % (sizeof(brightness_values) / sizeof(brightness_values[0]));
}

/* 更新傷口嚴重度的回調函數 */
void update_wound_severity(lv_timer_t * timer) {
    wound_severity = random(0, 100);  // 更新隨機數
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", wound_severity);
    lv_label_set_text(label, buf);

    // 根據 wound_severity 設置基礎背景顏色
    if (wound_severity <= 33) {
        base_bg_color = lv_color_hex(0x94BD4C);  // 基礎顏色：#94BD4C
        lv_label_set_text(info_label, "Safe");  // 設置對應的文字
    } else if (wound_severity <= 66) {
        base_bg_color = lv_color_hex(0xFDAD0D);  // 基礎顏色：#FDAD0D
        lv_label_set_text(info_label, "Warning");  // 設置對應的文字
    } else {
        base_bg_color = lv_color_hex(0xAA2422);  // 基礎顏色：#AA2422
        lv_label_set_text(info_label, "Dangerous");  // 設置對應的文字
    }
}

void setup() {
    Serial.begin(115200);

    lv_init();
#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
#endif

    tft.begin();
    tft.setRotation(0);
    touch.begin();

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "0");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_text_font(label, &futur_120, 0);  // 設定自訂字體

    // 創建info_label來顯示文字說明
    info_label = lv_label_create(lv_scr_act());
    lv_label_set_text(info_label, "Safe");  // 初始文字
    lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 30);  // 放置在數字下面
    lv_obj_set_style_text_font(info_label, &futur_30, 0);  // 使用futur_30字體

    // 創建藍牙動畫 (這裡可以替換成你需要的動畫)
    bluetooth_anim = lv_label_create(lv_scr_act());
    lv_label_set_text(bluetooth_anim, LV_SYMBOL_BLUETOOTH);
    lv_obj_align(bluetooth_anim, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_hidden(bluetooth_anim, true);  // 初始隱藏

    // 設置按鈕引腳
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(BUTTON_PIN, button_isr_handler, FALLING);

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };

    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);

    // 創建一個呼吸燈效果的計時器，每300ms (0.3秒) 更新一次背景
    lv_timer_create(breathing_light_callback, 100, NULL);

    // 創建一個傷口嚴重度更新計時器，每10000ms (10秒) 更新一次
    lv_timer_create(update_wound_severity, 10000, NULL);

    Serial.println("Setup done");
}

void loop() {
    lv_timer_handler();  // 處理LVGL的計時器和任務
    delay(5);  // 延遲以避免過度佔用CPU
}
