/// @file OtaProcessor.cpp
/// @brief OTA (Over-The-Air) update processor for the BW16 project.

#include "OtaProcessor.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "flash_api.h"
#ifdef __cplusplus
}
#endif

static int img2Address = 0;
static int currentImageNumber = 0;

/// @brief Check the flash size from the JEDEC ID
/// @return The flash size in bytes, or 0 if unknown
int OtaProcessor::checkFlashSize() {
  uint8_t flashSizeInfo[3];
  flash_read_id(NULL, flashSizeInfo, 3);

  int flashSize = 0;
  if (flashSizeInfo[2] == 0x16) {
    flashSize = 4 * 1024 * 1024;
  } else if (flashSizeInfo[2] == 0x15) {
    flashSize = 2 * 1024 * 1024;
  } else {
    Serial.println("Unknown flash size");
    Serial.print("Flash ID: ");
    for (int i = 0; i < 3; i++) {
      Serial.print(flashSizeInfo[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
  // BW16 4M variants has 20 40 16
  // BW16 2M variants has 68 40 15

  return flashSize;
}

/// @brief Check the image 2 address in the bootloader with a binary check or search
/// @return The image 2 address if found, 0 otherwise
int OtaProcessor::checkImg2AddressInBootloader() {
  uint32_t *originalArduino_3_1_7_bootloaderOtaRegionAddr =
      (uint32_t *)0x0800111c;
  if (originalArduino_3_1_7_bootloaderOtaRegionAddr[0] == 0x8006000) {
    uint32_t img2AddressReaded =
        originalArduino_3_1_7_bootloaderOtaRegionAddr[1];
    if ((img2AddressReaded & 0x8000000) == 0x8000000) {
      img2Address = (int)img2AddressReaded;
      return (int)img2AddressReaded;
    }
  }

  // the OTA_region is not in the 0x0800111c, let's do a search
  for (uint32_t *searchAddr = (uint32_t *)0x08000000;
       (int)searchAddr < (int)0x08002000; searchAddr++) {
    if (*searchAddr == 0x8006000) {
      uint32_t img2AddressReaded = *(searchAddr + 1);
      if ((img2AddressReaded & 0x8000000) == 0x8000000) {
        img2Address = (int)img2AddressReaded;
        return (int)img2AddressReaded;
      }
    }
  }
  return 0;
}

/// @brief Get the current image number based on the flash memory address
/// @return The current image number (1 or 2)
int OtaProcessor::getCurrentImgNumber() {
  uint32_t AddrStart, Offset, IsMinus, PhyAddr;

  RSIP_REG_TypeDef *RSIP = ((RSIP_REG_TypeDef *)RSIP_REG_BASE);
  u32 CtrlTemp = RSIP->FLASH_MMU[0].MMU_ENTRYx_CTRL;

  if (CtrlTemp & MMU_BIT_ENTRY_VALID) {
    AddrStart = RSIP->FLASH_MMU[0].MMU_ENTRYx_STRADDR;
    Offset = RSIP->FLASH_MMU[0].MMU_ENTRYx_OFFSET;
    IsMinus = CtrlTemp & MMU_BIT_ENTRY_OFFSET_MINUS;

    if (IsMinus)
      PhyAddr = AddrStart - Offset;
    else
      PhyAddr = AddrStart + Offset;
  }

  if (PhyAddr >= (uint32_t)0x8006000) {
    if (PhyAddr < (uint32_t)img2Address) {
      currentImageNumber = 1;
    } else {
      currentImageNumber = 2;
    }
  }

  return currentImageNumber;
}

/// @brief Initialize the code signing certificate
/// @param cert The certificate string
/// @return 1 on success, 0 on failure
int OtaProcessor::initCodeSigningCert(char *cert) {
  mbedtls_x509_crt_free(&codeSigningCert); // Free any existing certificate
  mbedtls_x509_crt_init(&codeSigningCert);
  int ret = mbedtls_x509_crt_parse(
      &codeSigningCert, (const unsigned char *)cert, strlen(cert) + 1);
  if (ret != 0) {
    Serial.println("ERROR: mbedtls x509 crt parse failed!");
    return 0;
  }
  return 1;
}

/// @brief Initialize the OTA process, including SHA256 context and malloc for page swap data
int OtaProcessor::OtaProcessInit() {
  mbedtls_sha256_init(&sha256ctx); // does not use malloc
  mbedtls_sha256_starts(&sha256ctx, 0);
  if (pageSwapData == NULL) {
    pageSwapData = (uint8_t *)malloc(4096); // Allocate memory for pageSwapData
    if (pageSwapData == NULL) {
      Serial.println("ERROR: Failed to allocate memory for pageSwapData");
      return 0;
    }
  }
  return 0;
}

/// @brief At the end of the OTA process, enable the written firmware image by changing the first 8 bytes of the image to "81958711" \n
/// next time the device boots, it will boot into the new image
/// @return 1 on success, 0 on failure
int OtaProcessor::OtaEnableOTA() {
  // Enable the OTA process here
  // For example, set a flag or perform some operation
  if (img2Address == 0) {
    Serial.println("img2Address is not set, please call "
                   "checkImg2AddressInBootloader() first");
    return 0;
  }

  int baseAddressImg1 = 0x08006000;
  if (currentImageNumber == 1) {
    // image 1
    // enable image 2, erase image 1
    // read img2Address page into pageSwapData
    flash_stream_read(NULL, img2Address & 0xFFFFFF, 4096, pageSwapData);
    memcpy(pageSwapData, "81958711", 8); // remove the first 4 bytes
    // erase img2Address page
    flash_erase_sector(NULL, img2Address & 0xFFFFFF);
    // write pageSwapData to img2Address page
    flash_stream_write(NULL, img2Address & 0xFFFFFF, 4096, pageSwapData);
    // erase image 1
    flash_stream_read(NULL, baseAddressImg1 & 0xFFFFFF, 4096, pageSwapData);
    memset(pageSwapData, 0, 8);
    flash_erase_sector(NULL, baseAddressImg1 & 0xFFFFFF);
    flash_stream_write(NULL, baseAddressImg1 & 0xFFFFFF, 4096, pageSwapData);
  } else if (currentImageNumber == 2) {
    // image 2
    // enable image 1, erase image 2
    // read baseAddressImg1 page into pageSwapData
    flash_stream_read(NULL, baseAddressImg1 & 0xFFFFFF, 4096, pageSwapData);
    memcpy(pageSwapData, "81958711", 8); // remove the first 4 bytes
    // erase baseAddressImg1 page
    flash_erase_sector(NULL, baseAddressImg1 & 0xFFFFFF);
    // write pageSwapData to baseAddressImg1 page
    flash_stream_write(NULL, baseAddressImg1 & 0xFFFFFF, 4096, pageSwapData);
    // erase image 2
    flash_stream_read(NULL, img2Address & 0xFFFFFF, 4096, pageSwapData);
    memset(pageSwapData, 0, 8);
    // erase image 2
    flash_erase_sector(NULL, img2Address & 0xFFFFFF);
    flash_stream_write(NULL, img2Address & 0xFFFFFF, 4096, pageSwapData);
  } else {
    Serial.println("currentImageNumber is not set, please call "
                   "getCurrentImgNumber() first");
    return 0;
  }

  Serial.println("Enabled OTA");
  return 1;
}

/// @brief Process a data chunk for OTA update, calculating SHA256 hash and writing to flash memory when full page is reached
/// @param data Pointer to the data chunk
/// @param length Length of the data chunk
/// @param offset Offset within the flash memory
/// @return 1 on success, 0 on failure
int OtaProcessor::OtaProcessDataChunk(uint8_t *data, size_t length,
                                      size_t offset) {
  // Process the data chunk here

  if (img2Address == 0) {
    Serial.println("img2Address is not set, please call "
                   "checkImg2AddressInBootloader() first");
    return 0;
  }
  int allowedFlashOtaSize = 0x00100000 - 0x6000 - 1; // 2MB flash size
  if (img2Address > 0x08200000) {
    allowedFlashOtaSize = 0x00200000 - 0x6000 - 1; // 4MB flash size
  }

  int baseAddress = 0x08006000;
  if (currentImageNumber == 1) {
    // image 1
    baseAddress = img2Address;
  } else if (currentImageNumber == 2) {
    // image 2
  } else {
    Serial.println("currentImageNumber is not set, please call "
                   "getCurrentImgNumber() first");
    return 0;
  }

  if ((int)(offset) < 0) {
    Serial.println("Offset is negative");
    return 0;
  }

  if ((int)(offset + length) > allowedFlashOtaSize) {
    Serial.println("Offset is too large");
    return 0;
  }

  mbedtls_sha256_update(&sha256ctx, data, length);

  int flashAddress = baseAddress + offset;
  // Serial.print("Writing to flash address: ");
  // Serial.println(flashAddress, HEX);

  if ((flashAddress & (4096 - 1)) == 0) {
    int eraseLocation = flashAddress & 0xFFFFFF;
    Serial.print("Erasing flash page at address: ");
    Serial.println(eraseLocation, HEX);
    flash_erase_sector(NULL, eraseLocation);
    erasedFlashAddress = eraseLocation;
    memset(pageSwapData, 0xFF, sizeof(pageSwapData));
  }

  if ((flashAddress & 0xFFF) + length > 4096) {
    Serial.println("Data chunk is too large for flash page");
    return 0;
  }

  int pageOffset = flashAddress & 0xFFF;
  memcpy(pageSwapData + pageOffset, data, length);
  if (offset == 0) {
    memset(pageSwapData, 0,
           8); // do not enable image until signature is verified
  }

  if (((flashAddress & 0xFFF) + length == 4096)) {
    int flashInAddress = erasedFlashAddress;
    Serial.print("Writing flash page at address: ");
    Serial.println(flashInAddress, HEX);
    flash_stream_write(NULL, flashInAddress, 4096, pageSwapData);
    memset(pageSwapData, 0xFF, sizeof(pageSwapData));
  }

  return 1;
}

/// @brief If there is leftover data in pageSwapData, write it to flash memory
/// @return 1 on success, 0 on failure
int OtaProcessor::OtaWriteLeftoverData() {

  // check if pageSwapData is all 0xFF
  int allFF = 1;
  for (int i = 0; i < (int)sizeof(pageSwapData); i++) {
    if (pageSwapData[i] != 0xFF) {
      allFF = 0;
      break;
    }
  }
  if (allFF) {
    Serial.println("No leftover data to write");
    return 0;
  }
  // Write the leftover data to flash

  int flashInAddress = erasedFlashAddress;
  Serial.print("Writing left over flash page at address: ");
  Serial.println(flashInAddress, HEX);
  flash_stream_write(NULL, flashInAddress, 4096, pageSwapData);
  memset(pageSwapData, 0xFF, sizeof(pageSwapData));

  return 1;
}

/// @brief convert a Base64 encoded data chunk from AWS to binary
int OtaProcessor::OtaProcessDataChunkBase64(char *dataBase64, size_t length,
                                            size_t offset) {
  // Serial.print("OtaProcessDataChunkBase64: ");
  // Serial.print("length: ");
  // Serial.print(length);
  // Serial.print(" offset: ");
  // Serial.println(offset);
  // Serial.println("Base64 ptr:");
  // Serial.println((int)dataBase64);
  // Serial.println("Base64 data:");
  // Serial.println(dataBase64);

  if (length > 2048) {
    Serial.println("Base64 data length too long");
    return 0;
  }
  // Decode the base64 data chunk
  unsigned char
      decodedData[2048]; // safe size for decoded data, usually 2048 bytes
  int decodedLength = 0;
  int base64DecodeResult = mbedtls_base64_decode(
      decodedData, sizeof(decodedData), (size_t *)&decodedLength,
      (const unsigned char *)dataBase64, strlen(dataBase64));
  if (base64DecodeResult != 0) {
    Serial.println("Base64 decode failed");
    return 0;
  }

  // Serial.print("Decoded Data length:");
  // Serial.println(decodedLength);

  // Process the decoded data chunk
  return OtaProcessDataChunk(decodedData, decodedLength, offset);
}

/// @brief Verify the signature of the OTA update using the code signing certificate
/// @param signatureBase64 The Base64 encoded signature string
int OtaProcessor::OtaProcessVerifySignature(char *signatureBase64) {
  // Verify the signature here
  // For example, decode the base64 signature and compare it with the hash
  // You can use mbedtls functions for base64 decoding and signature
  // verification

  unsigned char output[32];
  mbedtls_sha256_finish(&sha256ctx, output);
  mbedtls_sha256_free(&sha256ctx); // does not use free

  Serial.println("SHA256 Hash:");
  for (int i = 0; i < (int)sizeof(output); i++) {
    Serial.print(output[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  uint8_t
      signatureDecoded[80]; // safe size for decoded signature, usually 72 bytes
  int decodedSignatureLength = 0;
  int base64DecodeResult = mbedtls_base64_decode(
      signatureDecoded, sizeof(signatureDecoded),
      (size_t *)&decodedSignatureLength, (const unsigned char *)signatureBase64,
      strlen(signatureBase64));
  if (base64DecodeResult != 0) {
    Serial.println("Base64 decode failed");
    return -1;
  }

  // Serial.println("Decoded Signature:");
  // for (int i = 0; i < decodedSignatureLength; i++) {
  //     Serial.print(signatureDecoded[i], HEX);
  //     Serial.print(" ");
  // }
  // Serial.println();

  int verify_ret = mbedtls_pk_verify(
      &codeSigningCert.pk,                     // Public key from certificate
      MBEDTLS_MD_SHA256,                       // Hash algorithm
      output, sizeof(output),                  // Hash and its length
      signatureDecoded, decodedSignatureLength // Signature and its length
  );

  if (verify_ret != 0) {
    Serial.print("Signature verification failed, mbedtls_pk_verify returned: ");
    Serial.println(verify_ret);
    return -2;
  } else {
    Serial.println("Signature verification succeeded!");
  }

  return 0;
}
