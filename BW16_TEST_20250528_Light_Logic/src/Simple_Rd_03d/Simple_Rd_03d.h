#ifndef SIMPLE_RD_03D_H
#define SIMPLE_RD_03D_H
#include <Arduino.h>

#define UART_MUX_A_PIN PA27
#define UART_MUX_B_PIN PA30

typedef struct {
    int x;
    int y;
    int speed;
    int resolution;
} radarObject_t;

class Simple_Rd_03D{
    public:
        Simple_Rd_03D();
        boolean begin(uint8_t _serial_mux = 0);
    
        uint8_t serial_mux;

        int checkRadarData(int timeout = 10);
        void activeSerialMux();
        void disableRadarStreaming();
        void enableRadarStreaming();

        char radarRecvBuffer[30];
        int radarRecvBufferIndex;
        int radarDataLength;

        radarObject_t radarObject[3];
        int radarObjectCount;

};

#endif
