#pragma once

#include "domain/ReminderItem.h"

#include <QDateTime>
#include <QString>

#include <optional>

std::optional<ReminderItem> makeMessageReminderItem(const QString& messageId,
                                                    const QString& conversationId,
                                                    const QString& titleSnapshot,
                                                    const QString& previewSnapshot,
                                                    const QString& note,
                                                    qint64 dueAtMs,
                                                    const QDateTime& now);

std::optional<ReminderItem> makeContactReminderItem(const QString& contactId,
                                                    const QString& displayName,
                                                    const QString& previewSnapshot,
                                                    const QDateTime& now);

std::optional<ReminderItem> makeGroupAnnouncementReminderItem(const QString& groupId,
                                                              const QString& groupName,
                                                              const QString& announcement,
                                                              const QString& note,
                                                              qint64 dueAtMs,
                                                              const QDateTime& now);

std::optional<ReminderItem> makeGroupFileReminderItem(const QString& groupId,
                                                      const QString& resourceId,
                                                      const QString& fileName,
                                                      const QString& previewSnapshot,
                                                      const QString& note,
                                                      qint64 dueAtMs,
                                                      const QDateTime& now);
