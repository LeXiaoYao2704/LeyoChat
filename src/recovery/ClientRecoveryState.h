#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

enum class RecoveryWindowMode
{
    Visible,
    Minimized,
    TrayHidden,
};

struct ClientRecoveryState
{
    int schemaVersion = 1;
    QString sessionId;
    qint64 savedAtMs = 0;
    RecoveryWindowMode windowMode = RecoveryWindowMode::Visible;
    QByteArray windowGeometry;
    bool windowMaximized = false;
    QString navigationPageId;
    QString conversationId;
    QString composerHtml;
    QString replyMessageId;
    QString replySenderId;
    QString replySenderName;
    QString replyBody;
    QString editingMessageId;
    QString editingBody;

    bool operator==(const ClientRecoveryState&) const = default;
};

class ClientRecoveryStateStore
{
public:
    explicit ClientRecoveryStateStore(QString filePath);

    bool save(const ClientRecoveryState& state, QString* errorMessage = nullptr) const;
    std::optional<ClientRecoveryState> loadForSession(
        const QString& expectedSessionId,
        qint64 nowMs,
        QString* errorMessage = nullptr) const;

    QString filePath() const;

    static constexpr qint64 maximumRecoveryAgeMs() { return 15 * 60 * 1000; }
    static constexpr qint64 snapshotRefreshIntervalMs() { return 5 * 60 * 1000; }
    static constexpr qint64 maximumFileSizeBytes() { return 2 * 1024 * 1024; }
    static constexpr qsizetype maximumWindowGeometryBytes() { return 64 * 1024; }
    static constexpr qsizetype maximumComposerHtmlLength() { return 256 * 1024; }
    static constexpr qsizetype maximumContextTextLength() { return 8 * 1024; }
    static bool shouldPersistSnapshot(bool payloadChanged,
                                      qint64 lastSavedAtMs,
                                      qint64 nowMs);

private:
    QString m_filePath;
};
