#pragma once

#include <QString>

struct ComposerRecoveryContext {
    QString composerHtml;
    QString replyMessageId;
    QString replySenderId;
    QString replySenderName;
    QString replyBody;
    QString editingMessageId;
    QString editingBody;

    bool operator==(const ComposerRecoveryContext&) const = default;
};
