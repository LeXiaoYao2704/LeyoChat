#pragma once

#include <QString>

struct RuntimeArchitectureSnapshot;

struct RuntimeArchitecturePresentation {
    QString chromeStatus;
    QString welcomeSummary;
    QString welcomeDetail;
    QString panelBadge;
    QString panelSummary;
    QString panelDetail;
    QString panelFootnote;
};

RuntimeArchitecturePresentation buildRuntimeArchitecturePresentation(
    int serviceCount,
    int workspaceBindingCount,
    int groupBindingCount,
    int resourceCount,
    bool bound,
    const QString& activeServiceName);

RuntimeArchitecturePresentation buildRuntimeArchitecturePresentation(
    const RuntimeArchitectureSnapshot& snapshot);
