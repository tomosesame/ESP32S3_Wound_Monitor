## ESP32-S3 Wound Monitor

### Author
陳芝任
呂欣擇

### Updates and Contributions

| Date       | Content                      | Owner     |
|------------|-------------------------------|------------|
| 2024-08-27 | Handover tasks from the senior        | 陳芝任、呂欣擇     |
| 2024-08-27 | Environment Setup and Demo Testing        | 陳芝任、呂欣擇     |
| 2024-08-28 | Bluetooth Setup              | 呂欣擇       |
| 2024-08-29 | ESP32 LCD Interface v1.0.1           | 陳芝任     |
| 2024-08-29 | Lab Meeting Preparation           | 呂欣擇     |


### Alerts
- Users must be aware of the datasheet given by Waveshare. Fundamental setups were introduced in the topic. Please check https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28 to learn more.
- Do not update any librarys, which may cause fatal errors.
- The Library should be the exact same to avoid unexpected errors.
- The tools area in Arduino IDE should be configured properly, if not, the monitor may not display.
- CH340 UART driver should be installed in advance, especially for MacOS users. Check https://www.wch-ic.com/products/CH340.html.