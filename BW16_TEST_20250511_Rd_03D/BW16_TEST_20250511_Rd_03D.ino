#include "src/Simple_Rd_03d/Simple_Rd_03d.h"

// Rd-03D module communicates through serial port (TTL level), and the radar serial port default baud rate is 256000, 
// 1 stop bit, no parity bit. 
// The radar outputs the detected target information, including the x coordinate, y coordinate in the area, 
// and the speed value of the target.

// By default, the radar is in single target detection mode and needs to be switched to multi-target detection mode.

// single target mode	  FD FC FB FA 02 00 80 00 04 03 02 01
// response             FD FC FB FA 04 00 80 01 01 00 04 03 02 01
// multiple target mode	FD FC FB FA 02 00 90 00 04 03 02 01
// response             FD FC FB FA 04 00 90 01 01 00 04 03 02 01
// copied from RD-03, not RD-03D
// open command mode    FD FC FB FA 04 00 FF 00 01 00 04 03 02 01
// response             FD FC FB FA 08 00 FF 01 00 00 01 00 40 00 04 03 02 01
// seems streaming got disabled
// close command mode   FD FC FB FA 02 00 FE 00 04 03 02 01
// response             FD FC FB FA 04 00 FE 01 00 00 04 03 02 01
// seems streaming got enabled


// 上报是数据帧格式：

// 帧头部	帧内数据	帧尾部
// AA FF 03 00	目标1信息 目标2信息 目标3信息	55 CC
// 数据示例：AA FF 03 00 0E 03 B1 86 10 00 68 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 55 CC

// 红色部分表示目标1的信息，蓝色表示目标2的信息，绿色表示目标3的信息。

// 本示例只展示有无人，所以只需要判断在对应的数据帧中有没有相应的目标信息即可。
// ————————————————

//                             版权声明：本文为博主原创文章，遵循 CC 4.0 BY-SA 版权协议，转载请附上原文出处链接和本声明。
                        
// 原文链接：https://blog.csdn.net/Boantong_/article/details/133338984

Simple_Rd_03D radar1,radar2;

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  int radarRet = radar1.begin(0);
  if (radarRet){
    Serial.println("Radar1 initialized successfully");
  }else{
    Serial.println("Radar1 initialization failed");
  }
  //radar1.disableRadarStreaming();
  radarRet = radar2.begin(1);
  if (radarRet){
    Serial.println("Radar2 initialized successfully");
  }else{
    Serial.println("Radar2 initialization failed");
  }
  //radar2.disableRadarStreaming();

  pinMode(PB3, OUTPUT);
}

void loop() {

  radar1.activeSerialMux();
  //radar1.enableRadarStreaming();
  int radarNewdata = radar1.checkRadarData(200);  //no enable/disable streaming, takes about 80ms, enable/disable streaming takes about 180ms
  if (radarNewdata){
    Serial.println("Radar1 data: ");
    for (int i = 0; i < radar1.radarObjectCount; i++){
      Serial.print("Object ");
      Serial.print(i);
      Serial.print(": ");
      Serial.print(radar1.radarObject[i].x);
      Serial.print(", ");
      Serial.print(radar1.radarObject[i].y);
      Serial.print(", ");
      Serial.print(radar1.radarObject[i].speed);
      Serial.print(", ");
      Serial.println(radar1.radarObject[i].resolution);
    }
  }
  //radar1.disableRadarStreaming();

  radar2.activeSerialMux();
  //radar2.enableRadarStreaming();
  radarNewdata = radar2.checkRadarData(200);
  if (radarNewdata){
    Serial.println("Radar2 data: ");
    for (int i = 0; i < radar2.radarObjectCount; i++){
      Serial.print("Object ");
      Serial.print(i);
      Serial.print(": ");
      Serial.print(radar2.radarObject[i].x);
      Serial.print(", ");
      Serial.print(radar2.radarObject[i].y);
      Serial.print(", ");
      Serial.print(radar2.radarObject[i].speed);
      Serial.print(", ");
      Serial.println(radar2.radarObject[i].resolution);
    }
  }
  //radar2.disableRadarStreaming();
}
