#pragma once
#include <lvgl.h>

// Compact 18x18 bell-off symbol. The one-pixel cutout around the slash
// separates it from the bell without extending beyond the icon's footprint.
inline void drawSkippedAlarmIcon(lv_draw_ctx_t *ctx, int x, int y,
                                 lv_color_t ink, lv_color_t background) {
  auto rect = [&](int left, int top, int width, int height, int radius) {
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = ink; d.radius = radius;
    lv_area_t area = {lv_coord_t(x+left), lv_coord_t(y+top),
                     lv_coord_t(x+left+width-1), lv_coord_t(y+top+height-1)};
    lv_draw_rect(ctx, &d, &area);
  };
  rect(8, 1, 2, 3, 1);
  rect(4, 3, 10, 11, 5);
  rect(4, 8, 10, 6, 0);
  rect(2, 12, 14, 2, 1);
  rect(7, 15, 4, 2, 2);
  const lv_point_t a = {lv_coord_t(x+2), lv_coord_t(y+16)};
  const lv_point_t b = {lv_coord_t(x+15), lv_coord_t(y+2)};
  lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
  line.color = background; line.width = 3;
  lv_draw_line(ctx, &line, &a, &b);
  line.color = ink; line.width = 1;
  lv_draw_line(ctx, &line, &a, &b);
}
