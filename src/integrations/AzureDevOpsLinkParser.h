#pragma once

#include <optional>

#include <QString>

enum class AzureDevOpsResourceKind {
    Unknown,
    WorkItem,
    PullRequest,
    Build
};

struct AzureDevOpsResourceLocator {
    AzureDevOpsResourceKind kind = AzureDevOpsResourceKind::Unknown;
    QString organization;
    QString project;
    QString repository;
    QString resourceId;
    QString webUrl;

    bool isValid() const;
    QString kindName() const;
};

class AzureDevOpsLinkParser {
public:
    static std::optional<AzureDevOpsResourceLocator> parse(const QString& text);
};
