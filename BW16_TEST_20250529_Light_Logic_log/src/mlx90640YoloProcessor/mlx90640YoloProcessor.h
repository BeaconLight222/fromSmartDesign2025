#ifndef MLX90640YOLOPROCESSOR_H
#define MLX90640YOLOPROCESSOR_H

#include "data_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int initMLX90640YoloProcessor(void);
void deinitMLX90640YoloProcessor(void);
// void feedInt8MLX90640YoloProcessor(int8_t *dataBuf);
// void feedFloatMLX90640YoloProcessor(float *image);
void feedFloatMLX90640YoloProcessorAndRun(float *image);
int getDetectionsCount(struct Detection **_detections);
// int runMLX90640YoloProcessor();

#ifdef __cplusplus
}
#endif

#endif // MLX90640YOLOPROCESSOR_H