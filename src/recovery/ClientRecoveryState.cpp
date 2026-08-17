#include "recovery/ClientRecoveryState.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace
{
constexpr int kSchemaVersion = 1;
constexpr qsizetype kMaxSessionIdLength = 128;
constexpr qsizetype kMaxConversationIdLength = 512;
constexpr qsizetype kMaxNavigationPageIdLength = 64;
constexpr qint64 kMaximumFutureSkewMs = 5 * 60 * 1000;

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage)
    {
        *errorMessage = message;
    }
}

QString boundedOptional(const QString& value, qsizetype maximumLength)
{
    return value.size() <= maximumLength ? value : QString();
}

QString windowModeName(RecoveryWindowMode mode)
{
    switch (mode)
    {
    case RecoveryWindowMode::Visible:
        return QStringLiteral("visible");
    case RecoveryWindowMode::Minimized:
        return QStringLiteral("minimized");
    case RecoveryWindowMode::TrayHidden:
        return QStringLiteral("tray-hidden");
    }
    return QStringLiteral("visible");
}

std::optional<RecoveryWindowMode> windowModeFromName(const QString& name)
{
    if (name == QStringLiteral("visible"))
        return RecoveryWindowMode::Visible;
    if (name == QStringLiteral("minimized"))
        return RecoveryWindowMode::Minimized;
    if (name == QStringLiteral("tray-hidden"))
        return RecoveryWindowMode::TrayHidden;
    return std::nullopt;
}

QJsonObject toJson(const ClientRecoveryState& state)
{
    QJsonObject object;
    object.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
    object.insert(QStringLiteral("sessionId"), state.sessionId);
    object.insert(QStringLiteral("savedAtMs"), static_cast<double>(state.savedAtMs));
    object.insert(QStringLiteral("windowMode"), windowModeName(state.windowMode));
    const QByteArray geometry =
        state.windowGeometry.size() <= ClientRecoveryStateStore::maximumWindowGeometryBytes()
        ? state.windowGeometry
        : QByteArray();
    object.insert(QStringLiteral("windowGeometry"),
                  QString::fromLatin1(geometry.toBase64()));
    object.insert(QStringLiteral("windowMaximized"), state.windowMaximized);
    object.insert(QStringLiteral("navigationPageId"),
                  boundedOptional(state.navigationPageId, kMaxNavigationPageIdLength));
    object.insert(QStringLiteral("conversationId"),
                  boundedOptional(state.conversationId, kMaxConversationIdLength));
    object.insert(QStringLiteral("composerHtml"),
                  boundedOptional(state.composerHtml,
                                  ClientRecoveryStateStore::maximumComposerHtmlLength()));
    object.insert(QStringLiteral("replyMessageId"),
                  boundedOptional(state.replyMessageId, kMaxConversationIdLength));
    object.insert(QStringLiteral("replySenderId"),
                  boundedOptional(state.replySenderId, kMaxConversationIdLength));
    object.insert(QStringLiteral("replySenderName"),
                  boundedOptional(state.replySenderName,
                                  ClientRecoveryStateStore::maximumContextTextLength()));
    object.insert(QStringLiteral("replyBody"),
                  boundedOptional(state.replyBody,
                                  ClientRecoveryStateStore::maximumContextTextLength()));
    object.insert(QStringLiteral("editingMessageId"),
                  boundedOptional(state.editingMessageId, kMaxConversationIdLength));
    object.insert(QStringLiteral("editingBody"),
                  boundedOptional(state.editingBody,
                                  ClientRecoveryStateStore::maximumContextTextLength()));
    return object;
}
}

ClientRecoveryStateStore::ClientRecoveryStateStore(QString filePath)
    : m_filePath(std::move(filePath))
{
}

bool ClientRecoveryStateStore::shouldPersistSnapshot(bool payloadChanged,
                                                     qint64 lastSavedAtMs,
                                                     qint64 nowMs)
{
    return payloadChanged || lastSavedAtMs <= 0
        || nowMs - lastSavedAtMs >= snapshotRefreshIntervalMs();
}

bool ClientRecoveryStateStore::save(const ClientRecoveryState& state,
                                    QString* errorMessage) const
{
    if (state.sessionId.isEmpty() || state.sessionId.size() > kMaxSessionIdLength)
    {
        setError(errorMessage, QStringLiteral("Recovery session id is invalid."));
        return false;
    }
    if (state.savedAtMs <= 0)
    {
        setError(errorMessage, QStringLiteral("Recovery timestamp is invalid."));
        return false;
    }

    const QFileInfo info(m_filePath);
    if (!QDir().mkpath(info.absolutePath()))
    {
        setError(errorMessage, QStringLiteral("Cannot create recovery directory."));
        return false;
    }

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        setError(errorMessage, file.errorString());
        return false;
    }

    const QByteArray json = QJsonDocument(toJson(state)).toJson(QJsonDocument::Compact);
    if (json.size() > maximumFileSizeBytes())
    {
        setError(errorMessage, QStringLiteral("Recovery state is too large."));
        file.cancelWriting();
        return false;
    }
    if (file.write(json) != json.size())
    {
        setError(errorMessage, file.errorString());
        file.cancelWriting();
        return false;
    }
    if (!file.commit())
    {
        setError(errorMessage, file.errorString());
        return false;
    }

    setError(errorMessage, QString());
    return true;
}

std::optional<ClientRecoveryState> ClientRecoveryStateStore::loadForSession(
    const QString& expectedSessionId,
    qint64 nowMs,
    QString* errorMessage) const
{
    if (expectedSessionId.isEmpty() || expectedSessionId.size() > kMaxSessionIdLength)
    {
        setError(errorMessage, QStringLiteral("Expected recovery session id is invalid."));
        return std::nullopt;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        setError(errorMessage, file.errorString());
        return std::nullopt;
    }

    if (file.size() > maximumFileSizeBytes())
    {
        setError(errorMessage, QStringLiteral("Recovery file is too large."));
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QByteArray bytes = file.read(maximumFileSizeBytes() + 1);
    if (bytes.size() > maximumFileSizeBytes())
    {
        setError(errorMessage, QStringLiteral("Recovery file is too large."));
        return std::nullopt;
    }
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        setError(errorMessage,
                 QStringLiteral("Recovery JSON is invalid: %1").arg(parseError.errorString()));
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("schemaVersion")).toInt(-1) != kSchemaVersion)
    {
        setError(errorMessage, QStringLiteral("Recovery schema is unsupported."));
        return std::nullopt;
    }

    ClientRecoveryState state;
    state.schemaVersion = kSchemaVersion;
    state.sessionId = object.value(QStringLiteral("sessionId")).toString().trimmed();
    if (state.sessionId != expectedSessionId)
    {
        setError(errorMessage, QStringLiteral("Recovery session does not match."));
        return std::nullopt;
    }

    state.savedAtMs = static_cast<qint64>(object.value(QStringLiteral("savedAtMs")).toDouble());
    const qint64 ageMs = nowMs - state.savedAtMs;
    if (state.savedAtMs <= 0 || ageMs > maximumRecoveryAgeMs()
        || ageMs < -kMaximumFutureSkewMs)
    {
        setError(errorMessage, QStringLiteral("Recovery state is stale."));
        return std::nullopt;
    }

    const auto windowMode =
        windowModeFromName(object.value(QStringLiteral("windowMode")).toString());
    if (!windowMode.has_value())
    {
        setError(errorMessage, QStringLiteral("Recovery window mode is invalid."));
        return std::nullopt;
    }
    state.windowMode = *windowMode;
    const QByteArray encodedGeometry =
        object.value(QStringLiteral("windowGeometry")).toString().toLatin1();
    const qsizetype maximumEncodedGeometryLength =
        ((maximumWindowGeometryBytes() + 2) / 3) * 4;
    if (encodedGeometry.size() <= maximumEncodedGeometryLength)
    {
        state.windowGeometry = QByteArray::fromBase64(encodedGeometry);
        if (state.windowGeometry.size() > maximumWindowGeometryBytes())
            state.windowGeometry.clear();
    }
    state.windowMaximized = object.value(QStringLiteral("windowMaximized")).toBool(false);
    state.navigationPageId = boundedOptional(
        object.value(QStringLiteral("navigationPageId")).toString(),
        kMaxNavigationPageIdLength);
    state.conversationId = boundedOptional(
        object.value(QStringLiteral("conversationId")).toString(),
        kMaxConversationIdLength);
    state.composerHtml = boundedOptional(
        object.value(QStringLiteral("composerHtml")).toString(),
        maximumComposerHtmlLength());
    state.replyMessageId = boundedOptional(
        object.value(QStringLiteral("replyMessageId")).toString(),
        kMaxConversationIdLength);
    state.replySenderId = boundedOptional(
        object.value(QStringLiteral("replySenderId")).toString(),
        kMaxConversationIdLength);
    state.replySenderName = boundedOptional(
        object.value(QStringLiteral("replySenderName")).toString(),
        maximumContextTextLength());
    state.replyBody = boundedOptional(
        object.value(QStringLiteral("replyBody")).toString(),
        maximumContextTextLength());
    state.editingMessageId = boundedOptional(
        object.value(QStringLiteral("editingMessageId")).toString(),
        kMaxConversationIdLength);
    state.editingBody = boundedOptional(
        object.value(QStringLiteral("editingBody")).toString(),
        maximumContextTextLength());

    setError(errorMessage, QString());
    return state;
}

QString ClientRecoveryStateStore::filePath() const
{
    return m_filePath;
}
