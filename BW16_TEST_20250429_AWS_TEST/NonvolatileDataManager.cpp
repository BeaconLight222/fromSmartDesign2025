#include "NonvolatileDataManager.h"

// by default, the user data region is 0x2000 big, that is 8192 bytes
// Amazon Root CA 1188 bytes
// privateKey 1679
// certificate 1224
// OTA signature key size = 566
// total = 1188 + 1679 + 1224 + 566 = 3857 we should be fine
// if we take out the OTA signature key and the Amazon Root CA
// that would be 1679 + 1224 = 2903, that can easily fit in one page (4096, 0x1000)

#define CLIENT_ID_FLASH_ADDR 0x8201000
#define THING_NAME_FLASH_ADDR 0x8201040
#define CERT_FLASH_ADDR 0x8201080
#define PRIVATE_KEY_FLASH_ADDR 0x820165c

char *emptyString = "";

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

  char* firmwareSignPubKey =
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



unsigned char cacert_subject_country_oid[3];
unsigned char cacert_subject_country_value[3];
unsigned char cacert_subject_organization_oid[3];
unsigned char cacert_subject_organization_value[6];
unsigned char cacert_subject_common_name_oid[3];
unsigned char cacert_subject_common_name_value[16];

const unsigned char cacert_subject_country_oid_data[3] = {0x55, 0x04, 0x06};
const unsigned char cacert_subject_country_value_data[] = {"US"};
const unsigned char cacert_subject_organization_oid_data[3] = {0x55, 0x04, 0x0A};
const unsigned char cacert_subject_organization_value_data[] = {"Amazon"};
const unsigned char cacert_subject_common_name_oid_data[3] = {0x55, 0x04, 0x03};
const unsigned char cacert_subject_common_name_value_data[] = {"Amazon Root CA 1"};

struct mbedtls_asn1_named_data cacert_subject_common_name;
struct mbedtls_asn1_named_data cacert_subject_organization;

//if it is const, we get hard fault...
const struct mbedtls_asn1_named_data cacert_subject_common_name_data = {
    .oid = {
        .tag = MBEDTLS_ASN1_OID,
        .len = 3,
        .p = (unsigned char *)cacert_subject_common_name_oid
    },
    .val = {
        .tag = MBEDTLS_ASN1_PRINTABLE_STRING,
        .len = 16,
        .p = (unsigned char *)cacert_subject_common_name_value
    },
    .next = NULL,
    .next_merged = 0
};
const struct mbedtls_asn1_named_data cacert_subject_organization_data = {
    .oid = {
        .tag = MBEDTLS_ASN1_OID,
        .len = 3,
        .p = (unsigned char *)cacert_subject_organization_oid
    },
    .val = {
        .tag = MBEDTLS_ASN1_PRINTABLE_STRING,
        .len = 6,
        .p = (unsigned char *)cacert_subject_organization_value
    },
    .next = &cacert_subject_common_name,
    .next_merged = 0
};

extern const mbedtls_pk_info_t mbedtls_rsa_info;

uint32_t cacert_N_p[64];
const uint32_t cacert_N_p_data[64] = {
0x9AC8AA0D, 0x8E04AE6C, 0x3E3B2426, 0x80C4804C, 0x40207F20, 0x8723D63F, 0x67C80F9C, 0x54E177E0, 0x1BC5C71E, 0xDBFB2D62, 0xAE50386F, 0x02CB388A, 0x69FFEA95, 0xF7715C5E, 0x240E2D4B, 0xCAB4F29F, 
0xFAB71DAA, 0x4628B843, 0x8717F232, 0x596EBB73, 0x24F79EC4, 0x7035D788, 0x2D7D2D74, 0xFFD1E830, 0x91F948DC, 0x54EBA3C3, 0xA14566BA, 0xC0308666, 0x6F89A351, 0xEE94785E, 0x48673096, 0xBAD5A9F9, 
0x873958F8, 0x3807DCA2, 0x47198F50, 0xA6F9AFEC, 0x7AD080B2, 0x2050066C, 0x392FFAB0, 0xDA9C9B35, 0xB416FA74, 0x45E1F9FD, 0xE54A56E4, 0xFE8236A5, 0x1D619EA4, 0xD7B97638, 0x4968B0DE, 0xF5B519F8, 
0xC73EEB37, 0xF4EE8823, 0x0717364C, 0x8DDB48C8, 0x36BA01BE, 0x4EB2A4D0, 0xD3A0437A, 0xAC022D86, 0x7484012F, 0x582160F9, 0xF49968F7, 0xD8D78876, 0x50747D6E, 0x71AF4780, 0xCA78D5E3, 0xB2788071
};
uint32_t cacert_E_p[1];
const uint32_t cacert_E_p_data[1] = {0x10001};

mbedtls_rsa_context cacert_rsa;
const mbedtls_rsa_context cacert_rsa_data = {
    .ver = 0,
    .len = 256,
    .N = { .s=1, .n=64,.p = cacert_N_p },
    .E = { .s=1, .n=1, .p = cacert_E_p },
    .D = { .s=0, .n=0, .p = NULL },
    .P = { .s=0, .n=0, .p = NULL },
    .Q = { .s=0, .n=0, .p = NULL },
    .DP = { .s=0, .n=0, .p = NULL },
    .DQ = { .s=0, .n=0, .p = NULL },
    .QP = { .s=0, .n=0, .p = NULL },
    .RN = { .s=0, .n=0, .p = NULL },
    .RP = { .s=0, .n=0, .p = NULL },
    .RQ = { .s=0, .n=0, .p = NULL },
    .Vi = { .s=0, .n=0, .p = NULL },
    .Vf = { .s=0, .n=0, .p = NULL },
    .padding = MBEDTLS_RSA_PKCS_V15,    //0
    .hash_id = MBEDTLS_MD_NONE, //0
};

mbedtls_x509_crt cacert_pre_parsed;

unsigned char cli_crt_raw[862];
const unsigned char cli_crt_raw_data[862] = {
0x30, 0x82, 0x03, 0x5A, 0x30, 0x82, 0x02, 0x42, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x15, 0x00, 
0xFF, 0x8B, 0x44, 0x31, 0x1D, 0xD1, 0xE7, 0x53, 0x0F, 0x08, 0x31, 0x27, 0x67, 0x9D, 0x7A, 0x60, 
0x91, 0xD0, 0xDB, 0xA4, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 
0x0B, 0x05, 0x00, 0x30, 0x4D, 0x31, 0x4B, 0x30, 0x49, 0x06, 0x03, 0x55, 0x04, 0x0B, 0x0C, 0x42, 
0x41, 0x6D, 0x61, 0x7A, 0x6F, 0x6E, 0x20, 0x57, 0x65, 0x62, 0x20, 0x53, 0x65, 0x72, 0x76, 0x69, 
0x63, 0x65, 0x73, 0x20, 0x4F, 0x3D, 0x41, 0x6D, 0x61, 0x7A, 0x6F, 0x6E, 0x2E, 0x63, 0x6F, 0x6D, 
0x20, 0x49, 0x6E, 0x63, 0x2E, 0x20, 0x4C, 0x3D, 0x53, 0x65, 0x61, 0x74, 0x74, 0x6C, 0x65, 0x20, 
0x53, 0x54, 0x3D, 0x57, 0x61, 0x73, 0x68, 0x69, 0x6E, 0x67, 0x74, 0x6F, 0x6E, 0x20, 0x43, 0x3D, 
0x55, 0x53, 0x30, 0x1E, 0x17, 0x0D, 0x32, 0x35, 0x30, 0x34, 0x31, 0x35, 0x31, 0x36, 0x32, 0x33, 
0x31, 0x33, 0x5A, 0x17, 0x0D, 0x34, 0x39, 0x31, 0x32, 0x33, 0x31, 0x32, 0x33, 0x35, 0x39, 0x35, 
0x39, 0x5A, 0x30, 0x1E, 0x31, 0x1C, 0x30, 0x1A, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x13, 0x41, 
0x57, 0x53, 0x20, 0x49, 0x6F, 0x54, 0x20, 0x43, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69, 0x63, 0x61, 
0x74, 0x65, 0x30, 0x82, 0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 
0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00, 0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 
0x01, 0x01, 0x00, 0xD5, 0xD5, 0x7D, 0x85, 0x63, 0xF0, 0xC3, 0x6F, 0xB4, 0x57, 0xDA, 0xD6, 0xEF, 
0xD2, 0xDD, 0xAE, 0x13, 0x16, 0x39, 0x6F, 0x56, 0x04, 0xD3, 0x64, 0x3B, 0x92, 0x74, 0x7C, 0x13, 
0x64, 0x58, 0x63, 0xE8, 0x0B, 0xC0, 0x61, 0x72, 0x7A, 0x5B, 0xA2, 0x54, 0x13, 0x25, 0xFF, 0x1C, 
0x5E, 0x2C, 0x71, 0x99, 0x9A, 0x9B, 0x17, 0x01, 0x72, 0x20, 0xE2, 0x1D, 0xE3, 0x99, 0x0A, 0x98, 
0x29, 0xE1, 0x89, 0x72, 0xF9, 0xDC, 0x02, 0xFB, 0xED, 0xC8, 0x95, 0x2F, 0x10, 0xB4, 0x02, 0xC7, 
0xB4, 0x52, 0xA8, 0xF4, 0x14, 0xD5, 0xDA, 0xF7, 0x8E, 0xCE, 0x1F, 0x40, 0x0B, 0x69, 0xA9, 0x97, 
0x64, 0xE4, 0x4D, 0xEC, 0x41, 0x66, 0xF9, 0xD6, 0x67, 0x1A, 0x36, 0x48, 0x7C, 0xD8, 0xE0, 0x0D, 
0x29, 0x7B, 0xF1, 0xC3, 0xDD, 0xB4, 0x57, 0x1A, 0x0F, 0x3C, 0x0C, 0xFB, 0x9E, 0x4C, 0xD6, 0x32, 
0x27, 0xEF, 0x15, 0x6C, 0x18, 0x93, 0xF4, 0x60, 0xB4, 0x17, 0xD3, 0xFB, 0x83, 0xE1, 0xFE, 0x66, 
0x69, 0x91, 0x40, 0xA2, 0x72, 0xED, 0x59, 0xD8, 0xFC, 0xEE, 0x7E, 0x70, 0x98, 0x63, 0x33, 0xAE, 
0xF0, 0x38, 0xB5, 0x87, 0xE3, 0xE0, 0x2D, 0x41, 0x50, 0x8D, 0xF3, 0x93, 0xE0, 0xD4, 0xD9, 0x6A, 
0xA2, 0x7C, 0xFC, 0x4A, 0xF3, 0x81, 0x52, 0x63, 0x82, 0x03, 0x10, 0xC2, 0x8E, 0x0E, 0xF6, 0x17, 
0x2D, 0x55, 0xCB, 0xD2, 0x1F, 0x77, 0x2A, 0x6D, 0xD5, 0x3A, 0x90, 0x34, 0x50, 0xC1, 0x52, 0xAE, 
0x3B, 0x1A, 0x95, 0x30, 0x49, 0xCF, 0x8A, 0x34, 0x01, 0x11, 0x4C, 0xF4, 0x8A, 0x0C, 0x6F, 0x5A, 
0xD8, 0x3E, 0xD0, 0xD7, 0x2A, 0x2C, 0x0C, 0x1C, 0x9F, 0x69, 0xE1, 0x1E, 0x48, 0x30, 0xD5, 0x4C, 
0xB2, 0x71, 0x58, 0xE8, 0xF4, 0x46, 0x59, 0xB7, 0xFF, 0xB1, 0x91, 0x59, 0x73, 0x70, 0xBC, 0x21, 
0x62, 0xC3, 0xCD, 0x02, 0x03, 0x01, 0x00, 0x01, 0xA3, 0x60, 0x30, 0x5E, 0x30, 0x1F, 0x06, 0x03, 
0x55, 0x1D, 0x23, 0x04, 0x18, 0x30, 0x16, 0x80, 0x14, 0x1B, 0xE9, 0xF5, 0x28, 0x01, 0x26, 0x6A, 
0xB3, 0xAE, 0x28, 0xCB, 0x79, 0x1B, 0xE5, 0x79, 0xAF, 0xA8, 0x49, 0xE0, 0xB4, 0x30, 0x1D, 0x06, 
0x03, 0x55, 0x1D, 0x0E, 0x04, 0x16, 0x04, 0x14, 0x2B, 0xB5, 0x63, 0x3D, 0x90, 0xFA, 0x18, 0x6A, 
0x98, 0xFE, 0x59, 0xC0, 0xA6, 0xCE, 0x1B, 0x43, 0x5F, 0x42, 0xFB, 0xBE, 0x30, 0x0C, 0x06, 0x03, 
0x55, 0x1D, 0x13, 0x01, 0x01, 0xFF, 0x04, 0x02, 0x30, 0x00, 0x30, 0x0E, 0x06, 0x03, 0x55, 0x1D, 
0x0F, 0x01, 0x01, 0xFF, 0x04, 0x04, 0x03, 0x02, 0x07, 0x80, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 
0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B, 0x05, 0x00, 0x03, 0x82, 0x01, 0x01, 0x00, 0x57, 0xE3, 
0x86, 0x55, 0xC9, 0x2B, 0xF1, 0x58, 0x5C, 0x41, 0x9B, 0xCA, 0x0C, 0x44, 0xD9, 0x76, 0xDF, 0x1D, 
0xCA, 0xCC, 0xE9, 0x5B, 0x80, 0xE3, 0xCE, 0x2C, 0xAF, 0xAD, 0x47, 0xF1, 0x54, 0xC4, 0xE6, 0xC8, 
0x65, 0x52, 0xE5, 0x13, 0x7D, 0x2C, 0xB6, 0xE2, 0x54, 0x43, 0xF7, 0x25, 0x4A, 0x84, 0x7A, 0xBD, 
0x55, 0xE9, 0x7A, 0x9E, 0x62, 0x9D, 0x0C, 0xB1, 0x0D, 0x45, 0xF0, 0xAE, 0xB9, 0xAE, 0x88, 0xE0, 
0xD3, 0xE6, 0xB6, 0xC0, 0x86, 0x90, 0xD2, 0x90, 0x68, 0x8B, 0xFC, 0x51, 0xDF, 0xF1, 0x1F, 0x2A, 
0x56, 0x50, 0x76, 0x5C, 0x05, 0x48, 0x3F, 0x23, 0x8E, 0x13, 0x7C, 0xD5, 0x8D, 0x6E, 0xD1, 0x73, 
0xFD, 0x12, 0x75, 0xCB, 0xB5, 0x90, 0x45, 0x99, 0x92, 0x36, 0xDF, 0x06, 0x90, 0x78, 0xD8, 0x6A, 
0xAC, 0x5B, 0xA1, 0xB5, 0x14, 0x40, 0x40, 0x63, 0x95, 0xE6, 0x45, 0xE5, 0x40, 0xE9, 0xCB, 0xB5, 
0x4D, 0x57, 0xAF, 0x42, 0x74, 0x1C, 0xEC, 0x4F, 0x1C, 0xB7, 0xAE, 0x3B, 0x07, 0x99, 0x6F, 0xE9, 
0x5E, 0x4C, 0xAE, 0x79, 0x3E, 0xED, 0xDD, 0xDE, 0xF2, 0x09, 0x20, 0xCE, 0xFD, 0x89, 0x9A, 0xC8, 
0x6D, 0xB0, 0xBE, 0xD4, 0x52, 0x53, 0x99, 0xFA, 0xD4, 0x8B, 0x7B, 0x7E, 0x13, 0xE2, 0xC3, 0xAE, 
0xEC, 0x1F, 0x96, 0x04, 0x7C, 0xEF, 0xEF, 0xDB, 0x0E, 0x89, 0x18, 0x13, 0x68, 0x78, 0xF0, 0x31, 
0x28, 0x93, 0x99, 0xA4, 0x9A, 0xDD, 0x01, 0x0D, 0xC6, 0xC2, 0x1D, 0x93, 0x20, 0x79, 0x7B, 0xD5, 
0x8F, 0x0E, 0x07, 0x04, 0xDA, 0x04, 0x18, 0xFE, 0x0E, 0x46, 0xFF, 0xA4, 0xDA, 0xCD, 0xB2, 0xCA, 
0xAF, 0xB9, 0x05, 0xFE, 0x12, 0x71, 0x66, 0xCC, 0x45, 0xB3, 0xBD, 0x28, 0xA3, 0xB4, 0x42, 0x43, 
0x0C, 0x15, 0x53, 0x40, 0x19, 0x9B, 0x9E, 0xB9, 0x27, 0xB9, 0x51, 0x4F, 0x40, 0x9A};

mbedtls_x509_crt cert_pre_parsed;

mbedtls_x509_crt* NonvolatileDataManager::getCaCertificatePreParsed() {
    memset(&cacert_pre_parsed, 0, sizeof(cacert_pre_parsed));

    memcpy(&cacert_subject_country_oid, &cacert_subject_country_oid_data, sizeof(cacert_subject_country_oid));
    memcpy(&cacert_subject_country_value, &cacert_subject_country_value_data, sizeof(cacert_subject_country_value));
    memcpy(&cacert_subject_organization_oid, &cacert_subject_organization_oid_data, sizeof(cacert_subject_organization_oid));
    memcpy(&cacert_subject_organization_value, &cacert_subject_organization_value_data, sizeof(cacert_subject_organization_value));
    memcpy(&cacert_subject_common_name_oid, &cacert_subject_common_name_oid_data, sizeof(cacert_subject_common_name_oid));
    memcpy(&cacert_subject_common_name_value, &cacert_subject_common_name_value_data, sizeof(cacert_subject_common_name_value));
    memcpy(&cacert_subject_common_name, &cacert_subject_common_name_data, sizeof(cacert_subject_common_name));
    memcpy(&cacert_subject_organization, &cacert_subject_organization_data, sizeof(cacert_subject_organization));
    // double check the subject
    cacert_pre_parsed.subject.oid.tag = 6;
    cacert_pre_parsed.subject.oid.len = 3;
    cacert_pre_parsed.subject.oid.p = (unsigned char *)cacert_subject_country_oid;
    cacert_pre_parsed.subject.val.tag = 19; //MBEDTLS_ASN1_PRINTABLE_STRING
    cacert_pre_parsed.subject.val.len = 2;
    cacert_pre_parsed.subject.val.p = (unsigned char *)cacert_subject_country_value;
    cacert_pre_parsed.subject.next = &cacert_subject_organization;

    cacert_pre_parsed.ca_istrue = 1;

    cacert_pre_parsed.pk.pk_info = &mbedtls_rsa_info; // Set the public key info to RSA
    memcpy(&cacert_rsa, &cacert_rsa_data, sizeof(cacert_rsa_data)); // Copy the RSA context data
    cacert_pre_parsed.pk.pk_ctx = (void *)&cacert_rsa; // Set the public key context to our RSA context

    mbedtls_rsa_context* rsa_ctx = (mbedtls_rsa_context *) cacert_pre_parsed.pk.pk_ctx;
    memcpy(cacert_N_p, cacert_N_p_data, sizeof(cacert_N_p_data)); // Copy the N value
    memcpy(cacert_E_p, cacert_E_p_data, sizeof(cacert_E_p_data)); // Copy the E value
    rsa_ctx->N.p = (uint32_t *)cacert_N_p; // Set the N value
    rsa_ctx->E.p = (uint32_t *)cacert_E_p;

    return &cacert_pre_parsed;
}

unsigned char* NonvolatileDataManager::getRootCA() {
  return (unsigned char*)rootCABuffHardEncoded;
}

unsigned char* NonvolatileDataManager::getPrivateKey() {
    char *privateKeyPtr = (char *)(PRIVATE_KEY_FLASH_ADDR);
    if (privateKeyPtr == NULL){
        Serial.println("Private key is NULL");
        return (unsigned char*)emptyString;
    }
    if (privateKeyPtr[0] == 0){
        Serial.println("Private key is empty");
        return (unsigned char*)emptyString;
    }
    return (unsigned char*)privateKeyPtr;
}

mbedtls_x509_crt* NonvolatileDataManager::getCertificatePreParsed() {
    memset(&cert_pre_parsed, 0, sizeof(cert_pre_parsed));
    memcpy(cli_crt_raw, cli_crt_raw_data, sizeof(cli_crt_raw_data));
    cert_pre_parsed.raw.tag = 0;
    cert_pre_parsed.raw.len = sizeof(cli_crt_raw);
    cert_pre_parsed.raw.p = (unsigned char *)cli_crt_raw;

    return &cert_pre_parsed;
//   char *certificatePtr = (char *)(CERT_FLASH_ADDR);
//   if (certificatePtr == NULL){
//       Serial.println("Certificate is NULL");
//       return (unsigned char*)emptyString;
//   }
//   if (certificatePtr[0] == 0){
//       Serial.println("Certificate is empty");
//       return (unsigned char*)emptyString;
//   }
//   return (unsigned char*)certificatePtr;
}

unsigned char* NonvolatileDataManager::getFirmwareSignPubKey() {
  return (unsigned char*)firmwareSignPubKey;
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

void NonvolatileDataManager::initialize(){
    FlashMemory.begin(0x00200000, FLASH_SECTOR_SIZE);   //for 4MB layout
}

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
    Serial.print("Memory dump from address: 0x");
    Serial.print(startAddress, HEX);
    Serial.print(" length: 0x");
    Serial.println(length, HEX);
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
            if (buf[j] < 0x10){
                Serial.print("0");
            }
            Serial.print(buf[j], HEX);
            Serial.print(" ");
        }
        Serial.println();

      }
    }
}


char *NonvolatileDataManager::getThingName(){
    char *thingNamePtr = (char *)(THING_NAME_FLASH_ADDR);
    if (thingNamePtr == NULL){
        Serial.println("Thing name is NULL");
        return emptyString;
    }
    if (thingNamePtr[0] == 0){
        Serial.println("Thing name is empty");
        return emptyString;
    }
    return thingNamePtr;
}
char *NonvolatileDataManager::getClientId(){
    char *clientIdPtr = (char *)(CLIENT_ID_FLASH_ADDR);
    if (clientIdPtr == NULL){
        Serial.println("Client ID is NULL");
        return emptyString;
    }
    if (clientIdPtr[0] == 0){
        Serial.println("Client ID is empty");
        return emptyString;
    }
    return clientIdPtr;
}
