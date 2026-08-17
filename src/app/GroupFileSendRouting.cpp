// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增86行/修改0行/删除0行; 总行数44行
// @AI-LastModified: 2026-04-16 20:21:45

#include "app/GroupFileSendRouting.h"

namespace {

bool hasUsableFileServiceConfig(const GroupFileServiceConfig& config)
{
    return config.enabled
        && !config.baseUrl.trimmed().isEmpty();
}

}

bool isInlineGroupImagePath(const QString& filePath)
{
    const QString lowerFilePath = filePath.trimmed().toLower();
    return lowerFilePath.endsWith(QStringLiteral(".png"))
        || lowerFilePath.endsWith(QStringLiteral(".jpg"))
        || lowerFilePath.endsWith(QStringLiteral(".jpeg"))
        || lowerFilePath.endsWith(QStringLiteral(".gif"))
        || lowerFilePath.endsWith(QStringLiteral(".bmp"))
        || lowerFilePath.endsWith(QStringLiteral(".webp"))
        || lowerFilePath.endsWith(QStringLiteral(".svg"));
}

GroupFileSendDecision decideGroupFileSendRoute(
    const QString& filePath,
    const HybridRoutingDecision& routingDecision,
    const GroupFileServiceConfig& fileServiceConfig)
{
    GroupFileSendDecision decision;
    decision.isImage = isInlineGroupImagePath(filePath);
    decision.hasUsableFileServiceConfig = hasUsableFileServiceConfig(fileServiceConfig);
    decision.hasBoundService = routingDecision.hasBoundService;
    decision.sharedFilesEnabled = routingDecision.sharedFilesEnabled;

    if (decision.isImage) {
        decision.mode = GroupFileSendMode::InlineAttachmentImage;
        return decision;
    }

    if (decision.hasUsableFileServiceConfig) {
        decision.mode = GroupFileSendMode::FileServiceUpload;
        return decision;
    }

    decision.mode = GroupFileSendMode::P2PFileOffer;
    return decision;
}
