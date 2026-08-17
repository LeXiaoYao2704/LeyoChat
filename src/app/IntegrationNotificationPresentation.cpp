#include "app/IntegrationNotificationPresentation.h"

namespace IntegrationNotificationPresentation {

QString trayUnreadToolTip(const QString& appDisplayName, int unreadCount)
{
    if (unreadCount <= 0) {
        return appDisplayName.trimmed();
    }
    return QStringLiteral("%1 · %2 条未处理提醒")
        .arg(appDisplayName.trimmed(), QString::number(unreadCount));
}

QString trayUnreadStatusActionText(const QString& lastUnreadTitle, int unreadCount)
{
    if (unreadCount <= 0) {
        return QStringLiteral("当前没有未处理提醒");
    }
    const QString title =
        lastUnreadTitle.trimmed().isEmpty() ? QStringLiteral("新消息提醒")
                                            : lastUnreadTitle.trimmed();
    return QStringLiteral("%1 · %2 条未处理提醒")
        .arg(title, QString::number(unreadCount));
}

QString integrationFailureTitle(const QString& sourceLabel, const QString& category)
{
    if (category == QStringLiteral("auth")) {
        return QStringLiteral("%1 授权已失效").arg(sourceLabel);
    }
    if (category == QStringLiteral("network")) {
        return QStringLiteral("%1 连接异常").arg(sourceLabel);
    }
    return QStringLiteral("%1 同步异常").arg(sourceLabel);
}

QString integrationFailureSummary(const QString& sourceLabel,
                                  const QString& category,
                                  int consecutiveFailures,
                                  int nextPollMinutes)
{
    QString reason = QStringLiteral("最近一次同步没有成功完成。");
    if (category == QStringLiteral("auth")) {
        reason = QStringLiteral("请重新检查授权状态；如果刷新令牌已过期，请重新完成登录授权。");
    } else if (category == QStringLiteral("network")) {
        reason = QStringLiteral("网络不可达或服务暂时无响应，请稍后再试。");
    }
    return QStringLiteral("%1 连续失败 %2 次，约 %3 分钟后自动重试。%4")
        .arg(sourceLabel,
             QString::number(qMax(1, consecutiveFailures)),
             QString::number(qMax(1, nextPollMinutes)),
             reason);
}

QString integrationRecoveryTitle(const QString& sourceLabel)
{
    return QStringLiteral("%1 已恢复").arg(sourceLabel);
}

QString integrationRecoverySummary(const QString& sourceLabel)
{
    return QStringLiteral("%1 的连接与轮询已恢复，后续提醒会继续进入通知中心。")
        .arg(sourceLabel);
}

}  // namespace IntegrationNotificationPresentation
