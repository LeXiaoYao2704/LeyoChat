#pragma once

#include <QString>

bool isChatPreviewImageAttachmentName(const QString& fileName);

bool isReceiptQuietImageFileName(const QString& fileName);

bool isImageViewerSupportedPath(const QString& filePath);
