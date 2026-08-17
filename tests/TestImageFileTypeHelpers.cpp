#include <QtTest>

#include "app/ImageFileTypeHelpers.h"

class TestImageFileTypeHelpers : public QObject {
    Q_OBJECT

private slots:
    void chatPreviewImageAttachmentName_matchesExistingPreviewExtensions();
    void receiptQuietImageFileName_keepsSvgQuietForCompletedTransferReceipts();
    void imageViewerSupportedPath_acceptsViewerOnlyExtensions();
    void imageHelpers_rejectEmptyOrUnknownNames();
};

void TestImageFileTypeHelpers::chatPreviewImageAttachmentName_matchesExistingPreviewExtensions()
{
    QVERIFY(isChatPreviewImageAttachmentName(QStringLiteral("photo.PNG")));
    QVERIFY(isChatPreviewImageAttachmentName(QStringLiteral("scan.jpeg")));
    QVERIFY(!isChatPreviewImageAttachmentName(QStringLiteral("diagram.svg")));
    QVERIFY(!isChatPreviewImageAttachmentName(QStringLiteral("archive.zip")));
}

void TestImageFileTypeHelpers::receiptQuietImageFileName_keepsSvgQuietForCompletedTransferReceipts()
{
    QVERIFY(isReceiptQuietImageFileName(QStringLiteral("photo.webp")));
    QVERIFY(isReceiptQuietImageFileName(QStringLiteral("diagram.SVG")));
    QVERIFY(!isReceiptQuietImageFileName(QStringLiteral("icon.ico")));
    QVERIFY(!isReceiptQuietImageFileName(QStringLiteral("report.pdf")));
}

void TestImageFileTypeHelpers::imageViewerSupportedPath_acceptsViewerOnlyExtensions()
{
    QVERIFY(isImageViewerSupportedPath(QStringLiteral("C:/tmp/photo.bmp")));
    QVERIFY(isImageViewerSupportedPath(QStringLiteral("C:/tmp/icon.ICO")));
    QVERIFY(isImageViewerSupportedPath(QStringLiteral("C:/tmp/vector.svg")));
    QVERIFY(!isImageViewerSupportedPath(QStringLiteral("C:/tmp/report.pdf")));
}

void TestImageFileTypeHelpers::imageHelpers_rejectEmptyOrUnknownNames()
{
    QVERIFY(!isChatPreviewImageAttachmentName(QString()));
    QVERIFY(!isReceiptQuietImageFileName(QStringLiteral("README")));
    QVERIFY(!isImageViewerSupportedPath(QStringLiteral("")));
}

QTEST_MAIN(TestImageFileTypeHelpers)
#include "TestImageFileTypeHelpers.moc"
