#include <lvgl.h>
#include <TFT_eSPI.h>
#include "CST816S.h"
#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"
#include "assets/img_bluetooth.c"  // 包含蓝牙图标图片数据
#include "fonts/futur_120.c"  // 自定义字体
#include "fonts/futur_30.c"   // 自定义字体

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2
#define BUTTON_PIN 15

static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT实例 */
CST816S touch(6, 7, 13, 5); // sda, scl, rst, irq

lv_obj_t *label;
lv_obj_t *info_label;
lv_obj_t *bluetooth_icon;  // 蓝牙图标对象
int wound_severity = 0;
lv_color_t base_bg_color = lv_color_white();

BLEServer* pServer = NULL;
BLEService* pService = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

volatile bool buttonPressed = false;

#if LV_USE_LOG != 0
/* Serial调试 */
void my_print(const char * buf) {
    Serial.printf(buf);
    Serial.flush();
}
#endif

/* 按钮中断处理 */
void IRAM_ATTR button_isr_handler() {
    buttonPressed = true;
}

/* 显示刷新 */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

/* LVGL时间处理 */
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

/* 呼吸灯效果的回调函数 */
void breathing_light_callback(lv_timer_t * timer) {
    static const int brightness_values[] = {255, 253, 251, 249, 247, 249, 251, 253};
    static int index = 0;  // 当前索引

    int brightness = brightness_values[index];

    lv_color_t bgColor = lv_color_mix(base_bg_color, lv_color_black(), brightness); 
    lv_obj_set_style_bg_color(lv_scr_act(), bgColor, LV_PART_MAIN);  // 设置背景颜色

    index = (index + 1) % (sizeof(brightness_values) / sizeof(brightness_values[0]));
}

/* 更新伤口严重度的回调函数 */
void update_wound_severity(lv_timer_t * timer) {
    wound_severity = random(0, 100);  // 更新随机数
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", wound_severity);
    lv_label_set_text(label, buf);

    // 根据 wound_severity 设置基础背景颜色
    if (wound_severity <= 33) {
        base_bg_color = lv_color_hex(0x94BD4C);  // 基础颜色：#94BD4C
        lv_label_set_text(info_label, "Safe");  // 设置对应的文字
    } else if (wound_severity <= 66) {
        base_bg_color = lv_color_hex(0xFDAD0D);  // 基础颜色：#FDAD0D
        lv_label_set_text(info_label, "Warning");  // 设置对应的文字
    } else {
        base_bg_color = lv_color_hex(0xAA2422);  // 基础颜色：#AA2422
        lv_label_set_text(info_label, "Dangerous");  // 设置对应的文字
    }
}

// 创建蓝牙图标
void create_bluetooth_icon(lv_obj_t *parent) {
    bluetooth_icon = lv_img_create(parent);
    lv_img_set_src(bluetooth_icon, &img_bluetooth);  // 使用 img_bluetooth 数据
    lv_obj_align(bluetooth_icon, LV_ALIGN_CENTER, 0, 0);  // 将图标居中
    lv_obj_add_flag(bluetooth_icon, LV_OBJ_FLAG_HIDDEN);  // 初始时隐藏
    lv_obj_move_foreground(bluetooth_icon);
}

// 蓝牙连接回调
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Device connected");
        lv_obj_add_flag(bluetooth_icon, LV_OBJ_FLAG_HIDDEN);  // 隐藏蓝牙图标
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected");
        lv_obj_clear_flag(bluetooth_icon, LV_OBJ_FLAG_HIDDEN);  // 显示蓝牙图标
        pServer->startAdvertising(); // 重新开始广播
    }
};

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

    // 创建标签和信息标签
    label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "0");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_text_font(label, &futur_120, 0);

    info_label = lv_label_create(lv_scr_act());
    lv_label_set_text(info_label, "Safe");
    lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_text_font(info_label, &futur_30, 0);

    // 创建蓝牙图标
    create_bluetooth_icon(lv_scr_act());

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_isr_handler, FALLING);

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };

    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);

    // 创建一个呼吸灯效果的计时器，每100ms (0.1秒) 更新一次背景
    lv_timer_create(breathing_light_callback, 100, NULL);

    // 创建一个伤口严重度更新计时器，每10000ms (10秒) 更新一次
    lv_timer_create(update_wound_severity, 10000, NULL);

    // 初始化 BLE
    BLEDevice::init("ESP32_BLE");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());  // 设置回调
    pService = pServer->createService(BLEUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b"));
    pCharacteristic = pService->createCharacteristic(
        BLEUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8"),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    pCharacteristic->setValue("Hello World");
    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    pAdvertising->start();

    Serial.println("Setup done");
}

void loop() {
    lv_timer_handler();  // 处理 LVGL 的计时器和任务
    delay(5);  // 延迟以避免过度占用 CPU

    if (buttonPressed) {
        buttonPressed = false;
        Serial.println("Button pressed, restarting BLE advertising...");
        BLEDevice::getAdvertising()->start();  // 重新开始广播
        lv_obj_clear_flag(bluetooth_icon, LV_OBJ_FLAG_HIDDEN);  // 显示蓝牙图标
    }
}
