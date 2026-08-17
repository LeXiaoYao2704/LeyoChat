#include "architecture/RuntimeArchitecturePresentation.h"

#include "architecture/RuntimeArchitectureSnapshot.h"

namespace {
QString resolveActiveServiceName(const RuntimeArchitectureSnapshot& snapshot)
{
    const QString explicitName = snapshot.selection.serviceName.trimmed();
    if (!explicitName.isEmpty()) {
        return explicitName;
    }
    const QString registryName = snapshot.selection.registryEntry.serviceName.trimmed();
    if (!registryName.isEmpty()) {
        return registryName;
    }
    for (const auto& service : snapshot.discoveryResult.services) {
        if (service.serviceId == snapshot.discoveryResult.defaultServiceId
            && !service.serviceName.trimmed().isEmpty()) {
            return service.serviceName.trimmed();
        }
    }
    if (!snapshot.serviceRegistry.isEmpty()) {
        return snapshot.serviceRegistry.front().serviceName.trimmed();
    }
    return {};
}

QString resolveSelectedResourceTitle(const RuntimeArchitectureSnapshot& snapshot)
{
    const QString explicitTitle = snapshot.selection.selectedResource.title.trimmed();
    if (!explicitTitle.isEmpty()) {
        return explicitTitle;
    }
    const QString selectedResourceId = snapshot.selection.selectedResource.resourceId.trimmed();
    if (selectedResourceId.isEmpty()) {
        return {};
    }
    for (const auto& resource : snapshot.visibleResources) {
        if (resource.resourceId == selectedResourceId && !resource.title.trimmed().isEmpty()) {
            return resource.title.trimmed();
        }
    }
    return selectedResourceId;
}
} // namespace

RuntimeArchitecturePresentation buildRuntimeArchitecturePresentation(
    int serviceCount,
    int workspaceBindingCount,
    int groupBindingCount,
    int resourceCount,
    bool bound,
    const QString& activeServiceName)
{
    const QString trimmedServiceName = activeServiceName.trimmed();
    RuntimeArchitecturePresentation presentation;
    presentation.chromeStatus =
        serviceCount > 0 ? QStringLiteral("%1 个协作服务").arg(serviceCount)
                         : QStringLiteral("今日概览");
    presentation.welcomeSummary =
        serviceCount > 0
            ? QStringLiteral("已发现 %1 个协作服务，当前%2")
                  .arg(serviceCount)
                  .arg(bound ? QStringLiteral("已绑定")
                             : QStringLiteral("未绑定"))
            : QStringLiteral("选择一个会话开始");
    presentation.welcomeDetail =
        serviceCount > 0
            ? QStringLiteral("工作区 %1 个，群绑定 %2 个，可见资源 %3 个%4")
                  .arg(workspaceBindingCount)
                  .arg(groupBindingCount)
                  .arg(resourceCount)
                  .arg(trimmedServiceName.isEmpty()
                           ? QString()
                           : QStringLiteral("，当前服务 %1").arg(trimmedServiceName))
            : QStringLiteral("从最近消息继续，或者搜索历史记录、图片和链接。");
    presentation.panelBadge = presentation.chromeStatus;
    presentation.panelSummary =
        serviceCount > 0
            ? QStringLiteral("运行时已加载 %1 个服务快照").arg(serviceCount)
            : QStringLiteral("当前没有已持久化的服务快照");
    presentation.panelDetail =
        serviceCount > 0
            ? QStringLiteral("%1 个工作区绑定，%2 个群绑定，%3 个可见资源")
                  .arg(workspaceBindingCount)
                  .arg(groupBindingCount)
                  .arg(resourceCount)
            : QStringLiteral("群公告和成员信息会在你打开侧栏时显示。");
    presentation.panelFootnote =
        serviceCount > 0
            ? QStringLiteral("当前%1%2。现阶段只读加载，不改变现有聊天行为。")
                  .arg(bound ? QStringLiteral("已绑定")
                             : QStringLiteral("未绑定"))
                  .arg(trimmedServiceName.isEmpty()
                           ? QString()
                           : QStringLiteral("，服务为 %1").arg(trimmedServiceName))
            : QStringLiteral("侧栏默认收起，聊天区保持清爽。");
    return presentation;
}

RuntimeArchitecturePresentation buildRuntimeArchitecturePresentation(
    const RuntimeArchitectureSnapshot& snapshot)
{
    const int serviceCount = snapshot.discoveryResult.services.isEmpty()
                                 ? snapshot.serviceRegistry.size()
                                 : snapshot.discoveryResult.services.size();
    RuntimeArchitecturePresentation presentation =
        buildRuntimeArchitecturePresentation(serviceCount,
                                            snapshot.workspaceBindings.size(),
                                            snapshot.groupBindings.size(),
                                            snapshot.visibleResources.size(),
                                            snapshot.selection.bound,
                                            resolveActiveServiceName(snapshot));
    const QString selectedResourceTitle = resolveSelectedResourceTitle(snapshot);
    if (!selectedResourceTitle.isEmpty()) {
        presentation.welcomeDetail.append(
            QStringLiteral(" 当前选中资源 %1。").arg(selectedResourceTitle));
        presentation.panelFootnote.append(
            QStringLiteral(" 当前选中资源 %1。").arg(selectedResourceTitle));
    }
    return presentation;
}
