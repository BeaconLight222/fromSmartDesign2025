#ifndef OTA_PROCESSOR_H
#define OTA_PROCESSOR_H

#include <Arduino.h>

#include "mbedtls/base64.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include "mbedtls/x509_crt.h"

class OtaProcessor {
public:
  static int checkFlashSize();
  static int checkImg2AddressInBootloader();
  static int getCurrentImgNumber();

  int initCodeSigningCert(char *cert);
  int OtaProcessInit();
  int OtaProcessDataChunkBase64(char *dataBase64, size_t length, size_t offset);
  int OtaProcessDataChunk(uint8_t *data, size_t length, size_t offset);
  int OtaWriteLeftoverData();
  int OtaProcessVerifySignature(char *signatureBase64);
  int OtaEnableOTA();

  mbedtls_sha256_context sha256ctx;
  mbedtls_x509_crt codeSigningCert;

  uint8_t *pageSwapData;

  int erasedFlashAddress;

  // def process_OTA_initHash():
  //     global otaDigest
  //     otaDigest = hashes.Hash(hashes.SHA256())

  // def process_OTA_datachunk(data_chunk, length, offset):
  //     global otaDigest
  //     # Process the data chunk here
  //     # For example, write it to a file or perform some operation
  //     print(f"Processing OTA data chunk of length {length} at offset
  //     {offset}") print(f"Data chunk: {data_chunk}")
  //     otaDigest.update(data_chunk)

  // def process_OTA_verify_signature(signatureBase64):
  // OtaProcessor();
  // void begin();
  // void handleOTA();
  // void setOTAUrl(const char* url);
  // void setOTASignature(const char* signature);
  // bool verifyOTASignature(const uint8_t* data, size_t length, const char*
  // signature); void processOTADataChunk(const uint8_t* data, size_t length);
  // void finalizeOTA();

private:
  const char *otaUrl;
  const char *otaSignature;
};

#endif