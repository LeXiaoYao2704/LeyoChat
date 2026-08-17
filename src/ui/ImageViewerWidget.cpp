#include "ImageViewerWidget.h"

#include <ElaScrollArea.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <ElaToolButton.h>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QShowEvent>
#include <QScrollBar>
#include <QApplication>
#include <QFileInfo>
#include <QScreen>
#include <QGraphicsDropShadowEffect>
#include <ElaText.h>

ImageViewerWidget::ImageViewerWidget(const QString& filePath, const QString& title, QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    const QString displayTitle = title.isEmpty() ? QFileInfo(filePath).fileName() : title;
    setWindowTitle(displayTitle);

    m_originalPixmap = QPixmap(filePath);
    if (m_originalPixmap.isNull()) {
        auto* errorLabel = new ElaText(QStringLiteral("无法加载图片"), this);
        errorLabel->setAlignment(Qt::AlignCenter);
        errorLabel->setStyleSheet(QStringLiteral("color:#999; font-size:14px;"));
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(errorLabel);
        resize(400, 300);
        return;
    }

    // 根据图片和屏幕大小决定窗口尺寸
    const QSize screenSize = QApplication::primaryScreen()->availableSize();
    const int maxW = qMin(screenSize.width() * 4 / 5, m_originalPixmap.width() + 40);
    const int maxH = qMin(screenSize.height() * 4 / 5, m_originalPixmap.height() + 40);
    resize(qMax(520, maxW), qMax(400, maxH));
    setMinimumSize(400, 300);

    setStyleSheet(QStringLiteral("ImageViewerWidget { background: #1a1a1a; }"));

    // ─── 图片显示区（占满整个窗口）───
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_scrollArea = new ElaScrollArea(this);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: #1a1a1a; border: none; }"
        "QScrollBar:vertical { width:6px; background:transparent; }"
        "QScrollBar::handle:vertical { background:rgba(255,255,255,60); border-radius:3px; min-height:30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        "QScrollBar:horizontal { height:6px; background:transparent; }"
        "QScrollBar::handle:horizontal { background:rgba(255,255,255,60); border-radius:3px; min-width:30px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }"
    ));

    m_imageLabel = new ElaText(m_scrollArea);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet(QStringLiteral("background:transparent;"));
    m_scrollArea->setWidget(m_imageLabel);
    layout->addWidget(m_scrollArea, 1);

    // ─── 右上角悬浮按钮组 ───
    m_topOverlay = new QWidget(this);
    m_topOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_topOverlay->setFixedHeight(40);

    const QString overlayBtnStyle = QStringLiteral(
        "ElaToolButton { background:rgba(0,0,0,160); border:none; border-radius:6px;"
        "  color:#ddd; font-size:16px; }"
        "ElaToolButton:hover { background:rgba(255,255,255,50); }");

    auto* topLayout = new QHBoxLayout(m_topOverlay);
    topLayout->setContentsMargins(12, 8, 12, 0);

    // 文件名标签（左侧）
    auto* nameLabel = new ElaText(displayTitle, m_topOverlay);
    nameLabel->setStyleSheet(QStringLiteral(
        "color:rgba(255,255,255,200); font-size:12px; background:transparent;"));
    topLayout->addWidget(nameLabel);
    topLayout->addStretch();

    auto makeOverlayBtn = [this, &overlayBtnStyle](const QString& text, const QString& tip) {
        auto* btn = new ElaToolButton(m_topOverlay);
        btn->setText(text);
        btn->setFixedSize(32, 32);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tip);
        btn->setStyleSheet(overlayBtnStyle);
        return btn;
    };

    auto* minimizeBtn = makeOverlayBtn(QStringLiteral("─"), QStringLiteral("最小化"));
    auto* maximizeBtn = makeOverlayBtn(QStringLiteral("□"), QStringLiteral("最大化"));
    auto* closeBtn = makeOverlayBtn(QStringLiteral("✕"), QStringLiteral("关闭"));
    closeBtn->setStyleSheet(QStringLiteral(
        "ElaToolButton { background:rgba(0,0,0,160); border:none; border-radius:6px;"
        "  color:#ddd; font-size:16px; }"
        "ElaToolButton:hover { background:#e81123; color:white; }"));

    topLayout->addWidget(minimizeBtn);
    topLayout->addWidget(maximizeBtn);
    topLayout->addWidget(closeBtn);

    connect(minimizeBtn, &ElaToolButton::clicked, this, &QWidget::showMinimized);
    connect(maximizeBtn, &ElaToolButton::clicked, this, [this]() {
        isMaximized() ? showNormal() : showMaximized();
    });
    connect(closeBtn, &ElaToolButton::clicked, this, &QWidget::close);

    // ─── 底部悬浮工具栏 ───
    m_bottomBar = new QWidget(this);
    m_bottomBar->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_bottomBar->setFixedHeight(48);
    m_bottomBar->setStyleSheet(QStringLiteral(
        "background:rgba(0,0,0,160); border-radius:12px;"));

    auto* shadow = new QGraphicsDropShadowEffect(m_bottomBar);
    shadow->setBlurRadius(16);
    shadow->setOffset(0, 2);
    shadow->setColor(QColor(0, 0, 0, 100));
    m_bottomBar->setGraphicsEffect(shadow);

    auto* tbLayout = new QHBoxLayout(m_bottomBar);
    tbLayout->setContentsMargins(16, 0, 16, 0);
    tbLayout->setSpacing(6);

    const QString tbBtnStyle = QStringLiteral(
        "ElaToolButton { background:transparent; border:none; border-radius:6px;"
        "  color:#ddd; font-size:15px; padding:4px 12px; }"
        "ElaToolButton:hover { background:rgba(255,255,255,40); }"
        "ElaToolButton:pressed { background:rgba(255,255,255,20); }");

    auto makeTbBtn = [this, &tbBtnStyle](const QString& text, const QString& tip) {
        auto* btn = new ElaToolButton(m_bottomBar);
        btn->setText(text);
        btn->setFixedHeight(32);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tip);
        btn->setStyleSheet(tbBtnStyle);
        return btn;
    };

    auto* zoomOutBtn = makeTbBtn(QStringLiteral("−"), QStringLiteral("缩小 (-)"));
    auto* fitBtn = makeTbBtn(QStringLiteral("适应窗口"), QStringLiteral("适应窗口大小"));
    auto* resetBtn = makeTbBtn(QStringLiteral("1:1"), QStringLiteral("原始大小"));
    auto* zoomInBtn = makeTbBtn(QStringLiteral("+"), QStringLiteral("放大 (+)"));

    m_zoomLabel = new ElaText(QStringLiteral("100%"), m_bottomBar);
    m_zoomLabel->setFixedWidth(50);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setStyleSheet(QStringLiteral("color:rgba(255,255,255,180); font-size:12px; background:transparent;"));

    auto* sizeLabel = new ElaText(
        QStringLiteral("%1×%2").arg(m_originalPixmap.width()).arg(m_originalPixmap.height()),
        m_bottomBar);
    sizeLabel->setStyleSheet(QStringLiteral("color:rgba(255,255,255,120); font-size:11px; background:transparent;"));

    tbLayout->addWidget(zoomOutBtn);
    tbLayout->addWidget(fitBtn);
    tbLayout->addWidget(m_zoomLabel);
    tbLayout->addWidget(resetBtn);
    tbLayout->addWidget(zoomInBtn);
    tbLayout->addSpacing(8);
    tbLayout->addWidget(sizeLabel);

    connect(zoomInBtn, &ElaToolButton::clicked, this, &ImageViewerWidget::zoomIn);
    connect(zoomOutBtn, &ElaToolButton::clicked, this, &ImageViewerWidget::zoomOut);
    connect(resetBtn, &ElaToolButton::clicked, this, &ImageViewerWidget::resetZoom);
    connect(fitBtn, &ElaToolButton::clicked, this, &ImageViewerWidget::fitToWindow);

    // ─── 悬浮控件默认隐藏，鼠标移入显示 ───
    m_topOverlay->hide();
    m_bottomBar->hide();

    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(2500);
    connect(&m_hideTimer, &QTimer::timeout, this, &ImageViewerWidget::hideOverlays);

    setMouseTracking(true);
    m_scrollArea->setMouseTracking(true);
    m_scrollArea->viewport()->setMouseTracking(true);
    m_imageLabel->setMouseTracking(true);

    setFocusPolicy(Qt::StrongFocus);
    setFocus();
}

void ImageViewerWidget::layoutOverlays()
{
    if (!m_topOverlay || !m_bottomBar)
        return;
    m_topOverlay->setGeometry(0, 0, width(), 40);
    const int barW = qMin(420, width() - 40);
    m_bottomBar->setFixedWidth(barW);
    m_bottomBar->move((width() - barW) / 2, height() - 60);
}

void ImageViewerWidget::showOverlays()
{
    if (m_topOverlay) { m_topOverlay->show(); m_topOverlay->raise(); }
    if (m_bottomBar) { m_bottomBar->show(); m_bottomBar->raise(); }
    m_hideTimer.start();
}

void ImageViewerWidget::hideOverlays()
{
    // 不隐藏如果鼠标在 overlay 上
    if (m_topOverlay && m_topOverlay->underMouse()) { m_hideTimer.start(); return; }
    if (m_bottomBar && m_bottomBar->underMouse()) { m_hideTimer.start(); return; }
    if (m_topOverlay) m_topOverlay->hide();
    if (m_bottomBar) m_bottomBar->hide();
}

void ImageViewerWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_firstShow && !m_originalPixmap.isNull()) {
        m_firstShow = false;
        layoutOverlays();
        fitToWindow();
        // 初始短暂显示控件
        showOverlays();
    }
}

void ImageViewerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutOverlays();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ImageViewerWidget::enterEvent(QEnterEvent* event)
#else
void ImageViewerWidget::enterEvent(QEvent* event)
#endif
{
    QWidget::enterEvent(event);
    showOverlays();
}

void ImageViewerWidget::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    m_hideTimer.start(800);
}

void ImageViewerWidget::fitToWindow()
{
    if (m_originalPixmap.isNull() || !m_scrollArea)
        return;
    const QSize viewSize = m_scrollArea->viewport()->size() - QSize(20, 20);
    if (viewSize.width() < 10 || viewSize.height() < 10)
        return;
    const QPixmap scaled = m_originalPixmap.scaled(viewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_zoomFactor = static_cast<double>(scaled.width()) / m_originalPixmap.width();
    m_imageLabel->setPixmap(scaled);
    m_imageLabel->resize(scaled.size());
    updateZoomLabel();
}

void ImageViewerWidget::updateDisplay()
{
    if (m_originalPixmap.isNull() || !m_imageLabel)
        return;
    if (m_zoomFactor <= 0) {
        fitToWindow();
        return;
    }
    const int w = qRound(m_originalPixmap.width() * m_zoomFactor);
    const int h = qRound(m_originalPixmap.height() * m_zoomFactor);
    const QPixmap scaled = m_originalPixmap.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_imageLabel->setPixmap(scaled);
    m_imageLabel->resize(scaled.size());
    updateZoomLabel();
}

void ImageViewerWidget::updateZoomLabel()
{
    if (m_zoomLabel && m_zoomFactor > 0)
        m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(m_zoomFactor * 100)));
}

void ImageViewerWidget::zoomIn()
{
    if (m_zoomFactor <= 0)
        m_zoomFactor = static_cast<double>(m_imageLabel->pixmap().width()) / m_originalPixmap.width();
    m_zoomFactor = qMin(m_zoomFactor * 1.25, 8.0);
    updateDisplay();
}

void ImageViewerWidget::zoomOut()
{
    if (m_zoomFactor <= 0)
        m_zoomFactor = static_cast<double>(m_imageLabel->pixmap().width()) / m_originalPixmap.width();
    m_zoomFactor = qMax(m_zoomFactor / 1.25, 0.05);
    updateDisplay();
}

void ImageViewerWidget::resetZoom()
{
    m_zoomFactor = 1.0;
    updateDisplay();
}

void ImageViewerWidget::wheelEvent(QWheelEvent* event)
{
    if (event->angleDelta().y() > 0)
        zoomIn();
    else
        zoomOut();
    showOverlays();
    event->accept();
}

void ImageViewerWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStart = event->globalPosition().toPoint();
        // 判断是拖拽图片还是拖拽窗口（图片小于视口时拖窗口）
        const bool imageSmall = m_imageLabel && m_scrollArea
            && m_imageLabel->width() <= m_scrollArea->viewport()->width()
            && m_imageLabel->height() <= m_scrollArea->viewport()->height();
        if (imageSmall) {
            m_scrollStart = pos();  // 窗口位置，用于拖动窗口
        } else {
            m_scrollStart = QPoint(m_scrollArea->horizontalScrollBar()->value(),
                                   m_scrollArea->verticalScrollBar()->value());
        }
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImageViewerWidget::mouseMoveEvent(QMouseEvent* event)
{
    showOverlays();
    if (m_dragging) {
        const QPoint globalPos = event->globalPosition().toPoint();
        const QPoint delta = globalPos - m_dragStart;
        const bool imageSmall = m_imageLabel && m_scrollArea
            && m_imageLabel->width() <= m_scrollArea->viewport()->width()
            && m_imageLabel->height() <= m_scrollArea->viewport()->height();
        if (imageSmall) {
            move(m_scrollStart + delta);  // 拖动窗口
        } else {
            m_scrollArea->horizontalScrollBar()->setValue(m_scrollStart.x() - delta.x());
            m_scrollArea->verticalScrollBar()->setValue(m_scrollStart.y() - delta.y());
        }
    }
}

void ImageViewerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void ImageViewerWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    fitToWindow();
}

void ImageViewerWidget::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomIn();
        break;
    case Qt::Key_Minus:
        zoomOut();
        break;
    case Qt::Key_0:
    case Qt::Key_1:
        resetZoom();
        break;
    case Qt::Key_Escape:
        close();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}
