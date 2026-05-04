#ifndef MODIFIED_WIFI_CLIENT_H
#define MODIFIED_WIFI_CLIENT_H

#include "WiFi.h"

#include "Print.h"
#include "Client.h"
#include "IPAddress.h"

#include "ModifiedSslDrv.h"

struct mbedtls_ssl_context;
class ModifiedWifiSSLClient : public Client {
public:

    ModifiedWifiSSLClient();
    ModifiedWifiSSLClient(uint8_t sock);
    ~ModifiedWifiSSLClient();

    uint8_t status();
    virtual int connect(IPAddress ip, uint16_t port);
    virtual int connect(const char *host, uint16_t port);
    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *buf, size_t size);
    virtual int available();
    virtual int read();
    virtual int read(uint8_t *buf, size_t size);
    virtual int peek();
    virtual void flush();
    virtual void stop();
    virtual uint8_t connected();
    virtual operator bool();

    void setRootCAPreParsed(mbedtls_x509_crt *rootCA);
    void setClientCertificate(mbedtls_x509_crt *client_ca, unsigned char *private_key);

    int connect(const char *host, uint16_t port, mbedtls_x509_crt *rootCA, mbedtls_x509_crt *cli_cert, unsigned char* cli_key);
    int connect(IPAddress ip, uint16_t port, mbedtls_x509_crt *rootCA, mbedtls_x509_crt *cli_cert, unsigned char* cli_key);


    using Print::write;
    int setRecvTimeout(int timeout);

private:
    int _sock;
    bool _is_connected;
    sslclient_context sslclient;
    ModifiedSSLDrv ssldrv;

    mbedtls_x509_crt *_rootCaPreParsed;
    mbedtls_x509_crt *_cli_certPreParsed;
    unsigned char *_cli_key;
    char *_sni_hostname;
};

#endif // MODIFIED_WIFI_CLIENT_H