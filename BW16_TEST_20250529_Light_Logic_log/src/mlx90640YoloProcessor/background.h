#ifndef BACKGROUND_H
#define BACKGROUND_H

#ifdef __cplusplus
extern "C" {
#endif

#include "data_types.h"

/*
 *  Create the initial background image
 *  input:      input image. If NULL, init will restart.
 *  background: the initialized background image will be stored here. If NULL,
 * init will restart
 *
 *  Call this function multiple times with new input images until it returns 0.
 *
 *  Note that this function keeps call count in a static library variable.
 *  Note that input and background need to be allocated in advance.
 */
int background_init(int16_t *input, int16_t *background);

/*
 *  Create an image mask from bounding boxes: 255 inside bbox, 0 elsewhere
 */
void background_create_bbox_mask(int16_t *input,
                                 const struct Detection *detections,
                                 int detections_len, float margin, float thresh,
                                 uint8_t *mask);

/*
 *  Background updating algorithm
 *  input:          input tensor containing the latest input image
 *  detections:     array of latest detections
 *  detections_len: length of the detections array
 *  ema_decay:      Exponential Moving Average decay value
 *  thresh:         Don't consider bboxes with confidence scores below this
 * threshold background:     the current background image. This image will be
 * updated. Note that input/output tensors need to be pre-allocated
 */
void background_update(int16_t *input, const struct Detection *detections,
                       int detections_len, float ema_decay, float thresh,
                       int16_t *background);

#ifdef __cplusplus
}
#endif

#endif /* BACKGROUND_H */
