#include "network/TlsHelper.h"

#include <QSslSocket>
#include <QDebug>

QSslCertificate TlsHelper::s_certificate;
QSslKey TlsHelper::s_privateKey;
bool TlsHelper::s_loaded = false;
bool TlsHelper::s_available = false;

bool TlsHelper::isAvailable() {
    if (!QSslSocket::supportsSsl()) {
        return false;
    }
    loadCredentials();
    return s_available;
}

QSslConfiguration TlsHelper::defaultConfiguration() {
    loadCredentials();

    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.setProtocol(QSsl::TlsV1_2OrLater);
    // TLS is opt-in and requires deployment-specific credentials. The open-source
    // client deliberately does not ship a shared private key.
    config.setPeerVerifyMode(QSslSocket::VerifyPeer);
    config.setPeerVerifyDepth(1);
    config.setOcspStaplingEnabled(false);

    if (s_available) {
        config.setLocalCertificate(s_certificate);
        config.setPrivateKey(s_privateKey);
        // Deployment-provided credentials may add their own trust chain here.
        config.setCaCertificates({s_certificate});
    }

    return config;
}

void TlsHelper::configureSslSocket(QSslSocket* socket) {
    if (!socket) {
        return;
    }
    socket->setSslConfiguration(defaultConfiguration());
    // A deployment-provided self-signed certificate may still need these
    // narrowly scoped exceptions when peers connect by IP address.
    if (s_available) {
        QList<QSslError> expectedErrors;
        expectedErrors.append(QSslError(QSslError::HostNameMismatch, s_certificate));
        expectedErrors.append(QSslError(QSslError::SelfSignedCertificate, s_certificate));
        socket->ignoreSslErrors(expectedErrors);
    }
}

bool TlsHelper::loadCredentials() {
    if (s_loaded) {
        return s_available;
    }
    s_loaded = true;
    // Never publish one private key for every installation. A deployment that
    // needs encrypted P2P must provide credentials through a future explicit
    // configuration path; until then negotiation stays disabled.
    qInfo() << "TlsHelper: no deployment-specific TLS credentials configured";
    return false;
}
