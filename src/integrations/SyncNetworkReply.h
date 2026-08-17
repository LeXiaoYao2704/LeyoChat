#pragma once

#include <QCoreApplication>
#include <QEvent>
#include <QNetworkReply>
#include <QPointer>

inline void deleteSynchronousNetworkReply(QPointer<QNetworkReply>& reply)
{
    if (!reply) {
        return;
    }

    QNetworkReply* rawReply = reply.data();
    rawReply->deleteLater();
    QCoreApplication::sendPostedEvents(rawReply, QEvent::DeferredDelete);
}

inline void deleteSynchronousNetworkReply(QNetworkReply* reply)
{
    if (!reply) {
        return;
    }

    QPointer<QNetworkReply> guarded(reply);
    deleteSynchronousNetworkReply(guarded);
}
