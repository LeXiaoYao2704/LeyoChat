#pragma once
#include <QWidget>

class QStackedWidget;
class ElaStackedWidget;
class ElaPlainTextEdit;
class QTextEdit;
class ElaTextEdit;
class ElaText;
class ElaPushButton;
#ifdef LEYOCHAT_HAS_WEBENGINE
class QWebEngineView;
#endif

class FilePreviewWidget : public QWidget
{
    Q_OBJECT
public:
    enum PreviewType { Markdown, PlainText, Office, Unsupported };

    static FilePreviewWidget* fromLocalFile(const QString& filePath,
                                            const QString& fileName,
                                            QWidget* parent = nullptr);
    static FilePreviewWidget* fromOnlyOffice(const QString& url,
                                             const QString& fileName,
                                             QWidget* parent = nullptr);

    static bool isPreviewSupported(const QString& fileName);
    static PreviewType detectPreviewType(const QString& fileName);

signals:
    void previewClosed();
    void editRequested();

protected:
    void closeEvent(QCloseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    explicit FilePreviewWidget(const QString& fileName, QWidget* parent = nullptr);

    void setupUi(const QString& fileName);
    void loadMarkdown(const QString& filePath);
    void loadPlainText(const QString& filePath);
    void loadMoreText();
    void loadTailText();
    void loadOffice(const QString& url);
    void showUnsupported(const QString& fileName);
    void updatePlainTextStatus();

    ElaStackedWidget*  m_renderStack      = nullptr;
    ElaTextEdit*       m_markdownEdit     = nullptr;  // Qt 原生 Markdown 渲染
    ElaPlainTextEdit*  m_textEdit         = nullptr;
    ElaText*          m_unsupportedLabel = nullptr;
    ElaText*          m_statusLabel      = nullptr;
    QWidget*         m_officePlaceholder = nullptr;
    QWidget*         m_loadMoreBar      = nullptr;
    ElaPushButton*     m_loadMoreBtn      = nullptr;
    ElaPushButton*     m_loadTailBtn      = nullptr;
    ElaPushButton*     m_editBtn          = nullptr;

    // 分段加载状态
    QString          m_currentFilePath;
    qint64           m_fileSize         = 0;
    qint64           m_bytesLoaded      = 0;

    // 无边框拖拽状态
    QWidget*         m_titleBar         = nullptr;
    QPoint           m_dragPos;
    bool             m_dragging         = false;

    static constexpr qint64 CHUNK_SIZE = 1024 * 1024; // 1 MB per chunk

#ifdef LEYOCHAT_HAS_WEBENGINE
    QWebEngineView*  m_officeView      = nullptr;
#endif
};
