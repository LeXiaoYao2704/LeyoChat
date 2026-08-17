// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增21行/修改0行/删除0行; 总行数21行
// @AI-LastModified: 2026-04-16 19:35:01

#pragma once

#include <QString>

#include "architecture/HybridRoutingPolicy.h"
#include "integrations/RemoteFileServiceSettings.h"

enum class GroupFileSendMode {
    InlineAttachmentImage,
    FileServiceUpload,
    P2PFileOffer
};

struct GroupFileSendDecision {
    GroupFileSendMode mode = GroupFileSendMode::P2PFileOffer;
    bool isImage = false;
    bool hasUsableFileServiceConfig = false;
    bool hasBoundService = false;
    bool sharedFilesEnabled = false;
};

bool isInlineGroupImagePath(const QString& filePath);

GroupFileSendDecision decideGroupFileSendRoute(
    const QString& filePath,
    const HybridRoutingDecision& routingDecision,
    const GroupFileServiceConfig& fileServiceConfig);