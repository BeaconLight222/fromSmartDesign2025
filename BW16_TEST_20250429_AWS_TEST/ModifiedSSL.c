
#include <sockets.h> 
#include <lwip/netif.h>
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/error.h>
#include <mbedtls/debug.h>

#include "ard_ssl.h"

static unsigned int ard_ssl_arc4random(void) {
    unsigned int res = xTaskGetTickCount();
    static unsigned int seed = 0xDEADB00B;

    seed = ((seed & 0x007F00FF) << 7) ^ 
           ((seed & 0x0F80FF00) >> 8) ^ // be sure to stir those low bits
            (res << 13) ^ (res >> 9);    // using the clock too!

    return seed;
}

static void get_random_bytes(void *buf, size_t len) {
    unsigned int ranbuf;
    unsigned int *lp;
    int i, count;
    count = len / sizeof(unsigned int);
    lp = (unsigned int *) buf;

    for (i = 0; i < count; i ++) {
        lp[i] = ard_ssl_arc4random();
        len -= sizeof(unsigned int);
    }

    if (len > 0) {
        ranbuf = ard_ssl_arc4random();
        memcpy(&lp[i], &ranbuf, len);
    }
}

static int my_random(void *p_rng, unsigned char *output, size_t output_len) {
    p_rng = p_rng;
    get_random_bytes(output, output_len);
    return 0;
}

static void* my_calloc(size_t nelements, size_t elementSize) {
    size_t size;
    void *ptr = NULL;

    size = nelements * elementSize;
    ptr = pvPortMalloc(size);
//  ptr = malloc(size);

    if (ptr) {
        memset(ptr, 0, size);
    }

    return ptr;
}







int start_ssl_client_modified(sslclient_context *ssl_client, uint32_t ipAddress, uint32_t port, mbedtls_x509_crt* rootCABuff, mbedtls_x509_crt* cli_cert, unsigned char* cli_key, char* SNI_hostname) {
    int ret = 0;
    //int timeout;
    int enable = 1;
    int keep_idle = 30;
    mbedtls_pk_context* _clikey_rsa = NULL;

    do {
        ssl_client->socket = -1;
        ssl_client->socket = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (ssl_client->socket < 0) {
            printf("ERROR: opening socket failed! \r\n");
            ret = -1;
            break;
        }

        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = ipAddress;
        serv_addr.sin_port = htons(port);

        lwip_setsockopt(ssl_client->socket, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));
        lwip_setsockopt(ssl_client->socket, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
        if (lwip_connect(ssl_client->socket, ((struct sockaddr *)&serv_addr), sizeof(serv_addr)) < 0) {
            lwip_close(ssl_client->socket);
            printf("ERROR: Connect to Server failed! \r\n");
            ret = -1;
            break;
        } else {
        /*
        if (lwip_connect(ssl_client->socket, ((struct sockaddr *)&serv_addr), sizeof(serv_addr)) == 0) {
            timeout = ssl_client->recvTimeout;
            lwip_setsockopt(ssl_client->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            timeout = 30000;
            lwip_setsockopt(ssl_client->socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
            lwip_setsockopt(ssl_client->socket, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
            lwip_setsockopt(ssl_client->socket, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));
        } else {
            printf("ERROR: Connect to Server failed!\r\n");
            ret = -1;
            break;
        }
        */

            mbedtls_platform_set_calloc_free(my_calloc,vPortFree);

            ssl_client->ssl = (mbedtls_ssl_context *)malloc(sizeof(mbedtls_ssl_context));
            ssl_client->conf = (mbedtls_ssl_config *)malloc(sizeof(mbedtls_ssl_config));
            if (( ssl_client->ssl == NULL )||( ssl_client->conf == NULL )) {
                printf("ERROR: malloc ssl failed! \r\n");
                ret = -1;
                break;
            }
            mbedtls_ssl_init(ssl_client->ssl);
            mbedtls_ssl_config_init(ssl_client->conf);

            // if (ARDUINO_MBEDTLS_DEBUG_LEVEL > 0) {
            //     mbedtls_ssl_conf_verify(ssl_client->conf, my_verify, NULL);
            //     mbedtls_ssl_conf_dbg(ssl_client->conf, my_debug, NULL);
            //     mbedtls_debug_set_threshold(ARDUINO_MBEDTLS_DEBUG_LEVEL);
            // }

            if ((mbedtls_ssl_config_defaults(ssl_client->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
                printf("ERROR: mbedtls ssl config defaults failed! \r\n");
                ret = -1;
                break;
            }
            mbedtls_ssl_conf_rng(ssl_client->conf, my_random, NULL);

            if (rootCABuff != NULL) {
                // Configure mbedTLS to use certificate authentication method
                mbedtls_ssl_conf_ca_chain(ssl_client->conf, rootCABuff, NULL);
                mbedtls_ssl_conf_authmode(ssl_client->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
            } else {
                mbedtls_ssl_conf_authmode(ssl_client->conf, MBEDTLS_SSL_VERIFY_NONE);
            }

            if ((cli_cert != NULL) && (cli_key != NULL)) {

                _clikey_rsa = (mbedtls_pk_context *) mbedtls_calloc( sizeof(mbedtls_pk_context), 1);
                if (_clikey_rsa == NULL) {
                    printf("ERROR: malloc client_rsa failed! \r\n");
                    ret = -1;
                    break;
                }
                mbedtls_pk_init(_clikey_rsa);

                // if (mbedtls_x509_crt_parse(_cli_crt, cli_cert, strlen((char*)cli_cert) + 1) != 0) {
                //     printf("ERROR: mbedtls x509 parse client_crt failed! \r\n");
                //     ret = -1;
                //     break;
                // }
                // mbedtls_x509_buf backup_raw = _cli_crt->raw;
                // memset(_cli_crt, 0, sizeof(mbedtls_x509_crt));
                // _cli_crt->raw = backup_raw; // restore raw buffer
                // memcpy(cli_crt_raw, cli_crt_raw_data, sizeof(cli_crt_raw_data));
                // _cli_crt->raw.tag = 0;
                // _cli_crt->raw.len = sizeof(cli_crt_raw);
                // _cli_crt->raw.p = (unsigned char *)cli_crt_raw;

                //print _cli_crt_raw
                // printf("cli_crt_raw tag: %d, len: %d\r\n", _cli_crt->raw.tag, _cli_crt->raw.len);
                // printf("const unsigned char cli_crt_raw[%d] = {\n", _cli_crt->raw.len);
                // for (size_t i = 0; i < _cli_crt->raw.len; i++) {
                //     printf("0x%02X", _cli_crt->raw.p[i]);
                //     if (i < _cli_crt->raw.len - 1) {
                //         printf(", ");
                //     }
                //     if ((i + 1) % 16 == 0) {
                //         printf("\n");
                //     }
                // }
                // printf("};\n");
                //check if cli_crt_raw is the same as _cli_crt->raw.p
                // if (memcmp(_cli_crt->raw.p, cli_crt_raw, _cli_crt->raw.len) != 0) {
                //     printf("ERROR: cli_crt_raw is not the same as _cli_crt->raw.p!\r\n");
                // }else{
                //     printf("cli_crt_raw is the same as _cli_crt->raw.p!\r\n");
                // }

                if (mbedtls_pk_parse_key(_clikey_rsa, cli_key, strlen((char*)cli_key)+1, NULL, 0) != 0) {
                    printf("ERROR: mbedtls x509 parse client_rsa failed! \r\n");
                    ret = -1;
                    break;
                }

                //print everything of _clikey_rsa, until QP

                mbedtls_ssl_conf_own_cert(ssl_client->conf, cli_cert, _clikey_rsa);
            }

            if ((mbedtls_ssl_setup(ssl_client->ssl, ssl_client->conf)) != 0) {
                printf("ERROR: mbedtls ssl setup failed!\r\n");
                ret = -1;
                break;
            }
            mbedtls_ssl_set_bio(ssl_client->ssl, &ssl_client->socket, mbedtls_net_send, mbedtls_net_recv, NULL);

            mbedtls_ssl_set_hostname(ssl_client->ssl, SNI_hostname);

            ret = mbedtls_ssl_handshake(ssl_client->ssl);
            if (ret < 0) {
                printf("ERROR: mbedtls ssl handshake failed: -0x%04X \r\n", -ret);
                ret = -1;
            } else { 
                // if (ARDUINO_MBEDTLS_DEBUG_LEVEL > 0) {
                //     printf("mbedTLS SSL handshake success \r\n");
                // }
            }
            //mbedtls_debug_set_threshold(ARDUINO_MBEDTLS_DEBUG_LEVEL);
        }
    } while (0);

    if (_clikey_rsa) {
        mbedtls_pk_free(_clikey_rsa);
        mbedtls_free(_clikey_rsa);
        _clikey_rsa = NULL;
    }

    if (ret < 0) {
        if (ssl_client->socket >= 0) {
            mbedtls_net_free((mbedtls_net_context *)&ssl_client->socket);
            ssl_client->socket = -1;
        }

        if (ssl_client->ssl != NULL) {
            mbedtls_ssl_free(ssl_client->ssl);
            free(ssl_client->ssl);
            ssl_client->ssl = NULL;
        }
        if (ssl_client->conf != NULL) {
            mbedtls_ssl_config_free(ssl_client->conf);
            free(ssl_client->conf);
            ssl_client->conf = NULL;
        }
    }
    return ssl_client->socket;
}