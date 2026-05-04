/// @file NonvolatileDataManager.cpp
/// @brief Nonvolatile data manager for the BW16 project, handles WIFI credentials and other certificates and keys for AWS.

#include "NonvolatileDataManager.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "flash_api.h"
#ifdef __cplusplus
}
#endif

// by default, the user data region is 0x2000 big, that is 8192 bytes
// Amazon Root CA 1188 bytes
// privateKey 1679
// certificate 1224
// OTA signature key size = 566
// total = 1188 + 1679 + 1224 + 566 = 3857 we should be fine
// if we take out the OTA signature key and the Amazon Root CA
// that would be 1679 + 1224 = 2903, that can easily fit in one page (4096,
// 0x1000)

#define CLIENT_ID_FLASH_ADDR 0x8201000
#define THING_NAME_FLASH_ADDR 0x8201040
#define CERT_FLASH_ADDR 0x8201080
#define PRIVATE_KEY_FLASH_ADDR 0x820165c

char *emptyString = "";

char *storedSsid;
char *storedPassword;

/* Amazon Root CA can be download here:
 * https://docs.aws.amazon.com/iot/latest/developerguide/server-authentication.html?icmpid=docs_iot_console#server-authentication-certs
 */
// Deqing: also
// Invoke-WebRequest -Uri
// https://www.amazontrust.com/repository/AmazonRootCA1.pem -OutFile root-CA.crt
/// @brief Amazon Root CA 1 certificate, hardcoded as all devices will use the same CA
char *rootCABuffHardEncoded = // Amazon Root CA 1 RSA 2048
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

/// @brief Private key for the device, hardcoded as all devices will use the same key
char *firmwareSignPubKey =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBdjCCAR2gAwIBAgIJAJehK77gCF87MAoGCCqGSM49BAMCMC4xLDAqBgNVBAMM\n"
    "I2RlcWluZy5zdW5Ac21hcnRkZXNpZ253b3JsZHdpZGUuY29tMB4XDTI1MDQzMDA1\n"
    "MzUzNFoXDTI2MDQzMDA1MzUzNFowLjEsMCoGA1UEAwwjZGVxaW5nLnN1bkBzbWFy\n"
    "dGRlc2lnbndvcmxkd2lkZS5jb20wWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARp\n"
    "3earZ14OqZigRBHKP3KQ5G1afiwWHFYJHNsutZ0BSU6b6rIoJQQKr/S/lt1wrheJ\n"
    "U9EOOLzq023NSjYqmTbBoyQwIjALBgNVHQ8EBAMCB4AwEwYDVR0lBAwwCgYIKwYB\n"
    "BQUHAwMwCgYIKoZIzj0EAwIDRwAwRAIgJjKIaqOukQlVQMSRihgAsTZ2dKcH5qKb\n"
    "+ClvfUUNKRYCIBExWYBNwZCzCGpKQ1V/qilbeiZeG/VBE8EEXCi7XR9N\n"
    "-----END CERTIFICATE-----\n";

/// @brief Get the Amazon Root CA certificate
/// @return The Amazon Root CA certificate as a string
unsigned char *NonvolatileDataManager::getRootCA() {
  return (unsigned char *)rootCABuffHardEncoded;
}

/// @brief Get the private key for the device from user data partition
/// @return The private key as a string
unsigned char *NonvolatileDataManager::getPrivateKey() {
  char *privateKeyPtr = (char *)(PRIVATE_KEY_FLASH_ADDR);
  if (privateKeyPtr == NULL) {
    Serial.println("Private key is NULL");
    return (unsigned char *)emptyString;
  }
  if (privateKeyPtr[0] == 0) {
    Serial.println("Private key is empty");
    return (unsigned char *)emptyString;
  }
  return (unsigned char *)privateKeyPtr;
}

/// @brief Get the device certificate from user data partition
/// @return The device certificate as a string
unsigned char *NonvolatileDataManager::getCertificate() {
  char *certificatePtr = (char *)(CERT_FLASH_ADDR);
  if (certificatePtr == NULL) {
    Serial.println("Certificate is NULL");
    return (unsigned char *)emptyString;
  }
  if (certificatePtr[0] == 0) {
    Serial.println("Certificate is empty");
    return (unsigned char *)emptyString;
  }
  return (unsigned char *)certificatePtr;
}

/// @brief Get the firmware signing public key
/// @return The firmware signing public key as a string
unsigned char *NonvolatileDataManager::getFirmwareSignPubKey() {
  return (unsigned char *)firmwareSignPubKey;
}

// by default it uses #define FLASH_MEMORY_APP_BASE 0x00100000
// but it can be updated with void begin(unsigned int _base_address, unsigned
// int _buf_size); let's keep the _buf_size to be FLASH_SECTOR_SIZE 0x1000 as
// the code does not support multiple sectors well but we can create multiple
// instances of FlashMemory with different base addresses

// in AN0400 for 2MB layout (default)
// User Data 0x0810_0000 0x0810_1FFF 8K  Reserved for user data
// User Data 0x0810_2000 0x0810_4FFF 12K BT FTL physical map
// User Data 0x0810_5000 0x0810_5FFF 4K  WIFI Fast Connect profile

// in datasheet for 4MB layout (since we are using binary modified bootloader
// may be we do not need to follow this?) or maybe we should, as the a 2MB IMG1
// will overwrite the user space in the 4MB layout User Data 0x0820_0000
// 0x0820_1FFF 8K  Reserved for user data User Data 0x0820_2000 0x0820_4FFF 12K
// BT FTL physical map User Data 0x0820_5000 0x0820_5FFF 4K  WIFI Fast Connect
// profile

// On 20250516, I think the booloader does not care the user data layout,
// it will just use the OTA_Region to check signature and the start address
// LS_IMG2_OTA2_ADDR seems only used by void sys_clear_ota_signature(void) from
// rtl8721d_ota.h FTL_PHY_PAGE_START_ADDR only used in void BLEDevice::init()
// FAST_RECONNECT_DATA seems not used as CONFIG_EXAMPLE_WLAN_FAST_CONNECT is 0
// we are using the user data, and BT setting for WIFI, let's do a dump.
// when do the dump, sketch takes 979708, and 0x08006000 + 0xEF2FC = 0x80F52FC
// 0x0810_0000 - 0x0810_00A1 not 0xFF, 162 bytes =
// storedSsid[33]+storedPassword[129] 0x0810_2000 - 0x0810_2003 not 0xFF, 4
// bytes, seems used by the BT FTL let wait till the the firmware grow to
// 1024000 (0xFA000) to see if the firmware collides with the user data

//#include <FlashMemory.h>

/// @brief Initialize the NonvolatileDataManager
void NonvolatileDataManager::initialize() {
  // FlashMemory.begin(0x00200000, FLASH_SECTOR_SIZE);   //for 4MB layout
  storedSsid = emptyString;
  storedPassword = emptyString;
}

/// @brief Get the SSID and password from user data partition
/// @param ssid Pointer to store the SSID string
/// @param password Pointer to store the password string
void NonvolatileDataManager::getSsidPasswordFromFlash(char **ssid,
                                                      char **password) {

  // FlashMemory.read();
  int lengthSSID = 33;
  int lengthPassword = 129;
  storedSsid = (char *)0x08200000;
  storedPassword = (char *)(0x08200000 + lengthSSID);

  // check if there is a valid string ending in ssid and password
  bool ssidValidEnding = false;
  for (int i = 0; i < lengthSSID; i++) {
    if (storedSsid[i] == 0) {
      ssidValidEnding = true;
      break;
    }
  }
  if (ssidValidEnding == false) {
    storedSsid = emptyString; // if no valid ending, set to empty string
  }

  bool passwordValidEnding = false;
  for (int i = 0; i < lengthPassword; i++) {
    if (storedPassword[i] == 0) {
      passwordValidEnding = true;
      break;
    }
  }
  if (passwordValidEnding == false) {
    storedPassword = emptyString; // if no valid ending, set to empty string
  }

  *ssid = storedSsid;
  *password = storedPassword;
}

/// @brief Save the SSID and password to user data partition
/// @param ssid The SSID string to save
/// @param password The password string to save
/// @note If the SSID and password are the same as the stored ones, it will
/// not save to flash to avoid unnecessary writes.
void NonvolatileDataManager::saveSsidPasswordToFlash(char *ssid,
                                                     char *password) {
  bool ssidAndPasswordTheSame = true;
  if (strcmp(ssid, storedSsid) != 0) {
    ssidAndPasswordTheSame = false;
  }
  if (strcmp(password, storedPassword) != 0) {
    ssidAndPasswordTheSame = false;
  }

  if (ssidAndPasswordTheSame) {
    Serial.println("SSID and password are the same, not saving to flash.");
    return;
  } else {
    Serial.flush();
    delay(100); // no delay will cause crash, maybe out of memory
    int writeLocation = 0x08200000 & 0xFFFFFF;
    flash_erase_sector(NULL, writeLocation);
    Serial.println("Saving SSID and password to flash.");
    uint8_t writeBuf[33 + 129];
    memcpy(writeBuf, ssid, 33);
    memcpy(writeBuf + 33, password, 129);
    flash_stream_write(NULL, writeLocation, 33 + 129, writeBuf);
    Serial.println("Saved SSID and password to flash.");
    storedSsid = (char *)0x08200000;
    storedPassword = (char *)(0x08200000 + 33);
  }
}

/// @brief Erase the SSID and password from user data partition
void NonvolatileDataManager::eraseSsidPasswordFromFlash() {
  // FlashMemory.erase();
  int eraseLocation = 0x08200000 & 0xFFFFFF;
  Serial.print("Erasing flash page at address: ");
  Serial.println(eraseLocation, HEX);
  flash_erase_sector(NULL, eraseLocation);

  storedSsid = emptyString;
  storedPassword = emptyString;
}

/// @brief Print a memory dump from the specified address and length for debugging
void NonvolatileDataManager::printMemoryDump(unsigned int startAddress,
                                             unsigned int length) {
  Serial.print("Memory dump from address: 0x");
  Serial.print(startAddress, HEX);
  Serial.print(" length: 0x");
  Serial.println(length, HEX);
  for (int i = 0; i < (int)length; i += 16) {
    uint8_t buf[16];
    uint8_t all0xFF = 1;
    for (int j = 0; j < 16; j++) {
      uint8_t *p = (uint8_t *)(startAddress + i + j);
      uint8_t data = *p;
      if (data != 0xFF) {
        all0xFF = 0;
      }
      buf[j] = data;
    }
    if (!all0xFF) {
      Serial.print("0x");
      Serial.print(startAddress + i, HEX);
      Serial.print(": ");
      for (int j = 0; j < 16; j++) {
        Serial.print("0x");
        if (buf[j] < 0x10) {
          Serial.print("0");
        }
        Serial.print(buf[j], HEX);
        Serial.print(" ");
      }
      Serial.println();
    }
  }
}

/// @brief Get the Thing Name from user data partition
/// @return The Thing Name as a string
char *NonvolatileDataManager::getThingName() {
  char *thingNamePtr = (char *)(THING_NAME_FLASH_ADDR);
  if (thingNamePtr == NULL) {
    Serial.println("Thing name is NULL");
    return emptyString;
  }
  if (thingNamePtr[0] == 0) {
    Serial.println("Thing name is empty");
    return emptyString;
  }
  return thingNamePtr;
}
/// @brief  Get the Client ID from user data partition
/// @return The Client ID as a string
char *NonvolatileDataManager::getClientId() {
  char *clientIdPtr = (char *)(CLIENT_ID_FLASH_ADDR);
  if (clientIdPtr == NULL) {
    Serial.println("Client ID is NULL");
    return emptyString;
  }
  if (clientIdPtr[0] == 0) {
    Serial.println("Client ID is empty");
    return emptyString;
  }
  return clientIdPtr;
}
