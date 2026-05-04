#include "post_process.h"
#include "macros.h"
#include <math.h>
#include <stdlib.h>

static inline float sigmoid(float x) { return 1.f / (1.f + exp(-x)); }

static inline float dequant(int8_t x, float scale, int32_t zero_point) {
  return ((int32_t)(x)-zero_point) * scale;
}

// compare function for qsort in descending order
static int compare_nms(const void *a, const void *b) {
  const struct Detection *_a = a;
  const struct Detection *_b = b;
  return (_a->conf < _b->conf) - (_a->conf > _b->conf);
}

int post_get_bboxes(int8_t *input, float thresh, const struct Anchor *anchors,
                    int anchors_len, int coord_scale,
                    struct Detection *detections, int detections_len) {
  // Expect tensor in HWC format
  // Channels dims have order: x, y, w, h, conf, x, y, w, h, conf ... x, y, w,
  // h, conf
  const int net_out_h = 6;
  const int net_out_w = 8;
  const int num_channels = 25;

  if (num_channels != (anchors_len * 5))
    return -1;

  const int32_t q_zero = MODEL_OUTPUT_QUANT_ZERO;
  const float q_scale = MODEL_OUTPUT_QUANT_SCALE;

  // conf is calculated from int8 tensor intput as: conf =  sigmoid((in_int8 -
  // q_zero) * q_scale) instead of evaluating this expression for every element
  // in the tensor, we reverse calculate what the int8 threshold should be and
  // use that for comparison which is much faster since we only need to do this
  // once
  const int32_t int_thresh = round(-log(1.f / thresh - 1.f) / q_scale + q_zero);
  const int8_t int8_thresh =
      MAX(MIN(int_thresh, 127), -128); // ensure it is within range

  int det_index = 0;
  int index = 0;

  // iterate through tensor in HWC fashion
  for (int y = 0; y < net_out_h; y++) {
    for (int x = 0; x < net_out_w; x++) {
      for (int anchor = 0; anchor < anchors_len; anchor++) {
        const int _index = index;
        index += 5;

        // extract objectness
        const int8_t int8_objectness = input[_index + 4];

        // extract class probs if objectness >= threshold
        if (int8_objectness < int8_thresh)
          continue;

        detections[det_index].conf =
            sigmoid(dequant(int8_objectness, q_scale, q_zero));

        // dequant box coordinates
        const float pred_x = dequant(input[_index], q_scale, q_zero);
        const float pred_y = dequant(input[_index + 1], q_scale, q_zero);
        const float pred_w = dequant(input[_index + 2], q_scale, q_zero);
        const float pred_h = dequant(input[_index + 3], q_scale, q_zero);

        // extract box coordinates
        const float bbox_x = (x + sigmoid(pred_x)) * coord_scale;
        const float bbox_y = (y + sigmoid(pred_y)) * coord_scale;
        const float bbox_w = (anchors[anchor].w * exp(pred_w)) * coord_scale;
        const float bbox_h = (anchors[anchor].h * exp(pred_h)) * coord_scale;

        // convert center xy to top left xy
        detections[det_index].x = bbox_x - bbox_w / 2.f;
        detections[det_index].y = bbox_y - bbox_h / 2.f;
        detections[det_index].w = bbox_w;
        detections[det_index].h = bbox_h;

        det_index++;
        if (det_index == detections_len)
          return det_index;
      }
    }
  }

  return det_index;
}

float post_box_iou(const struct Detection a, const struct Detection b) {
  // calculate box areas
  const float area_a = a.w * a.h;
  const float area_b = b.w * b.h;
  if (area_a <= 0 || area_b <= 0)
    return 0.0;

  // calculate intersection area
  const float intersection_ymin = MAX(a.y, b.y);
  const float intersection_xmin = MAX(a.x, b.x);
  const float intersection_ymax = MIN(a.y + a.h, b.y + b.h);
  const float intersection_xmax = MIN(a.x + a.w, b.x + b.w);
  const float intersection_area =
      MAX(intersection_ymax - intersection_ymin, 0) *
      MAX(intersection_xmax - intersection_xmin, 0);

  return intersection_area / (float)(area_a + area_b - intersection_area);
}

int post_nms(struct Detection *detections, int detections_len, float thresh) {
  int i, j;

  // sort detections by confidence in descending order
  qsort(detections, detections_len, sizeof(struct Detection), compare_nms);

  // suppress by setting detection probabilities to zero
  for (i = 0; i < detections_len; i++) {
    if (detections[i].conf == 0.0)
      continue;
    for (j = i + 1; j < detections_len; j++) {
      if (post_box_iou(detections[i], detections[j]) > thresh)
        detections[j].conf = 0.0;
    }
  }

  int valid_detections = 0;
  // move valid detections to the front of the array
  for (i = 0; i < detections_len; i++) {
    if (detections[i].conf > 0.0) {
      if (valid_detections != i) {
        detections[valid_detections] = detections[i];
      }
      valid_detections++;
    }
  }

  return valid_detections;
}
