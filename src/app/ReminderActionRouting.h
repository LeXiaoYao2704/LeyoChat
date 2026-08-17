#pragma once

#include <optional>

#include <QString>
#include <QUrl>

struct ReminderActionRoute {
    QString verb;
    QString reminderId;
};

inline QString reminderActionUrl(const QString& verb, const QString& reminderId)
{
    const QString normalizedVerb = verb.trimmed().toLower();
    const QString trimmedId = reminderId.trimmed();
    if (trimmedId.isEmpty()) {
        return {};
    }
    if (normalizedVerb != QStringLiteral("open") && normalizedVerb != QStringLiteral("done")
        && normalizedVerb != QStringLiteral("dismiss") && normalizedVerb != QStringLiteral("snooze")) {
        return {};
    }

    return QStringLiteral("leyochat://reminder/%1/%2")
        .arg(normalizedVerb,
             QString::fromUtf8(QUrl::toPercentEncoding(trimmedId)));
}

inline std::optional<ReminderActionRoute> parseReminderActionUrl(const QString& urlString)
{
    const QUrl url(urlString.trimmed());
    if (!url.isValid() || url.scheme().compare(QStringLiteral("leyochat"), Qt::CaseInsensitive) != 0
        || url.host().compare(QStringLiteral("reminder"), Qt::CaseInsensitive) != 0) {
        return std::nullopt;
    }

    const QString encodedPath = url.path(QUrl::FullyEncoded);
    const QStringList segments =
        (encodedPath.startsWith(QLatin1Char('/')) ? encodedPath.mid(1) : encodedPath)
            .split(QLatin1Char('/'));
    if (segments.size() != 2) {
        return std::nullopt;
    }

    const QString verb = QUrl::fromPercentEncoding(segments.at(0).toUtf8()).trimmed().toLower();
    const QString reminderId = QUrl::fromPercentEncoding(segments.at(1).toUtf8()).trimmed();
    if (verb.isEmpty() || reminderId.isEmpty()) {
        return std::nullopt;
    }
    if (verb != QStringLiteral("open") && verb != QStringLiteral("done")
        && verb != QStringLiteral("dismiss") && verb != QStringLiteral("snooze")) {
        return std::nullopt;
    }

    return ReminderActionRoute{verb, reminderId};
}
