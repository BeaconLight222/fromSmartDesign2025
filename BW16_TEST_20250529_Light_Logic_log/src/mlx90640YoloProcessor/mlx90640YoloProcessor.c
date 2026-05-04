#include "mlx90640YoloProcessor.h"
#include "../TinyMaix/include/tinymaix.h"
#include "background.h"
#include "g_model_45.h"
#include "macros.h"
#include "post_process.h"

#include <math.h>

const struct Anchor k_anchors[5] = {{8.853734918993446, 12.044021027232077},
                                    {7.7503849920115115, 6.525571303406621},
                                    {4.530664042752944, 7.975254206687415},
                                    {4.25046050720126, 5.26986904910139},
                                    {2.5676662786561533, 3.5365033789243747}};

tm_mdl_t mdl;
int8_t intBuf[24 * 32];
tm_mat_t in = {3, 24, 32, 1, {(mtype_t *)intBuf}};
tm_mat_t outs[1];

int16_t backgroundData[24 * 32];

float detectionThreshold = 0.4;
float nmsThreshold = 0.3;
float backgroundEmaDecay = 0.8;
int backgroundUpdateSteps = 2;
float thresholdForBackgroundSub = 0.4;

uint8_t firstTimeBackgroundUpdate = 1;
int backgroundUpdateCount = 0;

#define MODEL_MAX_DETECTIONS ((6 * 8 * 5) / 10)

#define MODEL_INPUT_QUANT_SCALE 0.10532574355602264
#define MODEL_INPUT_QUANT_ZERO -74

struct Detection detections[MODEL_MAX_DETECTIONS];
int validDetections = 0;

static tm_err_t layer_cb(tm_mdl_t *mdl, tml_head_t *lh) {
  (void)mdl;
  (void)lh;
  return TM_OK;
  // {   //dump middle result
  //     int h = lh->out_dims[1];
  //     int w = lh->out_dims[2];
  //     int ch= lh->out_dims[3];
  //     mtype_t* output = TML_GET_OUTPUT(mdl, lh);
  //     return TM_OK;
  //     TM_PRINTF("Layer %d callback ========\n", mdl->layer_i);
  //     #if 1
  //     for(int y=0; y<h; y++){
  //         TM_PRINTF("[");
  //         for(int x=0; x<w; x++){
  //             TM_PRINTF("[");
  //             for(int c=0; c<ch; c++){
  //             #if TM_MDL_TYPE == TM_MDL_FP32
  //                 TM_PRINTF("%.3f,", output[(y*w+x)*ch+c]);
  //             #else
  //                 TM_PRINTF("%.3f,", TML_DEQUANT(lh,output[(y*w+x)*ch+c]));
  //             #endif
  //             }
  //             TM_PRINTF("],");
  //         }
  //         TM_PRINTF("],\n");
  //     }
  //     TM_PRINTF("\n");
  //     #endif
  //     return TM_OK;
}

int initMLX90640YoloProcessor(void) {
  TM_DBGT_INIT();
  // TM_PRINTF("yolov2 demo\n");
  tm_err_t res;

  // tm_stat((tm_mdlbin_t*)mdl_data);

  res = tm_load(&mdl, mdl_data, NULL, layer_cb, &in);
  if (res != TM_OK) {
    TM_PRINTF("tm model load err %d\n", res);
    return -1;
  }
  return 0;
}

void deinitMLX90640YoloProcessor(void) {
  tm_unload(&mdl);
  // TM_PRINTF("yolov2 demo deinit\n");
}

// void feedInt8MLX90640YoloProcessor(int8_t *dataBuf){
//     memcpy(intBuf, dataBuf, 24*32);
//     tm_mat_t in_uint8 = {3, 24, 32, 1, {(mtype_t*)intBuf}};
//     tm_preprocess(&mdl, TMPP_NONE, &in_uint8, &in);
// }

// int runMLX90640YoloProcessor(){
//     tm_err_t res;

//     res = tm_run(&mdl, &in, outs);
//     if(res != TM_OK) {
//         TM_PRINTF("tm run err %d\n", res);
//         return -1;
//     }

//     int detectionsCount = post_get_bboxes(
//         (int8_t*)outs[0].data,
//         detectionThreshold,
//         k_anchors,
//         5,
//         4,
//         detections,
//         MODEL_MAX_DETECTIONS
//     );

//     int validDetections = post_nms(detections, detectionsCount,
//     nmsThreshold);    // suppress by setting confidence to zero

//     TM_PRINTF("validDetections %d\n", validDetections);
//     for (int i=0; i<validDetections; i++){
//         TM_PRINTF("valid detection %d: x %.2f y %.2f w %.2f h %.2f conf
//         %.2f\n", i, detections[i].x, detections[i].y, detections[i].w,
//         detections[i].h, detections[i].conf);
//     }

//     char filename[128];
//     sprintf(filename, "debug/c_gen_detection_%03d.txt",
//     outputCounterForDebug); FILE *fp = fopen(filename, "w"); if (fp == NULL)
//     {
//         printf("Error opening file %s\n", filename);
//     }
//     for (int i=0;i<validDetections;i++){
//         fprintf(fp, "%0.2f %0.2f %0.2f %0.2f %0.2f\n", detections[i].x,
//         detections[i].y, detections[i].w, detections[i].h,
//         detections[i].conf);
//     }
//     fclose(fp);

//     // update background image once in a while
//     backgroundUpdateCount++;
//     if (backgroundUpdateCount >= backgroundUpdateSteps) {
//         printf("background update\n");
//         backgroundUpdateCount = 0;
//         background_update(scaledData, detections, 0, backgroundEmaDecay,
//         thresholdForBackgroundSub, backgroundData);
//     }

//     return 0;
// }

// void feedFloatMLX90640YoloProcessor(float *image){

//     for (int i=0;i<24*32;i++){
//         scaledData[i] = (int16_t)(image[i]*100);
//     }
//     if (firstTimeBackgroundUpdate){
//         firstTimeBackgroundUpdate = 0;
//         backgroundUpdateCount = -1;
//         background_init(scaledData, backgroundData);
//     }

//     // pre-process input
//     for (int i=0; i<(24*32); i++) {
//         const int32_t value = (scaledData[i] - backgroundData[i]) *
//         MODEL_INPUT_QUANT_SCALE + MODEL_INPUT_QUANT_ZERO; intBuf[i] =
//         MAX(MIN(127, value), -128);
//     }

//     char filename[128];
//     sprintf(filename, "debug/c_gen_float_%03d.txt", outputCounterForDebug);
//     FILE *fp = fopen(filename, "w");
//     if (fp == NULL) {
//         printf("Error opening file %s\n", filename);
//         return;
//     }
//     for (int i=0;i<24*32;i++){
//         fprintf(fp, "%0.2f ", image[i]);
//         if (i%32==31){
//             fprintf(fp, "\n");
//         }
//     }
//     fclose(fp);

//     outputCounterForDebug++;

//     tm_mat_t in_uint8 = {3, 24, 32, 1, {(mtype_t*)intBuf}};
//     tm_preprocess(&mdl, TMPP_NONE, &in_uint8, &in);

// }

void feedFloatMLX90640YoloProcessorAndRun(float *image) {
  int16_t scaledData[24 * 32];

  for (int i = 0; i < 24 * 32; i++) {
    scaledData[i] = (int16_t)(image[i] * 100);
  }
  if (firstTimeBackgroundUpdate) {
    firstTimeBackgroundUpdate = 0;
    backgroundUpdateCount = -1;
    background_init(scaledData, backgroundData);
  }

  // pre-process input
  for (int i = 0; i < (24 * 32); i++) {
    const int32_t value =
        (scaledData[i] - backgroundData[i]) * MODEL_INPUT_QUANT_SCALE +
        MODEL_INPUT_QUANT_ZERO;
    intBuf[i] = MAX(MIN(127, value), -128);
  }

  tm_mat_t in_uint8 = {3, 24, 32, 1, {(mtype_t *)intBuf}};
  tm_preprocess(&mdl, TMPP_NONE, &in_uint8, &in);

  tm_err_t res;

  res = tm_run(&mdl, &in, outs);
  if (res != TM_OK) {
    TM_PRINTF("tm run err %d\n", res);
    return;
  }

  int detectionsCount =
      post_get_bboxes((int8_t *)outs[0].data, detectionThreshold, k_anchors, 5,
                      4, detections, MODEL_MAX_DETECTIONS);

  validDetections =
      post_nms(detections, detectionsCount,
               nmsThreshold); // suppress by setting confidence to zero

  // TM_PRINTF("validDetections %d\n", validDetections);
  // for (int i=0; i<validDetections; i++){
  //     TM_PRINTF("valid detection %d: x %.2f y %.2f w %.2f h %.2f conf
  //     %.2f\n", i, detections[i].x, detections[i].y, detections[i].w,
  //     detections[i].h, detections[i].conf);
  // }

  // update background image once in a while
  backgroundUpdateCount++;
  if (backgroundUpdateCount >= backgroundUpdateSteps) {
    // printf("background update\n");
    backgroundUpdateCount = 0;
    background_update(scaledData, detections, validDetections,
                      backgroundEmaDecay, thresholdForBackgroundSub,
                      backgroundData);
  }
}

int getDetectionsCount(struct Detection **_detections) {
  *_detections = detections;
  return validDetections;
}
