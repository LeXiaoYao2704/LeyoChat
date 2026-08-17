#include "app/MessagePresentationHelpers.h"

#include "architecture/ResourceReferenceMessage.h"

#include <algorithm>

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextDocument>

QString filePreviewText(const QString& fileName) {
    return fileName.trimmed().isEmpty() ? QStringLiteral("[File]")
                                        : QStringLiteral("[File] %1").arg(fileName.trimmed());
}

QString sanitizeNotificationText(const QString& text, const QString& fallback)
{
    QString plain = text.trimmed();
    if (plain.contains(QLatin1Char('<'))) {
        QTextDocument doc;
        doc.setHtml(plain);
        plain = doc.toPlainText().trimmed();
    }
    plain.replace(QRegularExpression(QStringLiteral("[\\r\\n\\t]+")), QStringLiteral(" "));
    plain = plain.simplified();
    if (plain.isEmpty()) {
        return fallback;
    }

    static const QRegularExpression guidPattern(
        QStringLiteral(R"(^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$)"));
    if (guidPattern.match(plain).hasMatch()) {
        return fallback;
    }
    if (plain.contains(QLatin1Char('|'))
        || plain.startsWith(QStringLiteral("group:"), Qt::CaseInsensitive)
        || plain.startsWith(QStringLiteral("direct:"), Qt::CaseInsensitive)) {
        return fallback;
    }
    return plain;
}

QString humanReadableBytes(qint64 bytes) {
    constexpr qint64 kKilobyte = 1024;
    constexpr qint64 kMegabyte = 1024 * 1024;
    constexpr qint64 kGigabyte = 1024 * 1024 * 1024;

    if (bytes >= kGigabyte) {
        return QStringLiteral("%1 GB").arg(QString::number(static_cast<double>(bytes) / kGigabyte, 'f', 1));
    }
    if (bytes >= kMegabyte) {
        return QStringLiteral("%1 MB").arg(QString::number(static_cast<double>(bytes) / kMegabyte, 'f', 1));
    }
    if (bytes >= kKilobyte) {
        return QStringLiteral("%1 KB").arg(QString::number(static_cast<double>(bytes) / kKilobyte, 'f', 1));
    }
    return QStringLiteral("%1 B").arg(QString::number(bytes));
}

QString fileTransferProgressText(qint64 bytesCompleted, qint64 fileSize) {
    if (fileSize <= 0) {
        return QString();
    }

    const qint64 clampedCompleted = std::clamp(bytesCompleted, 0LL, fileSize);
    const int percent = std::clamp(
        static_cast<int>((static_cast<long double>(clampedCompleted) * 100.0L)
                         / static_cast<long double>(fileSize)),
        0,
        100);
    return QStringLiteral("%1%, %2 / %3")
        .arg(QString::number(percent), humanReadableBytes(clampedCompleted), humanReadableBytes(fileSize));
}

QString fileTransferStatusText(const QString& fileName,
                               FileTransferState state,
                               FileTransferDirection direction,
                               qint64 bytesCompleted,
                               qint64 fileSize) {
    const QString preview = filePreviewText(fileName);
    const QString progressText = fileTransferProgressText(bytesCompleted, fileSize);
    switch (state) {
    case FileTransferState::PendingOffer:
    case FileTransferState::WaitingAccept:
        return QStringLiteral("%1 (\u7B49\u5F85\u5BF9\u65B9\u63A5\u53D7)").arg(preview);
    case FileTransferState::ReadyToTransfer:
        return direction == FileTransferDirection::Incoming
                   ? QStringLiteral("%1 (\u51C6\u5907\u63A5\u6536)").arg(preview)
                   : QStringLiteral("%1 (\u51C6\u5907\u53D1\u9001)").arg(preview);
    case FileTransferState::Transferring:
        return direction == FileTransferDirection::Incoming
                   ? (progressText.isEmpty()
                          ? QStringLiteral("%1 (\u63A5\u6536\u4E2D)").arg(preview)
                          : QStringLiteral("%1 (\u63A5\u6536\u4E2D %2)").arg(preview, progressText))
                   : (progressText.isEmpty()
                          ? QStringLiteral("%1 (\u53D1\u9001\u4E2D)").arg(preview)
                          : QStringLiteral("%1 (\u53D1\u9001\u4E2D %2)").arg(preview, progressText));
    case FileTransferState::Completing:
        return direction == FileTransferDirection::Incoming
                   ? (progressText.isEmpty()
                          ? QStringLiteral("%1 (\u6B63\u5728\u6574\u7406\u6587\u4EF6)").arg(preview)
                          : QStringLiteral("%1 (\u6B63\u5728\u6574\u7406\u6587\u4EF6 %2)").arg(preview, progressText))
                   : (progressText.isEmpty()
                          ? QStringLiteral("%1 (\u7B49\u5F85\u5BF9\u65B9\u786E\u8BA4)").arg(preview)
                          : QStringLiteral("%1 (\u7B49\u5F85\u5BF9\u65B9\u786E\u8BA4 %2)").arg(preview, progressText));
    case FileTransferState::Paused:
    case FileTransferState::Interrupted:
        return progressText.isEmpty()
                   ? QStringLiteral("%1 (\u7B49\u5F85\u7EED\u4F20)").arg(preview)
                   : QStringLiteral("%1 (\u7B49\u5F85\u7EED\u4F20 %2)").arg(preview, progressText);
    case FileTransferState::Failed:
        return QStringLiteral("%1 (\u4F20\u8F93\u5931\u8D25)").arg(preview);
    case FileTransferState::Canceled:
        return QStringLiteral("%1 (\u5DF2\u53D6\u6D88)").arg(preview);
    case FileTransferState::Completed:
        return preview;
    }

    return preview;
}

qint64 chunkBytesForTask(const FileTransferTask& task, int chunkIndex) {
    if (task.fileSize <= 0 || task.chunkSize <= 0 || chunkIndex < 0) {
        return 0;
    }

    const qint64 chunkOffset = static_cast<qint64>(chunkIndex) * task.chunkSize;
    if (chunkOffset >= task.fileSize) {
        return 0;
    }

    return std::min(task.chunkSize, task.fileSize - chunkOffset);
}

qint64 completedBytesForTask(const FileTransferTask& task, const std::vector<int>& completedChunks) {
    qint64 bytesCompleted = 0;
    for (const int chunkIndex : completedChunks) {
        bytesCompleted += chunkBytesForTask(task, chunkIndex);
    }
    return bytesCompleted;
}

qint64 displayBytesForTransferState(const FileTransferTask& task, FileTransferState state) {
    if (state == FileTransferState::Completed) {
        return task.fileSize;
    }
    if (state == FileTransferState::Completing && task.fileSize > 0) {
        return std::clamp(task.bytesCompleted, 0LL, task.fileSize - 1);
    }
    return task.bytesCompleted;
}

QString transferPeerLabel(const FileTransferTask& task, const QString& peerDisplayName) {
    return peerDisplayName.trimmed().isEmpty()
               ? QString::fromStdWString(task.peerClientId)
               : peerDisplayName.trimmed();
}

QString transferStatusChipText(const FileTransferTask& task) {
    switch (task.state) {
    case FileTransferState::PendingOffer:
    case FileTransferState::WaitingAccept:
        return QStringLiteral("\u7B49\u5F85\u63A5\u53D7");
    case FileTransferState::ReadyToTransfer:
        return task.direction == FileTransferDirection::Incoming
                   ? QStringLiteral("\u51C6\u5907\u63A5\u6536")
                   : QStringLiteral("\u51C6\u5907\u53D1\u9001");
    case FileTransferState::Transferring:
        return task.direction == FileTransferDirection::Incoming
                   ? QStringLiteral("\u63A5\u6536\u4E2D")
                   : QStringLiteral("\u53D1\u9001\u4E2D");
    case FileTransferState::Paused:
    case FileTransferState::Interrupted:
        return QStringLiteral("\u5F85\u7EED\u4F20");
    case FileTransferState::Completing:
        return task.direction == FileTransferDirection::Incoming
                   ? QStringLiteral("\u6B63\u5728\u6574\u7406")
                   : QStringLiteral("\u7B49\u5F85\u786E\u8BA4");
    case FileTransferState::Completed:
        return QStringLiteral("\u5DF2\u5B8C\u6210");
    case FileTransferState::Failed:
        return QStringLiteral("\u5DF2\u5931\u8D25");
    case FileTransferState::Canceled:
        return QStringLiteral("\u5DF2\u53D6\u6D88");
    }

    return QStringLiteral("\u672A\u77E5\u72B6\u6001");
}

QString transferDetailText(const FileTransferTask& task, const QString& peerDisplayName) {
    const QString peerLabel = transferPeerLabel(task, peerDisplayName);
    const QString directionLabel = task.direction == FileTransferDirection::Outgoing
                                       ? QStringLiteral("\u53D1\u7ED9 %1").arg(peerLabel)
                                       : QStringLiteral("\u6765\u81EA %1").arg(peerLabel);
    const QString progressText =
        fileTransferProgressText(displayBytesForTransferState(task, task.state), task.fileSize);
    if (progressText.isEmpty()) {
        return directionLabel;
    }
    return QStringLiteral("%1 \u00B7 %2").arg(directionLabel, progressText);
}

bool transferTaskRetryable(const FileTransferTask& task) {
    switch (task.state) {
    case FileTransferState::PendingOffer:
    case FileTransferState::WaitingAccept:
    case FileTransferState::ReadyToTransfer:
    case FileTransferState::Paused:
    case FileTransferState::Interrupted:
    case FileTransferState::Failed:
    case FileTransferState::Canceled:
        return true;
    case FileTransferState::Completing:
    case FileTransferState::Transferring:
    case FileTransferState::Completed:
        return false;
    }

    return false;
}

QString localFilePathForTransferTask(const FileTransferTask& task) {
    const QString sourcePath = QString::fromStdWString(task.sourcePath);
    const QString targetPath = QString::fromStdWString(task.targetPath);
    const QString tempPath = QString::fromStdWString(task.tempPath);

    if (task.direction == FileTransferDirection::Outgoing) {
        return sourcePath;
    }
    if (task.state == FileTransferState::Completed && !targetPath.trimmed().isEmpty()) {
        return targetPath;
    }
    return tempPath;
}

MessageDeliveryState effectiveFileTransferDeliveryState(
    FileTransferDirection direction,
    bool conversationIsOpen,
    MessageDeliveryState requestedState) {
    // Viewing a conversation locally only consumes incoming content. An
    // outgoing file/image must wait for the recipient's real read receipt.
    // Preserve failures as failures even when an incoming conversation is open.
    if (requestedState == MessageDeliveryState::Failed) {
        return requestedState;
    }
    if (direction == FileTransferDirection::Incoming && conversationIsOpen) {
        return MessageDeliveryState::Read;
    }
    return requestedState;
}

QString groupEnvelopePreviewText(const MessageEnvelope& envelope) {
    const QJsonObject bodyJson =
        QJsonDocument::fromJson(QByteArray::fromStdString(envelope.body)).object();
    if (envelope.type == MessageType::ResourceReference
        || bodyJson.value(QStringLiteral("message_kind")).toString() == QStringLiteral("resource_reference")) {
        const auto payload = parseResourceReferenceEnvelope(envelope);
        const QString resourceLabel = payload.has_value()
                                          ? (!payload->resource.title.trimmed().isEmpty()
                                                 ? payload->resource.title.trimmed()
                                                 : payload->resource.resourceId.trimmed())
                                          : QString();
        return resourceLabel.isEmpty()
                   ? QStringLiteral("[鍏变韩璧勬簮]")
                   : QStringLiteral("[鍏变韩璧勬簮] %1").arg(resourceLabel);
    }

    if (!bodyJson.isEmpty()) {
        const QString preview = bodyJson.value(QStringLiteral("text")).toString().trimmed();
        if (!preview.isEmpty()) {
            return preview;
        }
        const QString attachmentName = bodyJson.value(QStringLiteral("attachment_name")).toString().trimmed();
        if (!attachmentName.isEmpty()) {
            return QStringLiteral("[鍥剧墖] %1").arg(attachmentName);
        }
    }

    return QString::fromUtf8(envelope.body.data(), static_cast<int>(envelope.body.size())).trimmed();
}
