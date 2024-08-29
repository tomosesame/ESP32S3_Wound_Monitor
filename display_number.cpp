// display_number.c

#include "display_number.h"

void display_number(lv_obj_t *parent, const char *number, const lv_font_t *font, lv_align_t align, int x_ofs, int y_ofs) {
    // 創建標籤並設置字型
    lv_obj_t *label = lv_label_create(parent);  // 在指定父對象上創建標籤
    lv_label_set_text(label, number);  // 設置標籤文本
    lv_obj_set_style_text_font(label, font, 0);  // 設置標籤字型
    lv_obj_align(label, align, x_ofs, y_ofs);  // 將標籤置於指定位置
}
