#include "integrations/AzureDevOpsLinkParser.h"

#include <QUrl>
#include <QUrlQuery>

namespace {

QString trimTrailingSlash(const QString& value)
{
    QString trimmed = value.trimmed();
    while (trimmed.endsWith(QLatin1Char('/'))) {
        trimmed.chop(1);
    }
    return trimmed;
}

std::optional<AzureDevOpsResourceLocator> parseDevAzureUrl(const QUrl& url)
{
    const QStringList segments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.size() < 3) {
        return std::nullopt;
    }

    AzureDevOpsResourceLocator locator;
    locator.organization = segments.value(0).trimmed();
    locator.project = segments.value(1).trimmed();
    locator.webUrl = trimTrailingSlash(url.toString(QUrl::FullyDecoded));

    if (segments.size() >= 5
        && segments.value(2) == QStringLiteral("_workitems")
        && segments.value(3) == QStringLiteral("edit")) {
        locator.kind = AzureDevOpsResourceKind::WorkItem;
        locator.resourceId = segments.value(4).trimmed();
    } else if (segments.size() >= 6
               && segments.value(2) == QStringLiteral("_git")
               && segments.value(4) == QStringLiteral("pullrequest")) {
        locator.kind = AzureDevOpsResourceKind::PullRequest;
        locator.repository = segments.value(3).trimmed();
        locator.resourceId = segments.value(5).trimmed();
    } else if (segments.size() >= 4
               && segments.value(2) == QStringLiteral("_build")
               && segments.value(3) == QStringLiteral("results")) {
        const QUrlQuery query(url);
        locator.kind = AzureDevOpsResourceKind::Build;
        locator.resourceId = query.queryItemValue(QStringLiteral("buildId")).trimmed();
    }

    if (!locator.isValid()) {
        return std::nullopt;
    }
    return locator;
}

std::optional<AzureDevOpsResourceLocator> parseVisualStudioUrl(const QUrl& url)
{
    const QString host = url.host().trimmed();
    if (!host.endsWith(QStringLiteral(".visualstudio.com"), Qt::CaseInsensitive)) {
        return std::nullopt;
    }

    const QString organization = host.left(host.indexOf(QStringLiteral(".visualstudio.com")));
    const QStringList segments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (organization.isEmpty() || segments.size() < 2) {
        return std::nullopt;
    }

    AzureDevOpsResourceLocator locator;
    locator.organization = organization;
    locator.project = segments.value(0).trimmed();
    locator.webUrl = trimTrailingSlash(url.toString(QUrl::FullyDecoded));

    if (segments.size() >= 4
        && segments.value(1) == QStringLiteral("_workitems")
        && segments.value(2) == QStringLiteral("edit")) {
        locator.kind = AzureDevOpsResourceKind::WorkItem;
        locator.resourceId = segments.value(3).trimmed();
    } else if (segments.size() >= 5
               && segments.value(1) == QStringLiteral("_git")
               && segments.value(3) == QStringLiteral("pullrequest")) {
        locator.kind = AzureDevOpsResourceKind::PullRequest;
        locator.repository = segments.value(2).trimmed();
        locator.resourceId = segments.value(4).trimmed();
    } else if (segments.size() >= 3
               && segments.value(1) == QStringLiteral("_build")
               && segments.value(2) == QStringLiteral("results")) {
        const QUrlQuery query(url);
        locator.kind = AzureDevOpsResourceKind::Build;
        locator.resourceId = query.queryItemValue(QStringLiteral("buildId")).trimmed();
    }

    if (!locator.isValid()) {
        return std::nullopt;
    }
    return locator;
}

}  // namespace

bool AzureDevOpsResourceLocator::isValid() const
{
    return kind != AzureDevOpsResourceKind::Unknown
        && !organization.trimmed().isEmpty()
        && !project.trimmed().isEmpty()
        && !resourceId.trimmed().isEmpty();
}

QString AzureDevOpsResourceLocator::kindName() const
{
    switch (kind) {
    case AzureDevOpsResourceKind::WorkItem:
        return QStringLiteral("devops_work_item");
    case AzureDevOpsResourceKind::PullRequest:
        return QStringLiteral("devops_pull_request");
    case AzureDevOpsResourceKind::Build:
        return QStringLiteral("devops_build");
    case AzureDevOpsResourceKind::Unknown:
    default:
        return QString();
    }
}

std::optional<AzureDevOpsResourceLocator> AzureDevOpsLinkParser::parse(const QString& text)
{
    const QUrl url = QUrl::fromUserInput(text.trimmed());
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
        return std::nullopt;
    }

    if (url.host().compare(QStringLiteral("dev.azure.com"), Qt::CaseInsensitive) == 0) {
        return parseDevAzureUrl(url);
    }

    return parseVisualStudioUrl(url);
}
