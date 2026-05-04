#ifndef POST_PROCESS_H
#define POST_PROCESS_H

#define MODEL_OUTPUT_QUANT_SCALE 0.03067750856280327
#define MODEL_OUTPUT_QUANT_ZERO 57

#ifdef __cplusplus
extern "C" {
#endif

#include "data_types.h"

/*
 *  Retrieve bboxes from the network's output tensor. We currently only support
 * single class object detectors and assume the output of the network (tensor
 * param) is of type int8. tensor:         network output tensor (3D), expecting
 * this to be trained with YOLOV2 region loss thresh:         Confidence
 * threshold. Detections with a confidence lower than the confidence threshold
 * are ignored anchors:        Array of anchor priors for each output anchor
 *  anchors_len:    Number of anchor priors.
 *  coord_scale:    Scales the bounding box cooridnates towards some target
 * image size with dimensions [tensor_width * coord_scale x tensor_height *
 * coord_scale] where tensor_width and tensor_height are the the width and
 * height dims of the 'tensor' parameter. detections:     Array to store the
 * output detections detections_len: Size of the detections array
 */
int post_get_bboxes(int8_t *input, float thresh, const struct Anchor *anchors,
                    int anchors_len, int coord_scale,
                    struct Detection *detections, int detections_len);

float post_box_iou(const struct Detection a, const struct Detection b);

/*
 *  Suppress detections with high overlap by setting the confidence to zero
 *  detections:     Array of detection
 *  detections_len: Length of the detections array
 *  thresh:         IOU threshold. If the IOU > thresh, the bbox with the lowest
 * conf score is pruned
 */
int post_nms(struct Detection *detections, int detections_len, float thresh);

#ifdef __cplusplus
}
#endif

#endif /* POST_PROCESS_H */
