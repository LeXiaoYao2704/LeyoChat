#pragma once

#include <memory>

#include <QDateTime>
#include <QVector>

#include "integrations/LocalOutlookAdapter.h"
#include "integrations/OutlookEwsTransport.h"
#include "integrations/OutlookNotificationContracts.h"
#include "integrations/OutlookSettings.h"

struct OutlookNotificationPollResult {
    QVector<OutlookNotificationEvent> events;
    OutlookConnectionSettings updatedSettings;
};

class OutlookNotificationPoller {
public:
    explicit OutlookNotificationPoller(
        OutlookConnectionSettings settings = {},
        std::shared_ptr<IOutlookEwsTransport> ewsTransport =
            std::make_shared<NetworkOutlookEwsTransport>());

    OutlookNotificationPollResult poll(const QDateTime& now, QString* errorMessage = nullptr);

private:
    static QStringList rememberRecentIds(const QStringList& existing,
                                         const QStringList& latest,
                                         int maxCount);

    OutlookConnectionSettings m_settings;
    LocalOutlookAdapter m_adapter;
};
