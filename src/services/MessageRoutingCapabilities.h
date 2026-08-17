#pragma once

#include <QString>
#include <QStringList>

namespace MessageRoutingCapabilities {

inline QString remoteMessageV1()
{
    return QStringLiteral("remote_message_v1");
}

inline QString serverReceiveV1()
{
    return QStringLiteral("server_receive_v1");
}

inline QString p2pDeliveryReceiptV1()
{
    return QStringLiteral("p2p_delivery_receipt_v1");
}

inline bool hasP2PDeliveryReceiptV1(const QStringList& capabilities)
{
    return capabilities.contains(p2pDeliveryReceiptV1(), Qt::CaseInsensitive);
}

inline bool hasServerReceiveV1(const QStringList& capabilities)
{
    return capabilities.contains(serverReceiveV1(), Qt::CaseInsensitive);
}

}  // namespace MessageRoutingCapabilities
