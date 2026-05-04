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


const uint8_t codeSigningEcdsaContextData[172] = {
0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x78, 0xDB, 0x1C, 0x10, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 
0x08, 0x00, 0x00, 0x00, 0x98, 0xDB, 0x1C, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 
0xD8, 0xDB, 0x1C, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xF8, 0xDB, 0x1C, 0x10, 
0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xE4, 0xDA, 0x1C, 0x10, 0x01, 0x00, 0x00, 0x00, 
0x08, 0x00, 0x00, 0x00, 0xB8, 0xDB, 0x1C, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 
0x01, 0x00, 0x00, 0x00, 0x33, 0x7C, 0x12, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x20, 0x3A, 0x01, 0x10, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 
0x60, 0x38, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xA0, 0x38, 0x01, 0x10, 
0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xE0, 0x38, 0x01, 0x10};

int OtaProcessor::checkFlashSize(){
    uint8_t flashSizeInfo[3];
    flash_read_id(NULL, flashSizeInfo, 3);
    
    int flashSize = 0;
    if (flashSizeInfo[2] == 0x16) {
        flashSize = 4*1024*1024;
    } else if (flashSizeInfo[2] == 0x15) {
        flashSize = 2*1024*1024;
    } else {
        Serial.println("Unknown flash size");
        Serial.print("Flash ID: ");
        for (int i = 0; i < 3; i++) {
            Serial.print(flashSizeInfo[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
    //BW16 4M variants has 20 40 16
    //BW16 2M variants has 68 40 15

    return flashSize;
}

int OtaProcessor::checkImg2AddressInBootloader(){
    uint32_t *originalArduino_3_1_7_bootloaderOtaRegionAddr = (uint32_t*)0x0800111c;
    if (originalArduino_3_1_7_bootloaderOtaRegionAddr[0] == 0x8006000) {
       uint32_t img2AddressReaded = originalArduino_3_1_7_bootloaderOtaRegionAddr[1];
       if ((img2AddressReaded&0x8000000) == 0x8000000) {
            img2Address = (int)img2AddressReaded;
            return (int)img2AddressReaded;
       }
    }

    // the OTA_region is not in the 0x0800111c, let's do a search
    for (uint32_t *searchAddr = (uint32_t*)0x08000000; (int)searchAddr < (int) 0x08002000; searchAddr++) {
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

int OtaProcessor::getCurrentImgNumber(){
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
        if ((int)PhyAddr < img2Address) {
            currentImageNumber = 1;
        } else {
            currentImageNumber = 2;
        }
    }

    return currentImageNumber;
}

int OtaProcessor::initCodeSigningCert(char* cert){
    mbedtls_x509_crt_free(&codeSigningCert); // Free any existing certificate
    mbedtls_x509_crt_init(&codeSigningCert);
    int ret = mbedtls_x509_crt_parse(&codeSigningCert, (const unsigned char*)cert, strlen(cert) + 1);
    if (ret != 0) {
        Serial.println("ERROR: mbedtls x509 crt parse failed!");
        return 0;
    }
    return 1;
}

int OtaProcessor::OtaProcessInit(){
    mbedtls_sha256_init( &sha256ctx );  //does not use malloc
    mbedtls_sha256_starts( &sha256ctx, 0 );
    return 0;
}
int OtaProcessor::OtaProcessDataChunk(uint8_t* data, size_t length, size_t offset){
    // Process the data chunk here

    if (img2Address == 0) {
        Serial.println("img2Address is not set, please call checkImg2AddressInBootloader() first");
        return 0;
    }
    int allowedFlashOtaSize = 0x00100000-0x6000-1; // 2MB flash size
    if (img2Address > 0x08200000) {
        allowedFlashOtaSize = 0x00200000-0x6000-1; // 4MB flash size
    }

    if ((int)(offset) < 0){
        Serial.println("Offset is negative");
        return 0;
    }

    if ((int)(offset + length) > allowedFlashOtaSize){
        Serial.println("Offset is too large");
        return 0;
    }


    mbedtls_sha256_update( &sha256ctx, data, length );
    return 1;
}

int OtaProcessor::OtaProcessDataChunkBase64(char* dataBase64, size_t length, size_t offset){
    // Serial.print("OtaProcessDataChunkBase64: ");
    // Serial.print("length: ");
    // Serial.print(length);
    // Serial.print(" offset: ");
    // Serial.println(offset);
    // Serial.println("Base64 ptr:");
    // Serial.println((int)dataBase64);
    // Serial.println("Base64 data:");
    // Serial.println(dataBase64);

    if ( length>2048 ){
        Serial.println("Base64 data length too long");
        return 0;
    }
    // Decode the base64 data chunk
    unsigned char decodedData[2048]; //safe size for decoded data, usually 2048 bytes
    int decodedLength = 0;
    int base64DecodeResult = mbedtls_base64_decode(decodedData, sizeof(decodedData), (size_t*)&decodedLength, (const unsigned char*)dataBase64, strlen(dataBase64));
    if (base64DecodeResult != 0) {
        Serial.println("Base64 decode failed");
        return 0;
    }

    // Serial.print("Decoded Data length:");
    // Serial.println(decodedLength);

    // Process the decoded data chunk
    return OtaProcessDataChunk(decodedData, decodedLength, offset);
}

int OtaProcessor::OtaProcessVerifySignature(char* signatureBase64){
    // Verify the signature here
    // For example, decode the base64 signature and compare it with the hash
    // You can use mbedtls functions for base64 decoding and signature verification

    unsigned char output[32];
    mbedtls_sha256_finish( &sha256ctx, output );
    mbedtls_sha256_free( &sha256ctx ); //does not use free

    Serial.println("SHA256 Hash:");
    for (int i = 0; i < (int)sizeof(output); i++) {
        Serial.print(output[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    uint8_t signatureDecoded[80];   //safe size for decoded signature, usually 72 bytes
    int decodedSignatureLength = 0;
    int base64DecodeResult = mbedtls_base64_decode(signatureDecoded, sizeof(signatureDecoded), (size_t*)&decodedSignatureLength, (const unsigned char*)signatureBase64, strlen(signatureBase64));
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

    //mbedtls_pk_verify and all other funtions are in ROM, can not debug
    //can not check the pointer.....
    //guess it is mbedtls_ecdsa_verify?
    mbedtls_ecdsa_context *context = (mbedtls_ecdsa_context *)codeSigningCert.pk.pk_ctx;
    int retTest = mbedtls_ecdsa_read_signature(
        context,                        // ECP group from the public key
        output,                    // Hash output
        sizeof(output),            // Length of the hash
        signatureDecoded,          // Decoded signature
        decodedSignatureLength      // Length of the decoded signature
    );
    Serial.print("mbedtls_ecdsa_read_signature returned: ");
    Serial.println(retTest);
    // OK it is mbedtls_ecdsa_read_signature
    // ctx->grp and ctx->Q are used
    mbedtls_ecp_group grp = context->grp;
    mbedtls_ecp_point Q = context->Q;
    mbedtls_ecdsa_context newEcdsaContext;
    mbedtls_ecdsa_init(&newEcdsaContext); // Initialize a new context
    newEcdsaContext.grp = grp; // Copy the group
    newEcdsaContext.Q = Q;     // Copy the point
    int sizeOfmbedtls_ecdsa_context = sizeof(mbedtls_ecdsa_context);
    //print the content of newEcdsaContext as a c array
    Serial.print("const uint8_t codeSigningEcdsaContextData[");
    Serial.print(sizeOfmbedtls_ecdsa_context);
    Serial.println("] = {");
    for (int i = 0; i < sizeOfmbedtls_ecdsa_context; i++) {
        uint8_t data = ((uint8_t*)&newEcdsaContext)[i];
        Serial.print("0x");
        if (data < 0x10) {
            Serial.print("0");
        }
        Serial.print(data, HEX);
        if (i < sizeOfmbedtls_ecdsa_context - 1) {
            Serial.print(", ");
        }
        if ((i + 1) % 16 == 0) {
            Serial.println();
        }
    }
    Serial.println("};");

    retTest = mbedtls_ecdsa_read_signature(
        &newEcdsaContext,                // ECP group from the public key
        output,                    // Hash output
        sizeof(output),            // Length of the hash
        signatureDecoded,          // Decoded signature
        decodedSignatureLength      // Length of the decoded signature
    );
    Serial.print("mbedtls_ecdsa_read_signature with new context returned: ");
    Serial.println(retTest);

    //try again with codeSigningEcdsaContextData
    retTest = mbedtls_ecdsa_read_signature(
        (mbedtls_ecdsa_context *)codeSigningEcdsaContextData, // ECP group from the public key
        output,                    // Hash output
        sizeof(output),            // Length of the hash
        signatureDecoded,          // Decoded signature
        decodedSignatureLength      // Length of the decoded signature
    );
    Serial.print("mbedtls_ecdsa_read_signature with codeSigningEcdsaContextData returned: ");
    Serial.println(retTest);



    // int mbedtls_pk_verify( mbedtls_pk_context *ctx, mbedtls_md_type_t md_alg,
    //             const unsigned char *hash, size_t hash_len,
    //             const unsigned char *sig, size_t sig_len )
    // {
    //     if( ctx == NULL || ctx->pk_info == NULL ||
    //         pk_hashlen_helper( md_alg, &hash_len ) != 0 )
    //         return( MBEDTLS_ERR_PK_BAD_INPUT_DATA );

    //     if( ctx->pk_info->verify_func == NULL )
    //         return( MBEDTLS_ERR_PK_TYPE_MISMATCH );

    //     return( ctx->pk_info->verify_func( ctx->pk_ctx, md_alg, hash, hash_len,
    //                                     sig, sig_len ) );
    // }

    int verify_ret = mbedtls_pk_verify(
        &codeSigningCert.pk,                // Public key from certificate
        MBEDTLS_MD_SHA256,                  // Hash algorithm
        output, sizeof(output),             // Hash and its length
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


