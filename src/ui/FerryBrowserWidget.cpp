#include "FerryBrowserWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <ElaText.h>
#include <ElaPushButton.h>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QMessageBox>
#include <QEvent>
#include <QCloseEvent>
#include <QUrl>

FerryBrowserWidget::FerryBrowserWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void FerryBrowserWidget::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- 顶部工具栏 ---
    auto* toolbar = new QWidget(this);
    toolbar->setFixedHeight(32);
    toolbar->setStyleSheet(QStringLiteral(
        "background-color: #f5f5f5; border-bottom: 1px solid #e0e0e0;"));
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(8, 2, 8, 2);

    auto* titleLabel = new ElaText(QStringLiteral("Ferry 跨网文件安全交换"), toolbar);
    titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #333;"));
    tbLayout->addWidget(titleLabel);
    tbLayout->addStretch();

    m_refreshBtn = new ElaPushButton(QStringLiteral("🔄 刷新"), toolbar);
    m_refreshBtn->setFixedSize(72, 26);
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    m_refreshBtn->setToolTip(QStringLiteral("刷新"));
    m_refreshBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#e8e8e8; border:1px solid #ccc; border-radius:4px;"
        "  color:#333; font-size:12px; padding:2px 8px; }"
        "QPushButton:hover { background:#d0d0d0; }" ));
    tbLayout->addWidget(m_refreshBtn);

    m_detachBtn = new ElaPushButton(QStringLiteral("⬆ 弹出"), toolbar);
    m_detachBtn->setFixedSize(72, 26);
    m_detachBtn->setCursor(Qt::PointingHandCursor);
    m_detachBtn->setToolTip(QStringLiteral("弹出独立窗口"));
    m_detachBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#e8e8e8; border:1px solid #ccc; border-radius:4px;"
        "  color:#333; font-size:12px; padding:2px 8px; }"
        "QPushButton:hover { background:#d0d0d0; }" ));
    tbLayout->addWidget(m_detachBtn);

    layout->addWidget(toolbar);

    // --- WebEngine ---
    m_profile = new QWebEngineProfile(QStringLiteral("ferry"), this);
    m_webView = new QWebEngineView(this);
    auto* page = new QWebEnginePage(m_profile, m_webView);
    m_webView->setPage(page);
    layout->addWidget(m_webView);

    // --- 状态栏 ---
    m_statusLabel = new ElaText(QStringLiteral("正在加载..."), this);
    m_statusLabel->setFixedHeight(24);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "background-color: #e3f2fd; color: #1565c0; font-size: 12px;"));
    layout->addWidget(m_statusLabel);

    // --- 信号连接 ---
    connect(m_webView->page(), &QWebEnginePage::loadFinished,
            this, &FerryBrowserWidget::onLoadFinished);
    connect(m_webView->page(), &QWebEnginePage::renderProcessTerminated,
            this, [this](QWebEnginePage::RenderProcessTerminationStatus s, int code) {
                onRenderProcessTerminated(static_cast<int>(s), code);
            });
    connect(m_refreshBtn, &QAbstractButton::clicked, m_webView, &QWebEngineView::reload);
    connect(m_detachBtn, &QAbstractButton::clicked, this, &FerryBrowserWidget::detachRequested);

    const QUrl serviceUrl(qEnvironmentVariable("LEYOCHAT_FILE_EXCHANGE_URL").trimmed());
    const bool isSupportedUrl = serviceUrl.isValid()
        && (serviceUrl.scheme() == QStringLiteral("https")
            || serviceUrl.scheme() == QStringLiteral("http"))
        && !serviceUrl.host().isEmpty();
    m_serviceConfigured = isSupportedUrl;
    if (isSupportedUrl) {
        m_webView->load(serviceUrl);
    } else {
        m_refreshBtn->setEnabled(false);
        m_statusLabel->setText(QStringLiteral("未配置文件交换服务"));
        m_webView->setHtml(QStringLiteral(
            "<html><body style='font-family:sans-serif;color:#666;display:flex;"
            "align-items:center;justify-content:center;height:100%;'>"
            "<p>请设置 LEYOCHAT_FILE_EXCHANGE_URL 后重新启动 LeyoChat。</p>"
            "</body></html>"));
    }
}

void FerryBrowserWidget::onLoadFinished(bool ok)
{
    if (!m_serviceConfigured)
        return;

    if (ok) {
        m_statusLabel->setText(QStringLiteral("已连接"));
        m_statusLabel->setStyleSheet(QStringLiteral(
            "background-color: #e8f5e9; color: #2e7d32; font-size: 12px;"));
    } else {
        m_statusLabel->setText(QStringLiteral("连接失败 — 请检查网络"));
        m_statusLabel->setStyleSheet(QStringLiteral(
            "background-color: #ffebee; color: #c62828; font-size: 12px;"));
    }
}

void FerryBrowserWidget::onRenderProcessTerminated(int terminationStatus, int exitCode)
{
    Q_UNUSED(terminationStatus)
    Q_UNUSED(exitCode)
    auto result = QMessageBox::warning(this, QStringLiteral("Ferry 异常"),
        QStringLiteral("页面进程异常退出。是否重新加载？"),
        QMessageBox::Yes | QMessageBox::No);
    if (result == QMessageBox::Yes)
        m_webView->reload();
}

void FerryBrowserWidget::detachToWindow()
{
    if (m_detachedWindow) return;

    m_detachedWindow = new QWidget(nullptr, Qt::Window);
    m_detachedWindow->setWindowTitle(QStringLiteral("Ferry 跨网文件安全交换"));
    m_detachedWindow->setMinimumSize(1024, 768);
    m_detachedWindow->setAttribute(Qt::WA_DeleteOnClose, false);

    auto* dlayout = new QVBoxLayout(m_detachedWindow);
    dlayout->setContentsMargins(0, 0, 0, 0);

    // 把自身从 contentStack 移出，放入独立窗口
    setParent(m_detachedWindow);
    dlayout->addWidget(this);
    show();

    m_detachedWindow->installEventFilter(this);
    m_detachedWindow->show();
    m_detachedWindow->raise();
    m_detachedWindow->activateWindow();

    m_detachBtn->setText(QStringLiteral("⬇"));
    m_detachBtn->setToolTip(QStringLiteral("回嵌到主窗口"));
    disconnect(m_detachBtn, &QAbstractButton::clicked, this, &FerryBrowserWidget::detachRequested);
    connect(m_detachBtn, &QAbstractButton::clicked, this, &FerryBrowserWidget::reattachRequested);
}

void FerryBrowserWidget::reattachToPanel()
{
    if (!m_detachedWindow) return;

    setParent(nullptr);
    m_detachedWindow->hide();
    m_detachedWindow->deleteLater();
    m_detachedWindow = nullptr;

    m_detachBtn->setText(QStringLiteral("⬆"));
    m_detachBtn->setToolTip(QStringLiteral("弹出独立窗口"));
    disconnect(m_detachBtn, &QAbstractButton::clicked, this, &FerryBrowserWidget::reattachRequested);
    connect(m_detachBtn, &QAbstractButton::clicked, this, &FerryBrowserWidget::detachRequested);

    emit reattachRequested();
}

bool FerryBrowserWidget::isDetached() const
{
    return m_detachedWindow != nullptr;
}

bool FerryBrowserWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_detachedWindow && event->type() == QEvent::Close) {
        reattachToPanel();
        return true; // 已处理，不要真的销毁窗口
    }
    return QWidget::eventFilter(watched, event);
}
