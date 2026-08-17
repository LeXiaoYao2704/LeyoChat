#pragma once

#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>

class QSslSocket;

class TlsHelper {
public:
    static bool isAvailable();

    static QSslConfiguration defaultConfiguration();

    static void configureSslSocket(QSslSocket* socket);

private:
    static bool loadCredentials();

    static QSslCertificate s_certificate;
    static QSslKey s_privateKey;
    static bool s_loaded;
    static bool s_available;
};
