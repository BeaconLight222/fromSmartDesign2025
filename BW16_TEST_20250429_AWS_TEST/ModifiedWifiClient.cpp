#include "ModifiedWifiClient.h"

#ifdef __cplusplus
extern "C" {
#include "platform_stdlib.h"
}
#endif

ModifiedWifiSSLClient::ModifiedWifiSSLClient() {
    _is_connected = false;
    _sock = -1;

    sslclient.socket = -1;
    sslclient.ssl = NULL;
    sslclient.recvTimeout = 3000;

    _rootCaPreParsed = NULL;
    _cli_certPreParsed = NULL;
    _cli_key = NULL;
    _sni_hostname = NULL;
}

ModifiedWifiSSLClient::ModifiedWifiSSLClient(uint8_t sock) {
    _sock = sock;

    sslclient.socket = -1;
    sslclient.ssl = NULL;
    sslclient.recvTimeout = 3000;

//    if(sock >= 0) {
//        _is_connected = true;
//    }
    _is_connected = true;

    _rootCaPreParsed = NULL;
    _cli_certPreParsed = NULL;
    _cli_key = NULL;
    _sni_hostname = NULL;
}

ModifiedWifiSSLClient::~ModifiedWifiSSLClient() {
    stop();
}

uint8_t ModifiedWifiSSLClient::connected() {
    if (sslclient.socket < 0) {
        _is_connected = false;
        return 0;
    } else {
        if (_is_connected) {
            return 1;
        } else {
            stop();
            return 0;
        }
    }
}

int ModifiedWifiSSLClient::available() {
    int ret = 0;
    int err;

    if (!_is_connected) {
        return 0;
    }
    if (sslclient.socket >= 0) {
        ret = ssldrv.availData(&sslclient);
        if (ret > 0) {
            return 1;
        } else {
            err = ssldrv.getLastErrno(&sslclient);
            if ((err > 0) && (err != EAGAIN)) {
                _is_connected = false;
            }
        }
        return 0;
    }

    return 0;
}

int ModifiedWifiSSLClient::read() {
    int ret;
    int err;
    uint8_t b[1];

    if (!available()) {
        return -1;
    }

    ret = ssldrv.getData(&sslclient, b);
    if (ret > 0) {
        return b[0];
    } else {
        err = ssldrv.getLastErrno(&sslclient);
        if ((err > 0) && (err != EAGAIN)) {
            _is_connected = false;
        }
    }
    return -1;
}

int ModifiedWifiSSLClient::read(uint8_t* buf, size_t size) {
    uint16_t _size = size;
    int ret;
    int err;

    ret = ssldrv.getDataBuf(&sslclient, buf, _size);
    if (ret <= 0) {
        err = ssldrv.getLastErrno(&sslclient);
        if ((err > 0) && (err != EAGAIN)) {
            _is_connected = false;
        }
    }
    return ret;
}

void ModifiedWifiSSLClient::stop() {

    if (sslclient.socket < 0) {
        return;
    }

    ssldrv.stopClient(&sslclient);
    _is_connected = false;

    sslclient.socket = -1;
    _sock = -1;
}

size_t ModifiedWifiSSLClient::write(uint8_t b) {
    return write(&b, 1);
}

size_t ModifiedWifiSSLClient::write(const uint8_t *buf, size_t size) {
    if (sslclient.socket < 0) {
        setWriteError();
        return 0;
    }
    if (size == 0) {
        setWriteError();
        return 0;
    }

    if (!ssldrv.sendData(&sslclient, buf, size)) {
        setWriteError();
        _is_connected = false;
        return 0;
    }

    return size;
}

ModifiedWifiSSLClient::operator bool() {
    return (sslclient.socket >= 0);
}

int ModifiedWifiSSLClient::connect(IPAddress ip, uint16_t port) {
    return connect(ip, port, _rootCaPreParsed, _cli_certPreParsed, _cli_key);
}

int ModifiedWifiSSLClient::connect(const char *host, uint16_t port) {
    if (_sni_hostname == NULL) {
        _sni_hostname = (char*)host;
    }

    return connect(host, port, _rootCaPreParsed, _cli_certPreParsed, _cli_key);
}

int ModifiedWifiSSLClient::connect(const char* host, uint16_t port, mbedtls_x509_crt* rootCABuff, mbedtls_x509_crt* cli_cert, unsigned char* cli_key) {
    IPAddress remote_addr;

    if (_sni_hostname == NULL) {
        _sni_hostname = (char*)host;
    }

    if (WiFi.hostByName(host, remote_addr)) {
        return connect(remote_addr, port, rootCABuff, cli_cert, cli_key);
    }
    return 0;
}

int ModifiedWifiSSLClient::connect(IPAddress ip, uint16_t port, mbedtls_x509_crt* rootCABuff, mbedtls_x509_crt* cli_cert, unsigned char* cli_key) {
    int ret = 0;

    ret = ssldrv.startClient(&sslclient, ip, port, rootCABuff, cli_cert, cli_key, _sni_hostname);

    if (ret < 0) {
        _is_connected = false;
        return 0;
    } else {
        _is_connected = true;
    }

    return 1;
}

int ModifiedWifiSSLClient::peek() {
    uint8_t b;

    if (!available()) {
        return -1;
    }

    ssldrv.getData(&sslclient, &b, 1);

    return b;
}
void ModifiedWifiSSLClient::flush() {
    while (available()) {
        read();
    }
}

void ModifiedWifiSSLClient::setRootCAPreParsed(mbedtls_x509_crt *rootCA) {
    _rootCaPreParsed = rootCA;
}

void ModifiedWifiSSLClient::setClientCertificate(mbedtls_x509_crt *client_ca, unsigned char *private_key) {
    _cli_certPreParsed = client_ca;
    _cli_key = private_key;
}

int ModifiedWifiSSLClient::setRecvTimeout(int timeout) {
    sslclient.recvTimeout = timeout;
    if (connected()) {
        ssldrv.setSockRecvTimeout(sslclient.socket, sslclient.recvTimeout);
    }

    return 0;
}


