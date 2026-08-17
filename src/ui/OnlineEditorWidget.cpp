#include "OnlineEditorWidget.h"
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QCloseEvent>

void OnlineEditorWidget::warmUp()
{
    // Accessing the default profile triggers Chromium subprocess launch.
    // This is a no-op if already initialized.
    QWebEngineProfile::defaultProfile();
}

OnlineEditorWidget::OnlineEditorWidget(const QString& editorUrl,
                                         const QString& fileName,
                                         QWidget* parent)
    : QWidget(parent)
{
    setupUi(editorUrl, fileName);
}

void OnlineEditorWidget::setupUi(const QString& editorUrl, const QString& fileName)
{
    setWindowTitle(QStringLiteral("编辑 - %1").arg(fileName));
    setMinimumSize(1024, 768);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_statusLabel = new QLabel(QStringLiteral("正在加载..."), this);
    m_statusLabel->setFixedHeight(24);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(
        QStringLiteral("background-color: #e3f2fd; color: #1565c0; font-size: 12px;"));
    layout->addWidget(m_statusLabel);

    m_webView = new QWebEngineView(this);
    layout->addWidget(m_webView);

    connect(m_webView->page(), &QWebEnginePage::loadFinished,
            this, &OnlineEditorWidget::onLoadFinished);
    connect(m_webView->page(), &QWebEnginePage::renderProcessTerminated,
            this, [this](QWebEnginePage::RenderProcessTerminationStatus status, int code) {
                onRenderProcessTerminated(static_cast<int>(status), code);
            });

    m_webView->load(QUrl(editorUrl));
}

void OnlineEditorWidget::onLoadFinished(bool ok)
{
    if (ok) {
        m_statusLabel->setText(QStringLiteral("已连接"));
        m_statusLabel->setStyleSheet(
            QStringLiteral("background-color: #e8f5e9; color: #2e7d32; font-size: 12px;"));
    } else {
        m_statusLabel->setText(QStringLiteral("连接失败 — 文档服务不可用"));
        m_statusLabel->setStyleSheet(
            QStringLiteral("background-color: #ffebee; color: #c62828; font-size: 12px;"));
    }
}

void OnlineEditorWidget::onRenderProcessTerminated(int terminationStatus, int exitCode)
{
    Q_UNUSED(terminationStatus);
    Q_UNUSED(exitCode);
    auto result = QMessageBox::warning(this, QStringLiteral("编辑器异常"),
        QStringLiteral("编辑器进程异常退出。是否重新打开？"),
        QMessageBox::Yes | QMessageBox::No);
    if (result == QMessageBox::Yes)
        m_webView->reload();
    else
        close();
}

void OnlineEditorWidget::closeEvent(QCloseEvent* event)
{
    emit editorClosed();
    event->accept();
}
