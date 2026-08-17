#include "ui/MessageDeliveryPresentation.h"

namespace {

bool isSentState(MessageDeliveryState state)
{
    return state == MessageDeliveryState::Sent
        || state == MessageDeliveryState::ServerAcked;
}

}

QString messageDeliveryStateText(MessageDeliveryState state)
{
    switch (state) {
    case MessageDeliveryState::Pending:
        return QStringLiteral("发送中");
    case MessageDeliveryState::Sent:
    case MessageDeliveryState::ServerAcked:
        return QStringLiteral("已发送");
    case MessageDeliveryState::Received:
        return QStringLiteral("已送达");
    case MessageDeliveryState::Read:
        return QStringLiteral("已读");
    case MessageDeliveryState::Failed:
        return QStringLiteral("发送失败");
    }
    return QStringLiteral("状态未知");
}

QString messageDeliveryIndicatorText(MessageDeliveryState state,
                                     bool outgoing,
                                     int groupReadCount,
                                     int groupActiveMemberCount)
{
    if (!outgoing) {
        return {};
    }

    if (groupActiveMemberCount > 0) {
        const int expectedReaders = groupActiveMemberCount - 1;
        if (expectedReaders <= 0) {
            return isSentState(state) ? QStringLiteral("\u2713")
                                      : messageDeliveryStateText(state);
        }
        if (groupReadCount >= expectedReaders) {
            return QStringLiteral("全部已读");
        }
        if (groupReadCount > 0) {
            return QStringLiteral("%1人已读").arg(groupReadCount);
        }
        return isSentState(state) ? QStringLiteral("\u2713")
                                  : messageDeliveryStateText(state);
    }

    if (state == MessageDeliveryState::Read) {
        return QStringLiteral("已读");
    }
    if (state == MessageDeliveryState::Received) {
        return QStringLiteral("已送达");
    }
    return messageDeliveryStateText(state);
}
