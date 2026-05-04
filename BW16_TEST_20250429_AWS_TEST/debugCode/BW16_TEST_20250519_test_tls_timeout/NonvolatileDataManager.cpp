#include "NonvolatileDataManager.h"


/* Amazon Root CA can be download here:
 * https://docs.aws.amazon.com/iot/latest/developerguide/server-authentication.html?icmpid=docs_iot_console#server-authentication-certs
 */
//Deqing: also
//Invoke-WebRequest -Uri https://www.amazontrust.com/repository/AmazonRootCA1.pem -OutFile root-CA.crt
char* rootCABuffHardEncoded =  // Amazon Root CA 1 RSA 2048
  "-----BEGIN CERTIFICATE-----\n"
  "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n"
  "ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n"
  "b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n"
  "MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n"
  "b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n"
  "ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n"
  "9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n"
  "IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n"
  "VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n"
  "93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n"
  "jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
  "AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n"
  "A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n"
  "U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n"
  "N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n"
  "o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n"
  "5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n"
  "rqXRfboQnoZsG4q5WTP468SQvvG5\n"
  "-----END CERTIFICATE-----\n";

/* Fill in YOUR certificate.pem.crt with LINE ENDINGS */
char* certificateBuffHardEncoded =
  "-----BEGIN CERTIFICATE-----\n"
  "MIIDWjCCAkKgAwIBAgIVAP+LRDEd0edTDwgxJ2edemCR0NukMA0GCSqGSIb3DQEB\n"
  "CwUAME0xSzBJBgNVBAsMQkFtYXpvbiBXZWIgU2VydmljZXMgTz1BbWF6b24uY29t\n"
  "IEluYy4gTD1TZWF0dGxlIFNUPVdhc2hpbmd0b24gQz1VUzAeFw0yNTA0MTUxNjIz\n"
  "MTNaFw00OTEyMzEyMzU5NTlaMB4xHDAaBgNVBAMME0FXUyBJb1QgQ2VydGlmaWNh\n"
  "dGUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDV1X2FY/DDb7RX2tbv\n"
  "0t2uExY5b1YE02Q7knR8E2RYY+gLwGFyeluiVBMl/xxeLHGZmpsXAXIg4h3jmQqY\n"
  "KeGJcvncAvvtyJUvELQCx7RSqPQU1dr3js4fQAtpqZdk5E3sQWb51mcaNkh82OAN\n"
  "KXvxw920VxoPPAz7nkzWMifvFWwYk/RgtBfT+4Ph/mZpkUCicu1Z2PzufnCYYzOu\n"
  "8Di1h+PgLUFQjfOT4NTZaqJ8/ErzgVJjggMQwo4O9hctVcvSH3cqbdU6kDRQwVKu\n"
  "OxqVMEnPijQBEUz0igxvWtg+0NcqLAwcn2nhHkgw1UyycVjo9EZZt/+xkVlzcLwh\n"
  "YsPNAgMBAAGjYDBeMB8GA1UdIwQYMBaAFBvp9SgBJmqzrijLeRvlea+oSeC0MB0G\n"
  "A1UdDgQWBBQrtWM9kPoYapj+WcCmzhtDX0L7vjAMBgNVHRMBAf8EAjAAMA4GA1Ud\n"
  "DwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAV+OGVckr8VhcQZvKDETZdt8d\n"
  "yszpW4DjziyvrUfxVMTmyGVS5RN9LLbiVEP3JUqEer1V6XqeYp0MsQ1F8K65rojg\n"
  "0+a2wIaQ0pBoi/xR3/EfKlZQdlwFSD8jjhN81Y1u0XP9EnXLtZBFmZI23waQeNhq\n"
  "rFuhtRRAQGOV5kXlQOnLtU1Xr0J0HOxPHLeuOweZb+leTK55Pu3d3vIJIM79iZrI\n"
  "bbC+1FJTmfrUi3t+E+LDruwflgR87+/bDokYE2h48DEok5mkmt0BDcbCHZMgeXvV\n"
  "jw4HBNoEGP4ORv+k2s2yyq+5Bf4ScWbMRbO9KKO0QkMMFVNAGZueuSe5UU9Amg==\n"
  "-----END CERTIFICATE-----\n";

/* Fill in YOUR private.pem.key with LINE ENDINGS */
char* privateKeyBuffHardEncoded =
  "-----BEGIN RSA PRIVATE KEY-----\n"
  "MIIEpAIBAAKCAQEA1dV9hWPww2+0V9rW79LdrhMWOW9WBNNkO5J0fBNkWGPoC8Bh\n"
  "cnpbolQTJf8cXixxmZqbFwFyIOId45kKmCnhiXL53AL77ciVLxC0Ase0Uqj0FNXa\n"
  "947OH0ALaamXZORN7EFm+dZnGjZIfNjgDSl78cPdtFcaDzwM+55M1jIn7xVsGJP0\n"
  "YLQX0/uD4f5maZFAonLtWdj87n5wmGMzrvA4tYfj4C1BUI3zk+DU2WqifPxK84FS\n"
  "Y4IDEMKODvYXLVXL0h93Km3VOpA0UMFSrjsalTBJz4o0ARFM9IoMb1rYPtDXKiwM\n"
  "HJ9p4R5IMNVMsnFY6PRGWbf/sZFZc3C8IWLDzQIDAQABAoIBAQCWGgrGe+UiC2OY\n"
  "2DFQn8Ck1RkgsBq9wHX3q1LBVgt2UIsu1JiS93kjdckLPwz2vlPv8ysy9vzaQF+i\n"
  "yGc7cQq0pVTnL+EQhWDTbPIvkWfvLlJH3eabKE9geGCKh1WSqQBZ+38BmZM+PySb\n"
  "HtIH6zrp9wfF8+6DCzBB4FkncoW9uQ7NpCMVGb1eQgdvW71n1pqDQtPoNzL1cgTi\n"
  "R0jy/hWP8HfEngxj6RR0qy7hvXsrlNyZCcWjw1KZbf6KOrnA26SamdjoAKKlm2ht\n"
  "lw0inDM97Ejvn+gqTCcOMiGPnFewmvi9NkPw0VYfJ+f4pZHvwrfdyCv1/ue82fjr\n"
  "o/fa5XGxAoGBAPkSblj822+t+WH5REMzbx0AmCRGgXyrSwPWeBPPYKPG4OeNJA71\n"
  "B8Gh1xEFKgM3LYzZQWQ7LaYCYTuKqPuiTCHnKMAe6wZC8J2sKkYz9s0xdfHO2D/v\n"
  "66eCjX7CVvhfkGIZUPySIwnqaP9TX1ht5hJ32yXm6cKMsCAP6PUzHzFzAoGBANvI\n"
  "I7BOeSFIFxGV5/yYNMdyl2mLqAQkokibefEvAiU5w9AF5gtToug20fyP3veXGNdx\n"
  "5GivNpzHgSFpj6lyNhWdx0L/tIUqm2/cwJGZWug1/QmyP/0i6G06d1XGIodo6JJQ\n"
  "JLWH60CkhFHgerm4Jz7e99FUAWy8Kguo83f2W+W/AoGAa9igMSXjehrxhYuiFBr8\n"
  "PKmaNvLUdH/S7ml5+tHrfV7K2VgSyestHZmO/w6mX1gQABG+L8E0BdK3+UkT3Eks\n"
  "/+0QhwecKkzn1M3MTDOJ5NVKxZYTqrOe7RwpWj6Z29e9M8zUdVhtlYiLSCr4eNi1\n"
  "Kz/8gw/WHeg/BtL8wtcM5aMCgYEAn9xKgTIDDz81cFgePm+jbDMgiOfJFQJke/WC\n"
  "0/hCUjta/1NbCYATLV66jD9FiceAjSzNW4ueaJkAhwIWOcTWLBDwX/5IGthr6Qij\n"
  "lQP7yI2EeOoLex9J+jEdnekZMm8PQ2VB3jx31DQV/swN2EpnWWaq0LqOsr2Gw6Zn\n"
  "OSmbnFcCgYAmEGzUkqosBRj+9ryo99R3E7KiOj0ZgalAivbRhFEPBYoMqboP4TDk\n"
  "q7RPlHDmV5RA7IYoGsfwmdpJAxsaAb/GL8wvh+eDlNeKo4CRK2cH5anG4FgfHz0v\n"
  "E9v4E41uxURMvEpRES0bUTKiVfXWtM6TQfOmZq7tjVkfNUYl+PgTkw==\n"
  "-----END RSA PRIVATE KEY-----\n";

unsigned char* NonvolatileDataManager::getRootCA() {
  return (unsigned char*)rootCABuffHardEncoded;
}

unsigned char* NonvolatileDataManager::getPrivateKey() {
  return (unsigned char*)privateKeyBuffHardEncoded;
}

unsigned char* NonvolatileDataManager::getCertificate() {
  return (unsigned char*)certificateBuffHardEncoded;
}

// todo: check if the FlashMemory.h is good for the 4MB OTA layout
// by default it uses #define FLASH_MEMORY_APP_BASE 0x00100000
// but it can be updated with void begin(unsigned int _base_address, unsigned int _buf_size);
// let's keep the _buf_size to be FLASH_SECTOR_SIZE 0x1000 as the code does not support multiple sectors well
// but we can create multiple instances of FlashMemory with different base addresses

// in AN0400 for 2MB layout (default)
// User Data 0x0810_0000 0x0810_1FFF 8K  Reserved for user data
// User Data 0x0810_2000 0x0810_4FFF 12K BT FTL physical map
// User Data 0x0810_5000 0x0810_5FFF 4K  WIFI Fast Connect profile

// in datasheet for 4MB layout (since we are using binary modified bootloader may be we do not need to follow this?)
// or maybe we should, as the a 2MB IMG1 will overwrite the user space in the 4MB layout
// User Data 0x0820_0000 0x0820_1FFF 8K  Reserved for user data
// User Data 0x0820_2000 0x0820_4FFF 12K BT FTL physical map
// User Data 0x0820_5000 0x0820_5FFF 4K  WIFI Fast Connect profile 

// On 20250516, I think the booloader does not care the user data layout,
// it will just use the OTA_Region to check signature and the start address
// LS_IMG2_OTA2_ADDR seems only used by void sys_clear_ota_signature(void) from rtl8721d_ota.h
// FTL_PHY_PAGE_START_ADDR only used in void BLEDevice::init()
// FAST_RECONNECT_DATA seems not used as CONFIG_EXAMPLE_WLAN_FAST_CONNECT is 0
// we are using the user data, and BT setting for WIFI, let's do a dump.
// when do the dump, sketch takes 979708, and 0x08006000 + 0xEF2FC = 0x80F52FC
// 0x0810_0000 - 0x0810_00A1 not 0xFF, 162 bytes = storedSsid[33]+storedPassword[129]
// 0x0810_2000 - 0x0810_2003 not 0xFF, 4 bytes, seems used by the BT FTL
// let wait till the the firmware grow to 1024000 (0xFA000) to see if the firmware collides with the user data

#include <FlashMemory.h>

char storedSsid[33] = {0};
char storedPassword[129] = {0};

void NonvolatileDataManager::getSsidPasswordFromFlash(char **ssid, char **password){
    FlashMemory.read();
    int lengthSSID = 33;
    int lengthPassword = 129;
    for (int i = 0; i < 33; i++){
        storedSsid[i] = FlashMemory.buf[i+0];
    }
    for (int i = 0; i < 129; i++){
        storedPassword[i] = FlashMemory.buf[i+33];
    }
    //check if there is a valid string ending in ssid and password
    bool ssidValidEnding = false;
    for (int i = 0; i < lengthSSID; i++){
        if (storedSsid[i] == 0){
            ssidValidEnding = true;
            break;
        }
    }
    if (ssidValidEnding == false){
        for (int i = 0; i < lengthSSID; i++){
            storedSsid[i] = 0;
        }
    }

    bool passwordValidEnding = false;
    for (int i = 0; i < lengthPassword; i++){
        if (storedPassword[i] == 0){
            passwordValidEnding = true;
            break;
        }
    }
    if (passwordValidEnding == false){
        for (int i = 0; i < lengthPassword; i++){
            storedPassword[i] = 0;
        }
    }

    *ssid = storedSsid;
    *password = storedPassword;
}

void NonvolatileDataManager::saveSsidPasswordToFlash(char *ssid, char *password){
    bool ssidAndPasswordTheSame = true;
    if (strcmp(ssid, storedSsid) != 0){
        ssidAndPasswordTheSame = false;
    }
    if (strcmp(password, storedPassword) != 0){
        ssidAndPasswordTheSame = false;
    }
    
    if (ssidAndPasswordTheSame){
        Serial.println("SSID and password are the same, not saving to flash.");
        return;
    } else {
        Serial.println("Saving SSID and password to flash.");
        memcpy(storedSsid, ssid, 33);
        memcpy(storedPassword, password, 129);
        FlashMemory.read();
        for (int i = 0; i < 33; i++){
            FlashMemory.buf[i+0] = ssid[i];
        }
        for (int i = 0; i < 129; i++){
            FlashMemory.buf[i+33] = password[i];
        }
        FlashMemory.update();
    }
}

void NonvolatileDataManager::printMemoryDump(unsigned int startAddress, unsigned int length){
    Serial.print("Memory dump from address: ");
    Serial.print(startAddress);
    Serial.print(" length: ");
    Serial.println(length);
    for (int i = 0; i < (int)length; i+=16){
      uint8_t buf[16];
      uint8_t all0xFF = 1;
      for (int j = 0; j < 16; j++){
        uint8_t *p = (uint8_t *)(startAddress + i + j);
        uint8_t data = *p;
        if (data != 0xFF){
            all0xFF = 0;
        }
        buf[j] = data;
      }
      if (!all0xFF){
        Serial.print("0x");
        Serial.print(startAddress + i, HEX);
        Serial.print(": ");
        for (int j = 0; j < 16; j++){
            Serial.print("0x");
            Serial.print(buf[j], HEX);
            Serial.print(" ");
        }
        Serial.println();

      }
    }
}


char* thingNameNV = "amebaDevBoard";
char* clientIdNV = "amebaClient";

char *NonvolatileDataManager::getThingName(){
    return thingNameNV;
}
char *NonvolatileDataManager::getClientId(){
    return clientIdNV;
}
