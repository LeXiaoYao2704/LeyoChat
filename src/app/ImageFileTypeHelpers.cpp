#include "app/ImageFileTypeHelpers.h"

#include <QFileInfo>
#include <QStringList>

namespace {

bool hasCaseInsensitiveSuffix(const QString& value, const QStringList& suffixes)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    for (const QString& suffix : suffixes) {
        if (trimmed.endsWith(suffix, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool hasCaseInsensitiveExtension(const QString& filePath, const QStringList& extensions)
{
    const QString suffix = QFileInfo(filePath.trimmed()).suffix();
    if (suffix.isEmpty()) {
        return false;
    }
    return extensions.contains(suffix, Qt::CaseInsensitive);
}

} // namespace

bool isChatPreviewImageAttachmentName(const QString& fileName)
{
    static const QStringList kPreviewImageSuffixes = {
        QStringLiteral(".png"),
        QStringLiteral(".jpg"),
        QStringLiteral(".jpeg"),
        QStringLiteral(".gif"),
        QStringLiteral(".bmp"),
        QStringLiteral(".webp"),
    };
    return hasCaseInsensitiveSuffix(fileName, kPreviewImageSuffixes);
}

bool isReceiptQuietImageFileName(const QString& fileName)
{
    static const QStringList kQuietReceiptImageSuffixes = {
        QStringLiteral(".png"),
        QStringLiteral(".jpg"),
        QStringLiteral(".jpeg"),
        QStringLiteral(".gif"),
        QStringLiteral(".bmp"),
        QStringLiteral(".webp"),
        QStringLiteral(".svg"),
    };
    return hasCaseInsensitiveSuffix(fileName, kQuietReceiptImageSuffixes);
}

bool isImageViewerSupportedPath(const QString& filePath)
{
    static const QStringList kViewerImageExtensions = {
        QStringLiteral("png"),
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("bmp"),
        QStringLiteral("gif"),
        QStringLiteral("webp"),
        QStringLiteral("ico"),
        QStringLiteral("svg"),
    };
    return hasCaseInsensitiveExtension(filePath, kViewerImageExtensions);
}
