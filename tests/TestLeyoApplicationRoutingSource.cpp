#include <QtTest/QTest>

#include <QFile>
#include <QRegularExpression>
#include <QString>

class TestLeyoApplicationRoutingSource : public QObject {
    Q_OBJECT

private slots:
    void groupNonImageFileSendDoesNotUseLegacyP2PFanOutPath()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        QVERIFY2(text.contains(QStringLiteral("ReliableGroupFileMessageSender")),
                 "LeyoApplication should route group file cards through the reliable sender");
        QVERIFY2(!text.contains(QStringLiteral("uploaderName_p2p")),
                 "normal group file send must not keep the old hand-written P2P sender");
        QVERIFY2(!text.contains(QStringLiteral("[group-file-offer]")),
                 "normal group file send must not keep the old group-file-offer path");
    }

    void p2pGroupFanOutClearsPendingAfterSuccessfulDelivery()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        QVERIFY2(text.count(QStringLiteral("deliverGroupFanOutPayloads(")) >= 5,
                 "group fan-out deliveries should use the shared delivery helper");
        QVERIFY2(text.count(QStringLiteral("enablePendingCleanupForDelivery(&deliveryOptions);")) >= 5,
                 "every shared group fan-out delivery must configure pending cleanup on success");
        QVERIFY2(text.contains(QStringLiteral("clearDeliveredGroupFanOutPending(")),
                 "application should centralize pending cleanup for successful fan-out sends");
        QVERIFY2(text.contains(QStringLiteral("deletePendingGroupEnvelopeForTargetMessage(")),
                 "successful direct fan-out delivery should delete the matching pending row");

        const qsizetype reconnectBegin =
            text.indexOf(QStringLiteral("const auto flushPendingGroupEnvelopesForTarget"));
        QVERIFY2(reconnectBegin >= 0, "paged pending group resend helper must exist");
        const qsizetype reconnectEnd =
            text.indexOf(QStringLiteral("const auto requestFileTransferResumeForTarget"), reconnectBegin);
        QVERIFY2(reconnectEnd > reconnectBegin, "pending group resend helper should be bounded");
        const QString reconnectBlock = text.mid(reconnectBegin, reconnectEnd - reconnectBegin);
        QVERIFY2(reconnectBlock.contains(QStringLiteral("loadPendingGroupEnvelopesAfterId(")),
                 "pending group resend must page beyond the first 200 rows");
        QVERIFY2(reconnectBlock.contains(QStringLiteral("peerDeliveryReceiptCapabilities.value(normalizedTargetId, false)")),
                 "reconnected receipt-capable peers must be detected before pending cleanup");
        QVERIFY2(reconnectBlock.contains(QStringLiteral("retaining envelope until delivery receipt")),
                 "reconnected receipt-capable peers must retain pending rows after socket write");
        QVERIFY2(reconnectBlock.contains(QStringLiteral("deleteIds.append(pending.id)")),
                 "legacy peers must clear rows only after a successful socket write");
        QVERIFY2(!reconnectBlock.contains(QStringLiteral("kMaxPendingAgeMs")),
                 "unconfirmed group messages must not be silently deleted by age");
    }

    void groupFanOutUsesDeliveryReceiptCapabilityForPendingCleanup()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        QVERIFY2(text.contains(QStringLiteral("p2pDeliveryReceiptV1()")),
                 "HELLO capabilities must advertise the delivery receipt capability");
        QVERIFY2(text.contains(QStringLiteral("hasP2PDeliveryReceiptV1")),
                 "peer HELLO handling must record delivery receipt support");
        QVERIFY2(text.contains(QStringLiteral("retaining pending envelope until delivery receipt")),
                 "receipt-capable peers must retain pending group envelopes after socket write");
    }

    void persistedOutboxSweepRetriesDirectAndMixedGroupMessages()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        QVERIFY2(text.contains(QStringLiteral("pendingDeliveryTimer->setInterval(10000)")),
                 "a live client must retry durable outbox rows without requiring restart");
        QVERIFY2(text.contains(QStringLiteral("retryPendingDirectMessagesForAvailableRoute(targetId)")),
                 "the durable outbox sweep must retry pending direct messages through current routing");
        QVERIFY2(text.contains(QStringLiteral("flushPendingGroupEnvelopesForTarget(")),
                 "the durable outbox sweep must retry connected P2P group recipients");
        QVERIFY2(text.contains(QStringLiteral("retryPendingGroupEnvelopesViaMessageService()")),
                 "the durable outbox sweep must retry server-capable group recipients");
        QVERIFY2(text.contains(QStringLiteral("pendingDirectRetryNotBeforeMs")),
                 "each failed direct outbox row must have an in-memory retry deadline");
        QVERIFY2(text.contains(QStringLiteral("reliableDirectMessageRetryDelayMs")),
                 "direct outbox retries must use bounded exponential backoff");
        QVERIFY2(text.count(QStringLiteral("requireP2PDeliveryReceipt = true")) >= 3,
                 "direct P2P sends and retries must stay pending until the receiver persists them");

        const qsizetype reconnectBegin =
            text.indexOf(QStringLiteral("const auto flushPendingMessagesForTarget"));
        QVERIFY2(reconnectBegin >= 0, "P2P reconnect direct-message flush must exist");
        const qsizetype reconnectEnd =
            text.indexOf(QStringLiteral("const auto requestFileTransferResumeForTarget"),
                         reconnectBegin);
        QVERIFY2(reconnectEnd > reconnectBegin,
                 "P2P reconnect direct-message flush should be bounded");
        const QString reconnectBlock =
            text.mid(reconnectBegin, reconnectEnd - reconnectBegin);
        QVERIFY2(reconnectBlock.contains(QStringLiteral("message.messageType != L\"text\""))
                     && reconnectBlock.contains(QStringLiteral("!message.attachmentName.empty()")),
                 "P2P reconnect must not feed legacy attachment rows into the direct-text sender");
    }

    void mergedForwardPackageUsesReliableServerAwareRouting()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("MainWindow::mergedForwardPackageRequested"));
        QVERIFY2(begin >= 0, "merged forward package handler must exist");
        const qsizetype end =
            text.indexOf(QStringLiteral("MainWindow::editSaveRequested"), begin);
        QVERIFY2(end > begin, "merged forward package handler block should be bounded");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QStringLiteral("sendPersistedGroupEnvelopes(QStringLiteral(\"group-forward\")")),
                 "group merged-forward must route through the reliable group envelope sender");
        QVERIFY2(block.contains(QStringLiteral("sendDirectEnvelope(QStringLiteral(\"direct-forward\")")),
                 "direct merged-forward must route through the reliable direct envelope sender");
        QVERIFY2(!block.contains(QStringLiteral("conn->sendPayload(blob)")),
                 "group merged-forward must not keep the hand-written P2P fan-out loop");
        QVERIFY2(!block.contains(QStringLiteral("connection->sendPayload(")),
                 "direct merged-forward must not bypass the reliable direct envelope sender");
    }

    void incomingReadReceiptsRequireForegroundVisibleConversation()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype helperBegin =
            text.indexOf(QStringLiteral("const auto flushReadReceiptsForIncomingConversations"));
        QVERIFY2(helperBegin >= 0,
                 "incoming read receipts must use a shared foreground-visibility gate");
        const qsizetype helperEnd =
            text.indexOf(QStringLiteral("const auto pendingMessageCountForTarget"), helperBegin);
        QVERIFY2(helperEnd > helperBegin, "incoming read-receipt helper should be bounded");
        const QString helperBlock = text.mid(helperBegin, helperEnd - helperBegin);
        QVERIFY2(helperBlock.contains(QStringLiteral("windowCanConsumeIncomingConversation()")),
                 "incoming messages must not be consumed while minimized, hidden, or inactive");
        QVERIFY2(helperBlock.contains(QStringLiteral("conversationIds.contains(currentConversationId)")),
                 "only the currently visible synced conversation may be consumed");

        const qsizetype viewportBegin =
            text.indexOf(QStringLiteral("MainWindow::viewportReachedBottom"));
        QVERIFY2(viewportBegin >= 0, "viewport read-receipt connection must exist");
        const qsizetype viewportEnd =
            text.indexOf(QStringLiteral("const auto pendingMessageCountForTarget"), viewportBegin);
        QVERIFY2(viewportEnd > viewportBegin, "viewport read-receipt connection should be bounded");
        const QString viewportBlock = text.mid(viewportBegin, viewportEnd - viewportBegin);
        QVERIFY2(viewportBlock.contains(QStringLiteral("windowCanConsumeIncomingConversation()")),
                 "viewport signals must not mark messages read while the window is in the background");

        const qsizetype begin =
            text.indexOf(QStringLiteral("const RemoteMessageEventConsumerResult eventResult"));
        QVERIFY2(begin >= 0, "remote event consumer result block must exist");
        const qsizetype end =
            text.indexOf(QStringLiteral("qWarning().noquote() << \"[remote-message-events] failed reason=\""),
                         begin);
        QVERIFY2(end > begin, "remote event success block should be bounded by the failure log");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QRegularExpression(QStringLiteral(
                     R"(flushReadReceiptsForIncomingConversations\s*\(\s*eventResult\.newIncomingConversationIds\s*\))"))),
                 "remote event sync must consume only newly stored incoming messages through the shared gate");

        QVERIFY2(text.contains(QStringLiteral(
                     "flushReadReceiptsForIncomingConversations(result.newIncomingConversationIds)")),
                 "conversation-poll fallback must use the same foreground read-receipt gate");

        const qsizetype directReceiveBegin =
            text.indexOf(QStringLiteral("bool viewingIncomingConversation"));
        QVERIFY2(directReceiveBegin >= 0, "direct P2P receive block must exist");
        const qsizetype directReceiveEnd =
            text.indexOf(QStringLiteral("QObject::connect(&peerServer"), directReceiveBegin);
        QVERIFY2(directReceiveEnd > directReceiveBegin, "direct P2P receive block should be bounded");
        const QString directReceiveBlock =
            text.mid(directReceiveBegin, directReceiveEnd - directReceiveBegin);
        QVERIFY2(directReceiveBlock.contains(QRegularExpression(QStringLiteral(
                     R"(flushReadReceiptsForIncomingConversations\s*\(\s*QStringList\{conversationId\}\s*\))"))),
                 "direct P2P receive must not bypass the foreground read-receipt gate");
    }

    void remoteMessageSyncNotifiesForNewIncomingConversations()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype helperBegin =
            text.indexOf(QStringLiteral("const auto processRemoteSyncedNotifications"));
        QVERIFY2(helperBegin >= 0,
                 "remote message sync must have a shared structured-notification helper");
        const qsizetype helperEnd =
            text.indexOf(QStringLiteral("QTimer remoteMessageSyncDebounceTimer"), helperBegin);
        QVERIFY2(helperEnd > helperBegin,
                 "remote message notification helper should be bounded before sync timers");
        const QString helperBlock = text.mid(helperBegin, helperEnd - helperBegin);
        QVERIFY2(helperBlock.contains(QStringLiteral("notifyUnreadActivity(")),
                 "remote sync notification helper must route through taskbar/tray alerts");
        QVERIFY2(helperBlock.contains(QStringLiteral(
                     "for (const IncomingMessageNotificationEvent& notification : notifications)")),
                 "remote sync must preserve each newly stored message instead of deduplicating by conversation");
        QVERIFY2(helperBlock.contains(QStringLiteral("currentConversationId == conversationId")),
                 "the visible active conversation must not produce a redundant alert");
        QVERIFY2(helperBlock.contains(QStringLiteral(
                     "notification.messageType == QStringLiteral(\"nudge\")")),
                 "service nudges must keep their dedicated presentation semantics");
        QVERIFY2(helperBlock.contains(QStringLiteral("restoreMainWindow();"))
                     && helperBlock.contains(QStringLiteral("shakeMainWindow();")),
                 "service nudges must restore and shake the window like P2P nudges");
        QVERIFY2(helperBlock.contains(QStringLiteral(
                     "SharedFileResourceSync::syncIncomingSharedFileResource(")),
                 "service group file cards must update the Stage2 shared-resource directory");

        QVERIFY2(text.contains(QStringLiteral(
                     "processRemoteSyncedNotifications(eventResult.newIncomingNotifications)")),
                 "event-stream sync must process each newly stored incoming message");
        QVERIFY2(text.contains(QStringLiteral(
                     "processRemoteSyncedNotifications(result.newIncomingNotifications)")),
                 "conversation-poll fallback must process each newly stored incoming message");
    }

    void remoteMessageSyncProductionPolicyCannotBeBlockedByLegacyConversations()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("QTimer remoteMessageSyncDebounceTimer"));
        QVERIFY2(begin >= 0, "remote message sync timer block must exist");
        const qsizetype end =
            text.indexOf(QStringLiteral("// \u901a\u8bdd\u7ed3\u675f\u65f6"), begin);
        QVERIFY2(end > begin, "remote message sync block should be bounded");
        const QString block = text.mid(begin, end - begin);

        QVERIFY2(block.contains(QStringLiteral(
                     "remoteMessageSyncRetryTimer.isActive()")),
                 "periodic and peer-hello sync triggers must respect the active failure backoff");
        QVERIFY2(block.contains(QStringLiteral(
                     "reason != QStringLiteral(\"retry\")")),
                 "the retry timer must be the only automatic trigger allowed through backoff");
        QVERIFY2(block.contains(QStringLiteral(
                     "remoteMessageReconciliationIntervalMs")),
                 "a successful event poll must still periodically reconcile conversation cursors");
        QVERIFY2(block.contains(QStringLiteral(
                     "serverConversations.has_value()")),
                 "conversation reconciliation must distinguish an authoritative server list from fallback");
        QVERIFY2(block.contains(QStringLiteral(
                     "conversationIds = serverConversationIds")),
                 "a successful server list must exclude stale local-only P2P conversations");
        QVERIFY2(block.contains(QStringLiteral(
                     "localFallbackConversationIds")),
                 "local conversations should remain available only when listing server conversations fails");
        QVERIFY2(block.contains(QRegularExpression(QStringLiteral(
                     R"(RemoteMessageSyncCoordinator\s+coordinator\s*\(\s*localClientId\s*,\s*&conversationRepository\s*,\s*&serverClient\s*,\s*100\s*,\s*false\s*,)"))),
                 "one inaccessible conversation must not stop later conversations from syncing");
    }

    void remoteMessageSessionHeartbeatIsIndependentFromEventConsumption()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("QTimer remoteMessageHeartbeatTimer"));
        QVERIFY2(begin >= 0,
                 "the application must own a heartbeat timer independent of event consumption");
        const qsizetype end =
            text.indexOf(QStringLiteral("QTimer remoteMessageSyncDebounceTimer"), begin);
        QVERIFY2(end > begin,
                 "the independent heartbeat block should precede sync polling timers");
        const QString block = text.mid(begin, end - begin);

        QVERIFY2(block.contains(QRegularExpression(QStringLiteral(
                     R"(loadRemoteMessageEventCursor\s*\(\s*settings\.workspaceId\s*,\s*remoteMessageDeviceId\s*\))"))),
                 "independent heartbeat must report the durable event cursor");
        QVERIFY2(block.contains(QStringLiteral(
                     "sendSessionHeartbeat(settings.workspaceId")),
                 "independent heartbeat must publish the client session even when event polling fails");
        QVERIFY2(block.contains(QStringLiteral("localAppVersion"))
                     && block.contains(QStringLiteral("localRoutingCapabilities")),
                 "independent heartbeat must continuously publish version and routing capabilities");
        QVERIFY2(block.contains(QStringLiteral(
                     "remoteMessageHeartbeatTimer.setInterval(30000)")),
                 "heartbeat must refresh well before the service expires the session");
    }

    void stickerSendRejectsOversizedBinaryBeforeAnyTransport()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("MainWindow::stickerSendRequested"));
        QVERIFY2(begin >= 0, "sticker send handler must exist");
        const qsizetype end =
            text.indexOf(QStringLiteral("const auto eligibilityErrorText"), begin);
        QVERIFY2(end > begin, "sticker send handler should be bounded");
        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QStringLiteral("kMaxStickerBinaryBytes")),
                 "sticker sends must have an explicit binary-size ceiling");
        QVERIFY2(block.contains(QStringLiteral("gifData.size() > kMaxStickerBinaryBytes")),
                 "oversized sticker data must be rejected before P2P or message-service routing");
    }

    void p2pFileReceiveFailuresNotifySender()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype helperBegin =
            text.indexOf(QStringLiteral("const auto notifyFileTransferFailureToSender"));
        QVERIFY2(helperBegin >= 0, "file receive failures should have a shared sender notification helper");
        const qsizetype helperEnd = text.indexOf(QStringLiteral("const auto finalizeIncomingTask"), helperBegin);
        QVERIFY2(helperEnd > helperBegin, "file failure notification helper block should be bounded");

        const QString helperBlock = text.mid(helperBegin, helperEnd - helperBegin);
        QVERIFY2(helperBlock.contains(QStringLiteral("FileControlType::Fail")),
                 "sender notification helper must build a file-transfer Fail control packet");

        const qsizetype begin = text.indexOf(QStringLiteral("FileReceiveWorker::chunkFailed"));
        QVERIFY2(begin >= 0, "file receive worker failure handler must exist");
        const qsizetype end = text.indexOf(QStringLiteral("const auto handleIncomingChunk"), begin);
        QVERIFY2(end > begin, "file receive worker failure handler block should be bounded");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QStringLiteral("markTaskState")),
                 "chunk failures must update the persisted transfer task state");
        QVERIFY2(block.contains(QStringLiteral("chunk_failed")),
                 "chunk failures should record a specific file-transfer error code");
        QVERIFY2(block.contains(QStringLiteral("notifyFileTransferFailureToSender(")),
                 "chunk failures should use the shared sender failure notification helper");
    }

    void p2pFileFinalizeRejectsSizeMismatch()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin = text.indexOf(QStringLiteral("const auto finalizeIncomingTask"));
        QVERIFY2(begin >= 0, "file finalize helper must exist");
        const qsizetype end = text.indexOf(QStringLiteral("if (QFileInfo::exists(targetPath))"), begin);
        QVERIFY2(end > begin, "file finalize mismatch block should be bounded before rename");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QStringLiteral("FileTransferState::Failed")),
                 "size-mismatched temp files must fail before rename");
        QVERIFY2(block.contains(QStringLiteral("notifyFileTransferFailureToSender(")),
                 "size mismatch must notify the sender instead of leaving it waiting for Complete");
        QVERIFY2(block.contains(QStringLiteral("size_mismatch")),
                 "size mismatch should record a specific file-transfer error code");
    }

    void p2pOutgoingCompletingHasTimeoutGuard()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("fileTransferService.markTaskState(taskId"));
        QVERIFY2(begin >= 0, "outgoing data pump should mark file task states");
        const qsizetype completingBegin =
            text.indexOf(QStringLiteral("FileTransferState::Completing"), begin);
        QVERIFY2(completingBegin > begin, "outgoing pump should enter Completing after sending all chunks");
        const qsizetype end =
            text.indexOf(QStringLiteral("const auto scheduleOutgoingOfferRetry"), completingBegin);
        QVERIFY2(end > completingBegin, "Completing block should be bounded by the retry scheduler");

        const QString block = text.mid(completingBegin, end - completingBegin);
        QVERIFY2(block.contains(QStringLiteral("complete_timeout")),
                 "outgoing Completing state must time out instead of waiting forever for Complete/Fail");
        QVERIFY2(block.contains(QStringLiteral("QTimer::singleShot")),
                 "outgoing Completing state should schedule a timeout guard");
        QVERIFY2(block.contains(QStringLiteral("FileTransferState::Interrupted")),
                 "timeout guard should return the transfer to a retryable interrupted state");
    }

    void p2pOutgoingPumpDoesNotTreatSocketWritesAsRemoteCompletion()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("const auto startOutgoingDataTransfer"));
        QVERIFY2(begin >= 0, "outgoing data pump helper must exist");
        const qsizetype end =
            text.indexOf(QStringLiteral("const auto scheduleOutgoingOfferRetry"), begin);
        QVERIFY2(end > begin, "outgoing data pump should be bounded by the retry scheduler");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(!block.contains(QStringLiteral("completedChunkSet->insert(preparedChunk.chunkIndex);")),
                 "socket writes must not be recorded as remotely completed chunks");
        QVERIFY2(block.contains(QStringLiteral("dispatchedChunkSet->insert(preparedChunk.chunkIndex);")),
                 "outgoing pump should track locally dispatched chunks separately from remote progress");

        const QRegularExpression completingUsesFileSizePattern(
            QStringLiteral("FileTransferState::Completing\\s*,\\s*task\\.fileSize"));
        QVERIFY2(!completingUsesFileSizePattern.match(block).hasMatch(),
                 "Completing progress must use receiver-acknowledged bytes, not local file size");
        QVERIFY2(block.contains(QStringLiteral("remoteCompletedChunkVector")),
                 "Completing progress should be based on latest receiver progress from storage");
    }

    void p2pOutgoingRemoteCompleteProgressStillWaitsForCompleteAck()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("if (task.chunkCount > 0 && completedChunks.size() >= task.chunkCount)"));
        QVERIFY2(begin >= 0, "outgoing resume should check receiver completed chunk progress");
        const qsizetype end =
            text.indexOf(QStringLiteral("fileTransferService.markTaskState(taskId"),
                         begin + 1);
        QVERIFY2(end > begin, "receiver completed progress block should be bounded before normal transfer start");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(!block.contains(QStringLiteral("FileTransferState::Completed")),
                 "receiver progress alone must not mark the sender task Completed before Complete/Fail");
        QVERIFY2(block.contains(QStringLiteral("markOutgoingAwaitingCompletion")),
                 "all receiver chunks should enter the same completion-ack wait path with timeout guard");
    }

    void remoteEventSyncRefreshesUiForSessionSnapshot()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("const RemoteMessageEventConsumerResult eventResult"));
        QVERIFY2(begin >= 0, "remote event consumer result block must exist");
        const qsizetype end =
            text.indexOf(QStringLiteral("qWarning().noquote() << \"[remote-message-events] failed reason=\""),
                         begin);
        QVERIFY2(end > begin, "remote event success block should be bounded by the failure log");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QStringLiteral("eventResult.sessionsSynced > 0")),
                 "remote session snapshots must refresh online status UI even when no messages were synced");
        QVERIFY2(block.contains(QStringLiteral("scheduleChatUiRefresh(true, true, true, true, 0, true);")),
                 "remote session snapshots must refresh conversations, header, selection, and contacts");
    }

    void mixedVersionP2PKeepsDuplicateConnectionsForLegacyPeers()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("const auto duplicateAction"));
        QVERIFY2(begin >= 0, "duplicate peer arbitration must use the shared policy helper");
        const qsizetype end =
            text.indexOf(QStringLiteral("const bool repeatedHelloOnRegisteredConnection"), begin);
        QVERIFY2(end > begin, "duplicate peer arbitration block should be bounded");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QStringLiteral("duplicatePeerConnectionAction(")),
                 "HELLO duplicate handling must choose behavior from peer capabilities");
        QVERIFY2(block.contains(QStringLiteral("if (keepBoth && keepExisting)")),
                 "legacy/unknown peers must be able to keep both TCP connections");
        QVERIFY2(block.contains(QStringLiteral("registerHelloConnectionForTarget = false;")),
                 "when keeping both and preferring existing, the new connection must remain non-primary");
        QVERIFY2(block.contains(QStringLiteral("} else if (keepBoth)")),
                 "when keeping both and preferring new, the existing connection must remain alive as non-primary");

        const qsizetype retainedLog =
            text.indexOf(QStringLiteral("retained non-primary legacy connection"));
        QVERIFY2(retainedLog > end,
                 "retained non-primary legacy connections must return after HELLO bookkeeping");
        const qsizetype firstOutboundResend =
            text.indexOf(QStringLiteral("resendPendingFileOffersForTarget"), retainedLog);
        QVERIFY2(firstOutboundResend > retainedLog,
                 "retained non-primary legacy connection guard must appear before outbound resend");
        const QString nonPrimaryGuard =
            text.mid(retainedLog, firstOutboundResend - retainedLog);
        QVERIFY2(nonPrimaryGuard.contains(QStringLiteral("return;")),
                 "retained non-primary legacy connections must not run outbound resend paths");
    }

    void receiptValidationUsesConnectionIdentityFallback()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("if (decoded->type == MessageType::ReceiptReceived)"));
        QVERIFY2(begin >= 0, "receipt receive block must exist");
        const qsizetype end =
            text.indexOf(QStringLiteral("if (receiptSenderId.isEmpty()"), begin);
        QVERIFY2(end > begin, "receipt validation block should be bounded");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QStringLiteral("peerIdsByConnection.value(connection).trimmed()")),
                 "receipts arriving on a retained non-primary legacy connection must still validate against HELLO identity");
    }

    void disconnectDoesNotMarkPeerOfflineWhenRetainedConnectionIsStillActive()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("const QStringList removedTargetIds"));
        QVERIFY2(begin >= 0, "disconnect handler must remove target aliases");
        const qsizetype end =
            text.indexOf(QStringLiteral("scheduleChatUiRefresh(true, false, true, true, 150, contactListChanged);"),
                         begin);
        QVERIFY2(end > begin, "disconnect handler block should be bounded");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QStringLiteral("connectedConnectionForTarget(")),
                 "disconnect handler must resolve the retained connection instance");
        QVERIFY2(block.contains(QStringLiteral("connectionsByTargetId.insert(resolvedTargetId, retainedConnection);")),
                 "disconnect handler must promote the retained connection back into the primary route table");
        QVERIFY2(block.contains(QStringLiteral("disconnect skipped offline mark; promoted retained connection")),
                 "disconnect handler must log when it keeps a peer online through a promoted retained connection");
        QVERIFY2(block.contains(QStringLiteral("interruptFileTransfersForTargets(disconnectedTargetIds);")),
                 "file transfers must only be interrupted for peers with no retained connection");
        QVERIFY2(block.contains(QStringLiteral("disconnectedTargetIds.contains(callPeerId)")),
                 "calls must only hang up when no retained connection remains for the peer");
    }

    void directP2PSendAndReadReceiptsUseRetainedConnectionFallback()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype receiptBegin =
            text.indexOf(QStringLiteral("DatabaseWorker::readReceiptsFlushed"));
        QVERIFY2(receiptBegin >= 0, "read receipt flush handler must exist");
        const qsizetype receiptEnd =
            text.indexOf(QStringLiteral("dbWorkerThread.start();"), receiptBegin);
        QVERIFY2(receiptEnd > receiptBegin, "read receipt flush block should be bounded");
        const QString receiptBlock = text.mid(receiptBegin, receiptEnd - receiptBegin);
        QVERIFY2(receiptBlock.contains(QStringLiteral("connectedConnectionForTarget(connectionsByTargetId")),
                 "direct read receipts must use retained legacy connections when the primary route was removed");

        const qsizetype envelopeBegin =
            text.indexOf(QStringLiteral("const auto sendDirectEnvelope"));
        QVERIFY2(envelopeBegin >= 0, "shared direct envelope sender must exist");
        const qsizetype envelopeEnd =
            text.indexOf(QStringLiteral("const auto sendPersistedGroupEnvelopes"), envelopeBegin);
        QVERIFY2(envelopeEnd > envelopeBegin, "shared direct envelope sender block should be bounded");
        const QString envelopeBlock = text.mid(envelopeBegin, envelopeEnd - envelopeBegin);
        QVERIFY2(envelopeBlock.contains(QStringLiteral("connectedConnectionForTarget(connectionsByTargetId")),
                 "direct envelope sends must use retained legacy connections when the primary route was removed");

        const qsizetype textBegin =
            text.indexOf(QStringLiteral("MainWindow::sendRequested"));
        QVERIFY2(textBegin >= 0, "direct text send handler must exist");
        const qsizetype textEnd =
            text.indexOf(QStringLiteral("MainWindow::stickerSendRequested"), textBegin);
        QVERIFY2(textEnd > textBegin, "direct text send block should be bounded");
        const QString textBlock = text.mid(textBegin, textEnd - textBegin);
        QVERIFY2(textBlock.contains(QStringLiteral("connectedConnectionForTarget(connectionsByTargetId")),
                 "direct text sends must use retained legacy connections when the primary route was removed");
    }

    void directLegacyTargetsPreflightP2PBeforeRouteDecision()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        QVERIFY2(text.contains(QStringLiteral("ensureLegacyP2PConnectionForTarget")),
                 "application should have one shared legacy P2P preflight helper");

        const qsizetype envelopeBegin =
            text.indexOf(QStringLiteral("const auto sendDirectEnvelope"));
        QVERIFY2(envelopeBegin >= 0, "shared direct envelope sender must exist");
        const qsizetype envelopeEnd =
            text.indexOf(QStringLiteral("const auto sendPersistedGroupEnvelopes"),
                         envelopeBegin);
        QVERIFY2(envelopeEnd > envelopeBegin, "shared direct envelope block should be bounded");
        const QString envelopeBlock = text.mid(envelopeBegin, envelopeEnd - envelopeBegin);
        const qsizetype envelopePreflight =
            envelopeBlock.indexOf(QStringLiteral("ensureLegacyP2PConnectionForTarget("));
        const qsizetype envelopeRoute =
            envelopeBlock.indexOf(QStringLiteral("directEnvelopeSender.send(sendRequest)"));
        QVERIFY2(envelopePreflight >= 0,
                 "direct envelopes to legacy peers must start P2P preflight");
        QVERIFY2(envelopeRoute > envelopePreflight,
                 "direct envelope P2P preflight must run before route decision/send");

        const qsizetype textBegin =
            text.indexOf(QStringLiteral("MainWindow::sendRequested"));
        QVERIFY2(textBegin >= 0, "direct text send handler must exist");
        const qsizetype textEnd =
            text.indexOf(QStringLiteral("MainWindow::stickerSendRequested"), textBegin);
        QVERIFY2(textEnd > textBegin, "direct text send block should be bounded");
        const QString directTextBlock = text.mid(textBegin, textEnd - textBegin);
        const qsizetype textPreflight =
            directTextBlock.indexOf(QStringLiteral("ensureLegacyP2PConnectionForTarget("));
        const qsizetype textRoute =
            directTextBlock.indexOf(QStringLiteral("directSender.sendText(sendRequest)"));
        QVERIFY2(textPreflight >= 0,
                 "direct text messages to legacy peers must start P2P preflight");
        QVERIFY2(textRoute > textPreflight,
                 "direct text P2P preflight must run before route decision/send");
    }

    void selectingDirectLegacyConversationPreflightsP2P()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype conversationBegin =
            text.indexOf(QStringLiteral("MainWindow::conversationSelected"));
        QVERIFY2(conversationBegin >= 0, "conversation selection handler must exist");
        const qsizetype conversationEnd =
            text.indexOf(QStringLiteral("MainWindow::contactSelected"),
                         conversationBegin);
        QVERIFY2(conversationEnd > conversationBegin,
                 "conversation selection block should be bounded");
        const QString conversationBlock =
            text.mid(conversationBegin, conversationEnd - conversationBegin);
        QVERIFY2(conversationBlock.contains(
                     QStringLiteral("preflightLegacyP2PForDirectTarget(currentTargetId,")),
                 "opening a direct conversation must preflight legacy P2P for the selected target");

        const qsizetype contactBegin =
            text.indexOf(QStringLiteral("MainWindow::contactSelected"));
        QVERIFY2(contactBegin >= 0, "contact selection handler must exist");
        const qsizetype contactEnd =
            text.indexOf(QStringLiteral("MainWindow::contactProfileRequested"),
                         contactBegin);
        QVERIFY2(contactEnd > contactBegin, "contact selection block should be bounded");
        const QString contactBlock = text.mid(contactBegin, contactEnd - contactBegin);
        QVERIFY2(contactBlock.contains(
                     QStringLiteral("preflightLegacyP2PForDirectTarget(currentTargetId,")),
                 "opening a contact chat must preflight legacy P2P for the selected target");
    }

    void lanDiscoveryPresenceDoesNotRequireTcpForUiOnlineState()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype discoveryBegin =
            text.indexOf(QStringLiteral("LanDiscoveryService::peerDiscovered"));
        QVERIFY2(discoveryBegin >= 0, "LAN discovery handler must exist");
        const qsizetype discoveryEnd =
            text.indexOf(QStringLiteral("const auto recipientIdsFromEnvelopes"),
                         discoveryBegin);
        QVERIFY2(discoveryEnd > discoveryBegin,
                 "LAN discovery handler block should be bounded");
        const QString discoveryBlock =
            text.mid(discoveryBegin, discoveryEnd - discoveryBegin);
        QVERIFY2(discoveryBlock.contains(QStringLiteral("rememberPeer(clientId"))
                     && discoveryBlock.contains(QStringLiteral("PeerPresenceStatus::Online")),
                 "UDP discovery should refresh online presence even before a TCP connection exists");

        const qsizetype refreshBegin =
            text.indexOf(QStringLiteral("const auto refreshConversationList"));
        QVERIFY2(refreshBegin >= 0, "conversation refresh block must exist");
        const qsizetype refreshEnd =
            text.indexOf(QStringLiteral("const auto refreshTransferList"), refreshBegin);
        QVERIFY2(refreshEnd > refreshBegin,
                 "conversation refresh block should be bounded");
        const QString refreshBlock = text.mid(refreshBegin, refreshEnd - refreshBegin);
        QVERIFY2(refreshBlock.contains(
                     QStringLiteral("RemotePresenceUiAdapter::directConversationIdsForOnlinePeers")),
                 "conversation list online dots must include UDP/session presence, not only active TCP routes");
    }

    void directFileSendStartsP2POnDemandAndRefreshesRouteAfterHash()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("MainWindow::fileSendRequested"));
        QVERIFY2(begin >= 0, "direct file send handler must exist");
        const qsizetype end =
            text.indexOf(QStringLiteral("MainWindow::retryPendingRequested"), begin);
        QVERIFY2(end > begin, "direct file send block should be bounded");

        const QString block = text.mid(begin, end - begin);
        const qsizetype preflight = block.indexOf(
            QStringLiteral("preflightDirectFileP2PForTarget(resolvedTargetId"));
        const qsizetype hashStart = block.indexOf(
            QStringLiteral("QFutureWatcher<\n            std::optional<FileTransferService::PreparedOutgoingFile>"));
        QVERIFY2(preflight >= 0,
                 "direct file send must explicitly start P2P even for server-capable peers");
        QVERIFY2(hashStart > preflight,
                 "direct file P2P preflight must run before asynchronous hashing");

        const qsizetype hashFinished = block.indexOf(
            QStringLiteral(
                "QFutureWatcher<std::optional<FileTransferService::PreparedOutgoingFile>>::finished"));
        QVERIFY2(hashFinished > hashStart, "file hash completion callback must exist");
        const QString completionBlock = block.mid(hashFinished);
        QVERIFY2(completionBlock.contains(QStringLiteral("connectedConnectionForTarget(")),
                 "hash completion must resolve the current route instead of using a stale captured pointer");
        QVERIFY2(!completionBlock.contains(QStringLiteral("safeConnection")),
                 "hash completion must not depend on the connection state captured before hashing");
        const qsizetype pendingOffer = completionBlock.indexOf(
            QStringLiteral("FileTransferState::PendingOffer"));
        const qsizetype reconnectAfterPending = completionBlock.indexOf(
            QStringLiteral("preflightDirectFileP2PForTarget(resolvedTargetId"),
            pendingOffer);
        QVERIFY2(pendingOffer >= 0,
                 "a file whose route disconnected during hashing must become PendingOffer");
        QVERIFY2(reconnectAfterPending > pendingOffer,
                 "after persisting PendingOffer, direct file send must reconnect so HELLO can resend it");
    }

    void directMessageRetryUsesPersistedReliableRouting()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("MainWindow::retryMessageRequested"));
        QVERIFY2(begin >= 0, "single-message retry handler must exist");
        const qsizetype end =
            text.indexOf(QStringLiteral("MainWindow::retryTransferRequested"), begin);
        QVERIFY2(end > begin, "single-message retry block should be bounded");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QStringLiteral("directSender.retryText(messageId, sendRequest)")),
                 "single-message retry must reuse the persisted id through reliable routing");
        QVERIFY2(block.contains(QStringLiteral("ensureLegacyP2PConnectionForTarget(")),
                 "a retry without an active legacy P2P route must start the on-demand connection");
        QVERIFY2(!block.contains(QStringLiteral("connection->sendPayload(")),
                 "single-message retry must not bypass reliable routing with a direct socket write");
    }

    void reconnectMessageResendUsesReliablePersistedRetry()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("const auto flushPendingMessagesForTarget"));
        QVERIFY2(begin >= 0, "automatic reconnect message resend helper must exist");
        const qsizetype end =
            text.indexOf(QStringLiteral("const auto requestFileTransferResumeForTarget"), begin);
        QVERIFY2(end > begin, "automatic reconnect message resend helper should be bounded");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QStringLiteral("ReliableDirectMessageSender directSender")),
                 "automatic reconnect resend must use the reliable direct sender");
        QVERIFY2(block.contains(QStringLiteral("directSender.retryText(messageId, sendRequest)")),
                 "automatic reconnect resend must reuse the persisted message id");
        QVERIFY2(!block.contains(QStringLiteral("const bool sent = connection->sendPayload(")),
                 "automatic reconnect resend must not bypass reliable routing with a direct socket write");
    }

    void groupSettingsDialogDoesNotExposeLegacyFileServicePage()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("dlg->setWindowTitle(QStringLiteral(\"\\u7fa4\\u8bbe\\u7f6e\"))"));
        QVERIFY2(begin >= 0, "group settings dialog block must exist");
        const qsizetype end =
            text.indexOf(QStringLiteral("// 单聊历史记录"), begin);
        QVERIFY2(end > begin, "group settings dialog block should be bounded before direct chat history wiring");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(!block.contains(QStringLiteral("groupSettingsFileServicePage")),
                 "group settings dialog must not expose the legacy group file service page");
        QVERIFY2(!block.contains(QStringLiteral("groupSettingsFileServiceCard")),
                 "group settings dialog must not keep the legacy per-group file service form");
        QVERIFY2(!block.contains(QStringLiteral("groupFileServiceSaveRequested(cfg)")),
                 "group settings dialog must not save per-group file service settings anymore");
    }

    void directSendSlotCatchesStdExceptions()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype begin =
            text.indexOf(QStringLiteral("MainWindow::sendRequested"));
        QVERIFY2(begin >= 0, "direct/group send handler must exist");
        const qsizetype end =
            text.indexOf(QStringLiteral("MainWindow::stickerSendRequested"), begin);
        QVERIFY2(end > begin, "send handler block should be bounded before sticker handler");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QStringLiteral("try {")),
                 "sendRequested must not let C++ exceptions propagate through Qt event dispatch");
        QVERIFY2(block.contains(QStringLiteral("catch (const std::exception& e)")),
                 "sendRequested should log std::exception details instead of exiting the app");
        QVERIFY2(block.contains(QStringLiteral("[msg-send] exception in sendRequested handler")),
                 "sendRequested exception guard should leave a searchable production log");
    }

    void databaseWorkerUsesThreadOwnedDeletionOnShutdown()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        QVERIFY2(text.contains(QStringLiteral("QObject::connect(&dbWorkerThread, &QThread::finished")),
                 "DatabaseWorker should be deleted from its owning worker thread");
        QVERIFY2(text.contains(QStringLiteral("dbWorker, &QObject::deleteLater")),
                 "DatabaseWorker shutdown should use deleteLater on thread finish");

        const qsizetype shutdownBegin =
            text.indexOf(QStringLiteral("[app-exit] DatabaseWorker thread did not finish"));
        QVERIFY2(shutdownBegin >= 0, "DatabaseWorker shutdown block must exist");
        const QString shutdownBlock = text.mid(shutdownBegin);
        QVERIFY2(!shutdownBlock.contains(QStringLiteral("delete dbWorker;")),
                 "DatabaseWorker must not be directly deleted from the main thread after wait()");
    }

    void peerConnectionShutdownHandlersDoNotTouchDestroyedLocalRegistries()
    {
        QFile source(QStringLiteral(LEYOCHAT_APP_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        QVERIFY2(text.contains(QStringLiteral("auto appShuttingDown = std::make_shared<bool>(false);")),
                 "connection callbacks need a lifetime-safe shutdown flag");
        QVERIFY2(text.contains(QStringLiteral("*appShuttingDown = true;")),
                 "the shutdown flag must be set immediately after the Qt event loop exits");

        const qsizetype destroyedBegin =
            text.indexOf(QStringLiteral("QObject::connect(connection, &QObject::destroyed"));
        QVERIFY2(destroyedBegin >= 0, "connection destroyed handler must exist");
        const qsizetype destroyedEnd =
            text.indexOf(QStringLiteral("QObject::connect(connection, &PeerConnection::disconnected"),
                         destroyedBegin);
        QVERIFY2(destroyedEnd > destroyedBegin, "destroyed handler block should be bounded");
        const QString destroyedBlock = text.mid(destroyedBegin, destroyedEnd - destroyedBegin);
        QVERIFY2(destroyedBlock.contains(QStringLiteral("appShuttingDown")),
                 "destroyed handler must capture the shutdown flag by value");
        QVERIFY2(destroyedBlock.contains(QStringLiteral("if (*appShuttingDown)")),
                 "destroyed handler must skip registry cleanup during app shutdown");

        const qsizetype disconnectedBegin =
            text.indexOf(QStringLiteral("QObject::connect(connection, &PeerConnection::disconnected"),
                         destroyedEnd);
        QVERIFY2(disconnectedBegin >= 0, "connection disconnected handler must exist");
        const qsizetype disconnectedEnd =
            text.indexOf(QStringLiteral("QObject::connect(connection, &PeerConnection::payloadReceived"),
                         disconnectedBegin);
        QVERIFY2(disconnectedEnd > disconnectedBegin, "disconnected handler block should be bounded");
        const QString disconnectedBlock =
            text.mid(disconnectedBegin, disconnectedEnd - disconnectedBegin);
        QVERIFY2(disconnectedBlock.contains(QStringLiteral("appShuttingDown")),
                 "disconnected handler must capture the shutdown flag by value");
        QVERIFY2(disconnectedBlock.contains(QStringLiteral("if (*appShuttingDown)")),
                 "disconnected handler must not touch local registries during app shutdown");

        const qsizetype settledBegin =
            text.indexOf(QStringLiteral("auto onGlobalSettled"));
        QVERIFY2(settledBegin >= 0, "global connection-settled handler must exist");
        const qsizetype settledEnd =
            text.indexOf(QStringLiteral("QObject::connect(outbound, &PeerConnection::connected"),
                         settledBegin);
        QVERIFY2(settledEnd > settledBegin, "global settled handler block should be bounded");
        const QString settledBlock = text.mid(settledBegin, settledEnd - settledBegin);
        QVERIFY2(settledBlock.contains(QStringLiteral("appShuttingDown")),
                 "global settled handler must capture the shutdown flag by value");
        QVERIFY2(settledBlock.contains(QStringLiteral("if (*appShuttingDown)")),
                 "global pending connection counters must not be touched during app shutdown");
    }
};

QTEST_MAIN(TestLeyoApplicationRoutingSource)
#include "TestLeyoApplicationRoutingSource.moc"
