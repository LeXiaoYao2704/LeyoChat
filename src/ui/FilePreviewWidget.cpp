#include "FilePreviewWidget.h"

#include "ui/AppStyle.h"
#include "ui/MarkdownRenderer.h"

#include <QCloseEvent>
#include <QFileInfo>
#include <QFile>
#include <QFutureWatcher>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <ElaText.h>
#include <QLocale>
#include <QMouseEvent>
#include <ElaPlainTextEdit.h>
#include <ElaPushButton.h>
#include <QStackedWidget>
#include <ElaStackedWidget.h>
#include <QTextCursor>
#include <QTextEdit>
#include <ElaTextEdit.h>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windowsx.h>
#pragma comment(lib, "dwmapi.lib")
#endif

#ifdef LEYOCHAT_HAS_WEBENGINE
#include <QWebEngineView>
#include <QWebEnginePage>
#endif

// ── 静态方法 ──────────────────────────────────────────────────

FilePreviewWidget::PreviewType FilePreviewWidget::detectPreviewType(const QString& fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    if (ext == QStringLiteral("md") || ext == QStringLiteral("markdown"))
        return Markdown;
    if (ext == QStringLiteral("txt") || ext == QStringLiteral("log") ||
        ext == QStringLiteral("csv") || ext == QStringLiteral("json") ||
        ext == QStringLiteral("xml") || ext == QStringLiteral("yaml") ||
        ext == QStringLiteral("yml") || ext == QStringLiteral("ini") ||
        ext == QStringLiteral("cfg") || ext == QStringLiteral("conf"))
        return PlainText;
    if (ext == QStringLiteral("docx") || ext == QStringLiteral("xlsx") ||
        ext == QStringLiteral("pptx") || ext == QStringLiteral("doc") ||
        ext == QStringLiteral("xls")  || ext == QStringLiteral("ppt") ||
        ext == QStringLiteral("odt")  || ext == QStringLiteral("ods") ||
        ext == QStringLiteral("odp"))
        return Office;
    return Unsupported;
}

bool FilePreviewWidget::isPreviewSupported(const QString& fileName)
{
    return detectPreviewType(fileName) != Unsupported;
}

FilePreviewWidget* FilePreviewWidget::fromLocalFile(const QString& filePath,
                                                     const QString& fileName,
                                                     QWidget* parent)
{
    auto* w = new FilePreviewWidget(fileName, parent);
    const auto type = detectPreviewType(fileName);
    switch (type) {
    case Markdown:  w->loadMarkdown(filePath); break;
    case PlainText: w->loadPlainText(filePath); break;
    case Office:    w->showUnsupported(fileName); break;
    default:        w->showUnsupported(fileName); break;
    }
    return w;
}

FilePreviewWidget* FilePreviewWidget::fromOnlyOffice(const QString& url,
                                                      const QString& fileName,
                                                      QWidget* parent)
{
    auto* w = new FilePreviewWidget(fileName, parent);
    w->loadOffice(url);
    return w;
}

// ── 构造与 UI ─────────────────────────────────────────────────

FilePreviewWidget::FilePreviewWidget(const QString& fileName, QWidget* parent)
    : QWidget(parent)
{
    setupUi(fileName);
}

void FilePreviewWidget::setupUi(const QString& fileName)
{
    setWindowTitle(QStringLiteral("预览 - %1").arg(fileName));
    setMinimumSize(800, 600);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    // 外层布局：留 shadow margin
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(0);

    // 内容容器（带圆角和阴影）
    auto* container = new QWidget(this);
    container->setObjectName(QStringLiteral("previewContainer"));
    container->setStyleSheet(QStringLiteral(
        "#previewContainer { background:%1; border-radius:8px; border:1px solid %2; }")
        .arg(AppStyle::surface(), AppStyle::border()));

    auto* shadow = new QGraphicsDropShadowEffect(container);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 60));
    container->setGraphicsEffect(shadow);

    auto* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 标题栏（可拖拽） ──
    m_titleBar = new QWidget(container);
    m_titleBar->setFixedHeight(44);
    m_titleBar->setStyleSheet(QStringLiteral(
        "background:%1; border-top-left-radius:8px; border-top-right-radius:8px;")
        .arg(AppStyle::surface()));
    auto* tbLayout = new QHBoxLayout(m_titleBar);
    tbLayout->setContentsMargins(16, 0, 8, 0);

    auto* fileIcon = new ElaText(QStringLiteral("📄"), m_titleBar);
    fileIcon->setStyleSheet(QStringLiteral("font-size:16px;"));
    tbLayout->addWidget(fileIcon);

    auto* titleLabel = new ElaText(fileName, m_titleBar);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-weight:600; font-size:13px; color:%1; margin-left:6px;")
        .arg(AppStyle::textPrimary()));
    tbLayout->addWidget(titleLabel);
    tbLayout->addStretch();

    // 编辑按钮（仅 OnlyOffice 预览时显示，默认隐藏）
    m_editBtn = new ElaPushButton(QStringLiteral("\u270f \u7f16\u8f91"), m_titleBar);
    m_editBtn->setFixedHeight(28);
    m_editBtn->setStyleSheet(QStringLiteral(
        "QPushButton { border:1px solid %1; border-radius:4px; padding:2px 12px;"
        "  font-size:12px; color:%2; background:transparent; }"
        "QPushButton:hover { background:%3; }")
        .arg(AppStyle::accent(), AppStyle::accent(), AppStyle::hoverBg()));
    connect(m_editBtn, &ElaPushButton::clicked, this, &FilePreviewWidget::editRequested);
    m_editBtn->hide();
    tbLayout->addWidget(m_editBtn);
    tbLayout->addSpacing(8);
    const QString btnStyle = QStringLiteral(
        "QPushButton { border:none; border-radius:6px; font-size:14px; color:%1;"
        "  background:transparent; padding-bottom:2px; }"
        "QPushButton:hover { background:%2; }")
        .arg(AppStyle::textSecondary(), AppStyle::hoverBg());
    const QString closeBtnStyle = QStringLiteral(
        "QPushButton { border:none; border-radius:6px; font-size:13px; color:%1;"
        "  background:transparent; }"
        "QPushButton:hover { background:#e81123; color:white; }")
        .arg(AppStyle::textSecondary());

    auto* minBtn = new ElaPushButton(QStringLiteral("─"), m_titleBar);
    minBtn->setFixedSize(32, 28);
    minBtn->setStyleSheet(btnStyle);
    connect(minBtn, &ElaPushButton::clicked, this, &QWidget::showMinimized);
    tbLayout->addWidget(minBtn);

    auto* maxBtn = new ElaPushButton(QStringLiteral("□"), m_titleBar);
    maxBtn->setFixedSize(32, 28);
    maxBtn->setStyleSheet(btnStyle);
    connect(maxBtn, &ElaPushButton::clicked, this, [this, outerLayout]() {
        if (isMaximized()) {
            showNormal();
            outerLayout->setContentsMargins(8, 8, 8, 8);
        } else {
            outerLayout->setContentsMargins(0, 0, 0, 0);
            showMaximized();
        }
    });
    tbLayout->addWidget(maxBtn);

    auto* closeBtn = new ElaPushButton(QStringLiteral("✕"), m_titleBar);
    closeBtn->setFixedSize(32, 28);
    closeBtn->setStyleSheet(closeBtnStyle);
    connect(closeBtn, &ElaPushButton::clicked, this, &QWidget::close);
    tbLayout->addWidget(closeBtn);

    mainLayout->addWidget(m_titleBar);

    // ── 渲染区域 ──
    m_renderStack = new ElaStackedWidget(this);
    mainLayout->addWidget(m_renderStack);

    // Index 0: Markdown 渲染 (md4c)
    m_markdownEdit = new ElaTextEdit(this);
    m_markdownEdit->setReadOnly(true);
    m_markdownEdit->setFrameShape(QFrame::NoFrame);
    m_markdownEdit->setStyleSheet(QStringLiteral(
        "QTextEdit { background:%1; padding:0; border:none; }")
        .arg(AppStyle::surface()));
    m_renderStack->addWidget(m_markdownEdit);

    // Index 1: 纯文本
    m_textEdit = new ElaPlainTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { font-family: Consolas, 'Courier New', monospace;"
        "  font-size: 13px; padding: 12px; }"));
    m_renderStack->addWidget(m_textEdit);

    // Index 2: Office (ONLYOFFICE WebEngine 只读) — 懒加载，占位用空 widget
    m_officePlaceholder = new QWidget(this);
    m_renderStack->addWidget(m_officePlaceholder);

    // Index 3: 不支持预览
    m_unsupportedLabel = new ElaText(this);
    m_unsupportedLabel->setAlignment(Qt::AlignCenter);
    m_unsupportedLabel->setStyleSheet(QStringLiteral(
        "font-size: 16px; color: #999;"));
    m_renderStack->addWidget(m_unsupportedLabel);

    // ── 状态栏 ──
    m_statusLabel = new ElaText(this);
    m_statusLabel->setFixedHeight(28);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "background:%1; color:%2; font-size:12px; border-top:1px solid %3;")
        .arg(AppStyle::surfaceAlt(), AppStyle::textMuted(), AppStyle::border()));

    // ── 大文件分段加载工具栏（默认隐藏） ──
    m_loadMoreBar = new QWidget(this);
    m_loadMoreBar->setFixedHeight(36);
    m_loadMoreBar->setStyleSheet(QStringLiteral(
        "background:%1; border-top:1px solid %2;")
        .arg(AppStyle::surfaceAlt(), AppStyle::border()));
    auto* barLayout = new QHBoxLayout(m_loadMoreBar);
    barLayout->setContentsMargins(12, 4, 12, 4);
    barLayout->addStretch();

    m_loadMoreBtn = new ElaPushButton(QStringLiteral("加载更多 ↓"), m_loadMoreBar);
    m_loadMoreBtn->setStyleSheet(QStringLiteral(
        "QPushButton { border:1px solid %1; border-radius:4px; padding:4px 12px; font-size:12px; color:%2; background:transparent; }"
        "QPushButton:hover { background:%3; }")
        .arg(AppStyle::border(), AppStyle::textPrimary(), AppStyle::hoverBg()));
    connect(m_loadMoreBtn, &ElaPushButton::clicked, this, &FilePreviewWidget::loadMoreText);
    barLayout->addWidget(m_loadMoreBtn);

    m_loadTailBtn = new ElaPushButton(QStringLiteral("跳到末尾 ⏬"), m_loadMoreBar);
    m_loadTailBtn->setStyleSheet(QStringLiteral(
        "QPushButton { border:1px solid %1; border-radius:4px; padding:4px 12px; font-size:12px; color:%2; background:transparent; }"
        "QPushButton:hover { background:%3; }")
        .arg(AppStyle::border(), AppStyle::textPrimary(), AppStyle::hoverBg()));
    connect(m_loadTailBtn, &ElaPushButton::clicked, this, &FilePreviewWidget::loadTailText);
    barLayout->addWidget(m_loadTailBtn);

    barLayout->addStretch();
    m_loadMoreBar->hide();

    mainLayout->addWidget(m_loadMoreBar);
    mainLayout->addWidget(m_statusLabel);

    outerLayout->addWidget(container);

#ifdef Q_OS_WIN
    // 启用 DWM 阴影（原生效果更好）
    if (auto hwnd = reinterpret_cast<HWND>(winId())) {
        MARGINS margins = {1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
#endif
}

// ── 渲染方法 ──────────────────────────────────────────────────

void FilePreviewWidget::loadMarkdown(const QString& filePath)
{
    m_statusLabel->setText(QStringLiteral("正在加载…"));
    m_renderStack->setCurrentIndex(0);

    struct MarkdownResult { QString html; QString fileName; QString sizeStr; bool truncated; bool error; };

    auto* watcher = new QFutureWatcher<MarkdownResult>(this);
    QObject::connect(watcher, &QFutureWatcher<MarkdownResult>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        const auto result = watcher->result();
        if (result.error) {
            m_statusLabel->setText(QStringLiteral("无法打开文件"));
            return;
        }
        m_markdownEdit->setHtml(result.html);
        m_renderStack->setCurrentIndex(0);
        if (result.truncated) {
            m_statusLabel->setText(QStringLiteral("Markdown 预览（已截断） — %1 (%2)")
                .arg(result.fileName, result.sizeStr));
        } else {
            m_statusLabel->setText(QStringLiteral("Markdown 预览 — %1 (%2)")
                .arg(result.fileName, result.sizeStr));
        }
    });
    watcher->setFuture(QtConcurrent::run([filePath]() -> MarkdownResult {
        MarkdownResult result;
        result.error = false;
        result.truncated = false;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            result.error = true;
            return result;
        }

        static constexpr qint64 MAX_PREVIEW_BYTES = 1024 * 1024; // 1 MB
        const qint64 fileSize = file.size();
        result.truncated = fileSize > MAX_PREVIEW_BYTES;
        result.fileName = QFileInfo(filePath).fileName();
        result.sizeStr = QLocale().formattedDataSize(fileSize);

        QTextStream stream(&file);
        QString content;
        if (result.truncated) {
            content = stream.read(MAX_PREVIEW_BYTES);
            content.append(QStringLiteral("\n\n---\n**文件过大，仅显示前 1 MB 内容**\n"));
        } else {
            content = stream.readAll();
        }

        result.html = MarkdownRenderer::renderMarkdownToHtml(content);
        return result;
    }));
}

void FilePreviewWidget::loadPlainText(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_statusLabel->setText(QStringLiteral("无法打开文件"));
        showUnsupported(filePath);
        return;
    }

    m_currentFilePath = filePath;
    m_fileSize = file.size();

    QTextStream stream(&file);
    QString content;
    if (m_fileSize > CHUNK_SIZE) {
        content = stream.read(CHUNK_SIZE);
        m_bytesLoaded = CHUNK_SIZE;
        m_loadMoreBar->show();
    } else {
        content = stream.readAll();
        m_bytesLoaded = m_fileSize;
    }

    m_textEdit->setPlainText(content);
    m_renderStack->setCurrentIndex(1);
    updatePlainTextStatus();
}

void FilePreviewWidget::loadMoreText()
{
    if (m_currentFilePath.isEmpty() || m_bytesLoaded >= m_fileSize)
        return;

    QFile file(m_currentFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream stream(&file);
    stream.seek(m_bytesLoaded);
    const QString chunk = stream.read(CHUNK_SIZE);
    m_bytesLoaded = qMin(m_bytesLoaded + CHUNK_SIZE, m_fileSize);

    // 追加到已有内容
    m_textEdit->moveCursor(QTextCursor::End);
    m_textEdit->insertPlainText(chunk);

    if (m_bytesLoaded >= m_fileSize)
        m_loadMoreBar->hide();

    updatePlainTextStatus();
}

void FilePreviewWidget::loadTailText()
{
    if (m_currentFilePath.isEmpty())
        return;

    QFile file(m_currentFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    // 加载最后 1MB
    const qint64 tailStart = qMax(qint64(0), m_fileSize - CHUNK_SIZE);
    QTextStream stream(&file);
    stream.seek(tailStart);
    const QString tailContent = stream.readAll();

    m_textEdit->setPlainText(tailContent);
    m_bytesLoaded = m_fileSize;
    m_loadMoreBar->hide();

    // 滚动到底部
    auto cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_textEdit->setTextCursor(cursor);

    m_statusLabel->setText(QStringLiteral("纯文本（末尾 %1） — %2 (%3)")
        .arg(QLocale().formattedDataSize(m_fileSize - tailStart),
             QFileInfo(m_currentFilePath).fileName(),
             QLocale().formattedDataSize(m_fileSize)));
}

void FilePreviewWidget::updatePlainTextStatus()
{
    const QString sizeStr = QLocale().formattedDataSize(m_fileSize);
    const QString fileName = QFileInfo(m_currentFilePath).fileName();
    if (m_bytesLoaded >= m_fileSize) {
        m_statusLabel->setText(QStringLiteral("纯文本 — %1 (%2)")
            .arg(fileName, sizeStr));
    } else {
        const int percent = static_cast<int>(m_bytesLoaded * 100 / m_fileSize);
        m_statusLabel->setText(QStringLiteral("纯文本 — %1 (%2) — 已加载 %3%")
            .arg(fileName, sizeStr, QString::number(percent)));
    }
}

void FilePreviewWidget::loadOffice(const QString& url)
{
#ifdef LEYOCHAT_HAS_WEBENGINE
    // 懒加载 QWebEngineView（Chromium 初始化很重，仅在真正需要时创建）
    if (!m_officeView) {
        m_officeView = new QWebEngineView(this);
        const int idx = m_renderStack->indexOf(m_officePlaceholder);
        m_renderStack->removeWidget(m_officePlaceholder);
        m_officePlaceholder->deleteLater();
        m_officePlaceholder = nullptr;
        m_renderStack->insertWidget(idx, m_officeView);
    }
    m_officeView->load(QUrl(url));
    m_renderStack->setCurrentIndex(2);
    m_statusLabel->setText(QStringLiteral("正在加载 Office 预览..."));
    if (m_editBtn) {
        m_editBtn->show();
    }

    connect(m_officeView->page(), &QWebEnginePage::loadFinished,
            this, [this](bool ok) {
        if (ok)
            m_statusLabel->setText(QStringLiteral("Office 只读预览"));
        else
            m_statusLabel->setText(QStringLiteral("Office 预览加载失败"));
    });
#else
    Q_UNUSED(url)
    showUnsupported(QStringLiteral("Office 预览需要 WebEngine 支持"));
#endif
}

void FilePreviewWidget::showUnsupported(const QString& fileName)
{
    m_unsupportedLabel->setText(
        QStringLiteral("该文件不支持预览\n%1").arg(fileName));
    m_renderStack->setCurrentIndex(3);
    m_statusLabel->setText(QString());
}

void FilePreviewWidget::closeEvent(QCloseEvent* event)
{
    emit previewClosed();
    event->accept();
}

// ── 无边框窗口交互 ───────────────────────────────────────────────

void FilePreviewWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_titleBar &&
        m_titleBar->geometry().contains(event->pos())) {
        m_dragging = true;
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void FilePreviewWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}

void FilePreviewWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
    }
}

void FilePreviewWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_titleBar && m_titleBar->geometry().contains(event->pos())) {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
        event->accept();
    }
}

bool FilePreviewWidget::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(eventType)
    auto* msg = static_cast<MSG*>(message);
    if (msg->message == WM_NCHITTEST) {
        const int borderWidth = 5;
        RECT winRect;
        GetWindowRect(msg->hwnd, &winRect);
        const long x = GET_X_LPARAM(msg->lParam);
        const long y = GET_Y_LPARAM(msg->lParam);

        // 边缘拖拽调整大小
        if (x <= winRect.left + borderWidth) {
            if (y <= winRect.top + borderWidth) { *result = HTTOPLEFT; return true; }
            if (y >= winRect.bottom - borderWidth) { *result = HTBOTTOMLEFT; return true; }
            *result = HTLEFT; return true;
        }
        if (x >= winRect.right - borderWidth) {
            if (y <= winRect.top + borderWidth) { *result = HTTOPRIGHT; return true; }
            if (y >= winRect.bottom - borderWidth) { *result = HTBOTTOMRIGHT; return true; }
            *result = HTRIGHT; return true;
        }
        if (y <= winRect.top + borderWidth) { *result = HTTOP; return true; }
        if (y >= winRect.bottom - borderWidth) { *result = HTBOTTOM; return true; }
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return QWidget::nativeEvent(eventType, message, result);
}
