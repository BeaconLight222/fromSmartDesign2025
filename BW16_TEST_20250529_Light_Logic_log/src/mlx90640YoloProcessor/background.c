#include "background.h"
//#include "model_constants.h"
#include "macros.h"
#include <math.h>
#include <string.h>

int background_init(int16_t *input, int16_t *background) {

  for (int i = 0; i < 32 * 24; i++) {
    background[i] = input[i];
  }

  return 0;
}

void background_create_bbox_mask(int16_t *input,
                                 const struct Detection *detections,
                                 int detections_len, float margin, float thresh,
                                 uint8_t *mask) {
  (void)input;

  int i, x, y;
  const int x_max = 32;
  const int y_max = 24;

  // fill mask with zeros
  memset(mask, 0, 32 * 24);

  // iterate through detections
  for (i = 0; i < detections_len; i++) {
    if (detections[i].conf < thresh || detections[i].w == 0 ||
        detections[i].h == 0)
      continue;

    const int x1 = MAX(MIN((int)roundf(detections[i].x - margin), x_max), 0);
    const int y1 = MAX(MIN((int)roundf(detections[i].y - margin), y_max), 0);
    const int x2 = MAX(
        MIN((int)roundf(detections[i].x + detections[i].w + margin), x_max), 0);
    const int y2 = MAX(
        MIN((int)roundf(detections[i].y + detections[i].h + margin), y_max), 0);

    // fill bbox area with ones
    for (y = y1; y < y2; y++) {
      for (x = x1; x < x2; x++) {
        mask[y * 32 + x] = 1;
      }
    }
  }
}

void background_update(int16_t *input, const struct Detection *detections,
                       int detections_len, float ema_decay, float thresh,
                       int16_t *background) {
  int i;
  uint8_t mask[24 * 32];

  background_create_bbox_mask(input, detections, detections_len, 1, thresh,
                              mask);

  for (i = 0; i < (24 * 32); i++) {
    const int16_t value = mask[i] ? background[i] : input[i];
    background[i] = ema_decay * background[i] + (1 - ema_decay) * value;
  }
}
