#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>


#include "../src/mlx90640YoloProcessor/mlx90640YoloProcessor.h"

int main(int argc, char** argv)
{   
    (void)argc;
    (void)argv;

    float temperatures[30][24*32];
    FILE *fp = fopen("Serial Debug 2025-05-06 164458.txt", "r");
    if (fp == NULL) {
        printf("Error opening file\n");
        return -1;
    }
    char line[1024];
    int frameIndex = 0;
    int valueIndex = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (frameIndex >= 30) {
            break;
        }
        if (line[0] == '=') {
            // Start of a new frame

            valueIndex = 0;
        } else {
            //sample line
            //27.6, 27.6, 28.2, 28.5, 28.4, 27.5, 27.8, 27.7, 27.9, 28.3, 27.3, 27.2, 27.1, 27.1, 27.0, 27.5, 27.0, 26.5, 26.1, 26.6, 27.2, 27.3, 27.0, 27.2, 27.3, 27.3, 27.4, 27.3, 27.4, 26.9, 27.3, 27.3, 
            //Parse the line and extract temperature values

            //check if the line has at least 31 ,
            int commaCount = 0;
            for (int i = 0; line[i] != '\0'; i++) {
                if (line[i] == ',') {
                    commaCount++;
                }
            }
            if (commaCount < 31) {
                continue;
            }

            char *token = strtok(line, ",");
            while (token != NULL && valueIndex < 24*32) {
                temperatures[frameIndex][valueIndex] = atof(token);
                // check if the token part is a number
                if (token[0] == '-' || (token[0] >= '0' && token[0] <= '9')) {
                    valueIndex++;
                    if (valueIndex == 24*32) {
                        frameIndex++;
                        valueIndex = 0;
                    }
                } else {
                    //printf("Invalid number: %s\n", token);
                }
                token = strtok(NULL, ",");
            }
        }
    }
    fclose(fp);

    // //print frame 1
    // for (int y = 0; y < 24; y++) {
    //     for (int x = 0; x < 32; x++) {
    //         printf("%.1f ", temperatures[1][y*32+x]);
    //     }
    //     printf("\n");
    // } 

        // if (fp == NULL) {
        //     printf("Error opening file %s\n", filename);
        //     return -1;
        // }

    // loadFileName = "Serial Debug 2025-05-06 164458.txt"
    // allFrames = []
    // with open(loadFileName, "r") as f:
    //     lines = f.readlines()
    //     valuesForFrame = []
    //     for line in lines:
    //         if line.startswith("=========="):
    //             valuesForFrame = []
    //         else:
    //             splited = line.strip().strip(",").split(",")
    //             if len(splited) == 32:
    //                 for i in range(32):
    //                     valuesForFrame.append(float(splited[i]))
    //                 if len(valuesForFrame) == 32*24:
    //                     allFrames.append(valuesForFrame)




    



    for (int i=2;i<30;i++){
        // load with open(f"output/diffInt8_{i:03d}.bin", "wb") as f:
        // char filename[128];
        // sprintf(filename, "testDataDirectIn/diffInt8_%03d.bin", i);
        // printf("open file %s\n", filename);
        // FILE *fp = fopen(filename, "rb");
        // if (fp == NULL) {
        //     printf("Error opening file %s\n", filename);
        //     return -1;
        // }
        
        // uint8_t dataBuf[24*32];
        // int readRet = fread(dataBuf, 1, 24*32, fp);
        // (void) readRet;
        // // printf("read %d bytes\n", readRet);
        // // printf("dataBuf[0] %02x\n", dataBuf[0]);
        // fclose(fp);


        printf("feeding frame %d\n", i);
        initMLX90640YoloProcessor();
        feedFloatMLX90640YoloProcessorAndRun(temperatures[i]);
        deinitMLX90640YoloProcessor();

        struct Detection *detections = NULL;
        int detectionsCount = getDetectionsCount(&detections);

        int frameIndex = i-2;

        //=======debug=========
        char filename[128];
        sprintf(filename, "debug/c_gen_float_%03d.txt", frameIndex);
        FILE *fp = fopen(filename, "w");
        if (fp == NULL) {
            printf("Error opening file %s\n", filename);
        }
        float *imageOneFrame = temperatures[i];
        for (int i1=0;i1<24*32;i1++){
            fprintf(fp, "%0.2f ", imageOneFrame[i1]);
            if (i1%32==31){
                fprintf(fp, "\n");
            }
        } 
        fclose(fp);
        //=======debug=========


        //=======debug=========
        char filename2[128];
        sprintf(filename2, "debug/c_gen_detection_%03d.txt", frameIndex);
        FILE *fp2 = fopen(filename2, "w");
        if (fp2 == NULL) {
            printf("Error opening file %s\n", filename2);
        }
        for (int i=0;i<detectionsCount;i++){
            fprintf(fp2, "%0.2f %0.2f %0.2f %0.2f %0.2f\n", detections[i].x, detections[i].y, detections[i].w, detections[i].h, detections[i].conf);
        } 
        fclose(fp2);
        // //=======debug=========
        



    }


  //  tm_unload(&mdl);     
    printf("done\n");            
    return 0;
}