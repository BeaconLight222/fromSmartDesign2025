#ifndef NONVOLATILE_DATA_MANAGER_H
#define NONVOLATILE_DATA_MANAGER_H

#include <Arduino.h>

class NonvolatileDataManager {
public:
  static unsigned char* getRootCA();
  static unsigned char* getPrivateKey();
  static unsigned char* getCertificate();

  static void getSsidPasswordFromFlash(char **ssid, char **password);
  static void saveSsidPasswordToFlash(char *ssid, char *password);

  static char *getThingName();
  static char *getClientId();

  static void printMemoryDump(unsigned int startAddress, unsigned int length);
};


#endif  // NONVOLATILE_DATA_MANAGER_H
