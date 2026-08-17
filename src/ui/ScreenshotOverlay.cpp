#include "ui/ScreenshotOverlay.h"

#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <ElaText.h>
#include <QMouseEvent>
#include <QPainter>
#include <ElaPushButton.h>
#include <QScreen>
#include <QtMath>

// ═══════════════════════════════════════════════════════════════
//   ScreenshotOverlay
// ═══════════════════════════════════════════════════════════════

ScreenshotOverlay::ScreenshotOverlay(QWidget* parent)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    Q_UNUSED(parent)
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);

    QScreen* primaryScreen = QGuiApplication::primaryScreen();
    if (primaryScreen) {
        QRect virtualGeometry;
        const auto screens = QGuiApplication::screens();
        for (QScreen* screen : screens) {
            virtualGeometry = virtualGeometry.united(screen->geometry());
        }
        // 多屏场景下按每个屏幕分别抓取并拼接，避免主屏坐标系裁剪导致副屏缺失。
        if (virtualGeometry.isValid()) {
            QImage stitched(virtualGeometry.size(), QImage::Format_ARGB32_Premultiplied);
            stitched.fill(Qt::black);
            QPainter painter(&stitched);
            for (QScreen* screen : screens) {
                if (!screen) {
                    continue;
                }
                const QPixmap pix = screen->grabWindow(0);
                const QPoint offset = screen->geometry().topLeft() - virtualGeometry.topLeft();
                painter.drawPixmap(offset, pix);
            }
            painter.end();
            m_fullScreen = stitched;
        }
        if (m_fullScreen.isNull()) {
            // 兜底：至少保证主屏可截。
            m_fullScreen = primaryScreen->grabWindow(0).toImage();
        }
        setGeometry(virtualGeometry);
    }

    m_sizeLabel = new ElaText(this);
    m_sizeLabel->setStyleSheet(
        QStringLiteral("QLabel{"
                       "background:rgba(30,38,50,0.85);"
                       "color:#E8EEF8;"
                       "font-size:12px;"
                       "padding:3px 8px;"
                       "border-radius:3px;"
                       "}"));
    m_sizeLabel->hide();

    buildToolbar();
    m_toolbar->hide();

    // 注意：不能用 showFullScreen()，它会把窗口强制放到主屏幕上，
    // 导致多屏场景下副屏无法被覆盖。用 show() 保留 setGeometry() 的跨屏区域。
    show();
    raise();
    activateWindow();
    setFocus(Qt::ActiveWindowFocusReason);
    grabKeyboard();
}

QImage ScreenshotOverlay::croppedImage() const { return m_cropped; }
bool ScreenshotOverlay::wasAccepted() const { return m_accepted; }
QRect ScreenshotOverlay::selectedRect() const { return QRect(m_origin, m_current).normalized(); }

// ─── 工具栏构建 ────────────────────────────────────────────
void ScreenshotOverlay::buildToolbar()
{
    m_toolbar = new QWidget(this);
    m_toolbar->setObjectName(QStringLiteral("overlayToolbar"));
    m_toolbar->setStyleSheet(
        QStringLiteral("QWidget#overlayToolbar{"
                       "background:rgba(30,38,50,0.94);"
                       "border-radius:8px;"
                       "}"));

    auto* layout = new QHBoxLayout(m_toolbar);
    layout->setContentsMargins(8, 5, 8, 5);
    layout->setSpacing(2);

    const QString btnStyle = QStringLiteral(
        "QPushButton{background:transparent;color:#B6C3D8;border:none;"
        "border-radius:5px;font-size:18px;padding:4px 8px;min-width:34px;min-height:30px;}"
        "QPushButton:hover{background:rgba(255,255,255,0.12);color:#E8EEF8;}"
        "QPushButton:checked{background:rgba(47,111,237,0.35);color:#72A4FF;}");

    // 标注工具
    struct ToolDef { QString label; OverlayAnnotationTool tool; QString tip; };
    const ToolDef tools[] = {
        {QStringLiteral("\u25AD"), OverlayAnnotationTool::Rectangle, QStringLiteral("\u77E9\u5F62")},
        {QStringLiteral("\u25CB"), OverlayAnnotationTool::Ellipse,   QStringLiteral("\u692D\u5706")},
        {QStringLiteral("\u2192"), OverlayAnnotationTool::Arrow,     QStringLiteral("\u7BAD\u5934")},
        {QStringLiteral("\u270F"), OverlayAnnotationTool::Freehand,  QStringLiteral("\u753B\u7B14")},
        {QStringLiteral("T"),     OverlayAnnotationTool::Text,       QStringLiteral("\u6587\u5B57")},
        {QStringLiteral("\u2592"), OverlayAnnotationTool::Mosaic,    QStringLiteral("\u9A6C\u8D5B\u514B")},
    };
    for (const auto& def : tools) {
        auto* btn = new ElaPushButton(def.label, m_toolbar);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(btnStyle);
        btn->setToolTip(def.tip);
        connect(btn, &QPushButton::clicked, this, [this, t = def.tool]() { setActiveTool(t); });
        layout->addWidget(btn);
        m_toolButtons.append(btn);
    }

    // 分隔线
    auto addSep = [&]() {
        auto* sep = new QWidget(m_toolbar);
        sep->setFixedSize(1, 18);
        sep->setStyleSheet(QStringLiteral("background:rgba(255,255,255,0.15);"));
        layout->addWidget(sep);
    };
    addSep();

    // 快捷色板
    const QColor colors[] = {
        Qt::red, QColor(0x2F,0x6F,0xED), QColor(0x00,0xC8,0x53),
        QColor(0xF5,0x9E,0x0B), Qt::white, QColor(0x11,0x18,0x27)
    };
    for (const auto& c : colors) {
        auto* btn = new ElaPushButton(m_toolbar);
        btn->setFixedSize(18, 18);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QStringLiteral("QPushButton{background:%1;border:2px solid transparent;"
                           "border-radius:9px;min-width:18px;min-height:18px;}"
                           "QPushButton:hover{border-color:rgba(255,255,255,0.5);}")
                .arg(c.name()));
        btn->setProperty("colorValue", c);
        connect(btn, &QPushButton::clicked, this, [this, c]() {
            m_penColor = c;
            for (auto* cb : m_colorButtons) {
                const QColor bc = cb->property("colorValue").value<QColor>();
                cb->setStyleSheet(
                    QStringLiteral("QPushButton{background:%1;border:2px solid %2;"
                                   "border-radius:9px;min-width:18px;min-height:18px;}"
                                   "QPushButton:hover{border-color:rgba(255,255,255,0.5);}")
                        .arg(bc.name(), bc == c ? QStringLiteral("#72A4FF") : QStringLiteral("transparent")));
            }
        });
        layout->addWidget(btn);
        m_colorButtons.append(btn);
    }

    addSep();

    // 撤销
    auto* undoBtn = new ElaPushButton(QStringLiteral("\u21A9"), m_toolbar);
    undoBtn->setCursor(Qt::PointingHandCursor);
    undoBtn->setStyleSheet(btnStyle);
    undoBtn->setToolTip(QStringLiteral("\u64A4\u9500"));
    connect(undoBtn, &QPushButton::clicked, this, [this]() {
        if (!m_annotations.isEmpty()) {
            m_annotations.removeLast();
            update();
        }
    });
    layout->addWidget(undoBtn);

    addSep();

    // 确认 / 取消
    auto* cancelBtn = new ElaPushButton(QStringLiteral("\u2715"), m_toolbar);
    cancelBtn->setFixedSize(34, 30);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setStyleSheet(
        QStringLiteral("QPushButton{background:rgba(255,255,255,0.08);color:#FF6C68;"
                       "border:none;border-radius:5px;font-size:16px;font-weight:700;"
                       "min-width:34px;min-height:30px;}"
                       "QPushButton:hover{background:rgba(255,108,104,0.2);}"));
    connect(cancelBtn, &QPushButton::clicked, this, [this]() { finish(false); });
    layout->addWidget(cancelBtn);

    auto* confirmBtn = new ElaPushButton(QStringLiteral("\u2713"), m_toolbar);
    confirmBtn->setFixedSize(34, 30);
    confirmBtn->setCursor(Qt::PointingHandCursor);
    confirmBtn->setStyleSheet(
        QStringLiteral("QPushButton{background:#2F6FED;color:white;border:none;"
                       "border-radius:5px;font-size:16px;font-weight:700;"
                       "min-width:34px;min-height:30px;}"
                       "QPushButton:hover{background:#417CF0;}"));
    connect(confirmBtn, &QPushButton::clicked, this, [this]() { finish(true); });
    layout->addWidget(confirmBtn);

    m_toolbar->adjustSize();
}

void ScreenshotOverlay::repositionToolbar()
{
    if (!m_toolbar) return;
    const QRect sel = selectedRect();
    const int tbW = m_toolbar->sizeHint().width();
    const int tbH = m_toolbar->sizeHint().height();
    const int margin = 6;

    // 水平定位：靠齐选区右侧，但不超出左/右边界
    int tbX = sel.right() - tbW;
    if (tbX < sel.left()) tbX = sel.left();           // 尝试对齐选区左边
    if (tbX < 0) tbX = 0;                             // 不超出左屏幕边界
    if (tbX + tbW > width()) tbX = width() - tbW;     // 不超出右屏幕边界（最高优先）
    if (tbX < 0) tbX = 0;                             // 极端：toolbar 比屏幕宽

    // 垂直定位回退链：选区下方 → 选区上方 → 选区内部底部
    int tbY = sel.bottom() + margin;                       // 优先：选区正下方
    if (tbY + tbH > height()) {
        tbY = sel.top() - tbH - margin;                    // 备选：选区正上方
    }
    if (tbY < 0) {
        tbY = sel.bottom() - tbH - margin;                 // 最终：选区内部底部
        if (tbY < sel.top()) tbY = sel.top() + margin;     // 极端：贴选区顶部
    }

    m_toolbar->move(tbX, tbY);
    m_toolbar->show();
    m_toolbar->raise();
}

void ScreenshotOverlay::enterEditMode()
{
    m_editMode = true;
    setCursor(Qt::ArrowCursor);
    repositionToolbar();
}

void ScreenshotOverlay::setActiveTool(OverlayAnnotationTool tool)
{
    m_activeTool = tool;
    updateToolButtonStates();
    if (tool != OverlayAnnotationTool::None) {
        setCursor(Qt::CrossCursor);
    }
}

void ScreenshotOverlay::updateToolButtonStates()
{
    for (auto* btn : m_toolButtons) {
        btn->setChecked(false);
    }
    const int idx = static_cast<int>(m_activeTool) - 1;
    if (idx >= 0 && idx < m_toolButtons.size()) {
        m_toolButtons[idx]->setChecked(true);
    }
}

// ─── 绘制 ──────────────────────────────────────────────────
void ScreenshotOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 全屏截图 + 遮罩
    painter.drawImage(0, 0, m_fullScreen);
    painter.fillRect(rect(), QColor(0, 0, 0, 80));

    if (m_selecting || m_hasSelection) {
        const QRect sel = selectedRect();
        if (sel.width() > 0 && sel.height() > 0) {
            // 选区亮度还原
            painter.drawImage(sel, m_fullScreen, sel);
            // 绘制已有标注
            painter.setClipRect(sel);
            for (const auto& ann : m_annotations) {
                drawAnnotation(painter, ann);
            }
            // 正在绘制中的标注
            if (m_drawing && m_activeTool != OverlayAnnotationTool::None) {
                drawAnnotation(painter, m_currentAnnotation);
            }
            painter.setClipping(false);
            // 选区边框
            QPen borderPen(QColor(47, 111, 237), 1.5);
            painter.setPen(borderPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(sel);
        }
    }
}

void ScreenshotOverlay::drawAnnotation(QPainter& painter, const OverlayAnnotation& ann) const
{
    QPen pen(ann.color, ann.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const QRectF r = QRectF(ann.start, ann.end).normalized();

    switch (ann.tool) {
    case OverlayAnnotationTool::Rectangle:
        painter.drawRect(r);
        break;
    case OverlayAnnotationTool::Ellipse:
        painter.drawEllipse(r);
        break;
    case OverlayAnnotationTool::Arrow: {
        const QLineF line(ann.start, ann.end);
        painter.drawLine(line);
        const qreal arrowSize = qMax(12.0, ann.width * 4.0);
        const qreal angle = std::atan2(-(line.dy()), line.dx());
        const QPointF p1 = line.p2() - QPointF(std::cos(angle - M_PI / 6.0) * arrowSize,
                                                -std::sin(angle - M_PI / 6.0) * arrowSize);
        const QPointF p2 = line.p2() - QPointF(std::cos(angle + M_PI / 6.0) * arrowSize,
                                                -std::sin(angle + M_PI / 6.0) * arrowSize);
        painter.setBrush(ann.color);
        painter.drawPolygon(QPolygonF({line.p2(), p1, p2}));
        painter.setBrush(Qt::NoBrush);
        break;
    }
    case OverlayAnnotationTool::Freehand:
        painter.drawPath(ann.path);
        break;
    case OverlayAnnotationTool::Text: {
        QFont font;
        font.setPointSize(qMax(12, ann.width * 4));
        painter.setFont(font);
        painter.drawText(ann.start, ann.text);
        break;
    }
    case OverlayAnnotationTool::Mosaic:
        if (!ann.mosaicImage.isNull()) {
            painter.drawImage(ann.mosaicRect, ann.mosaicImage);
        }
        break;
    default:
        break;
    }
}

// ─── 鼠标事件 ──────────────────────────────────────────────
void ScreenshotOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        if (m_editMode) {
            // 右键退出编辑工具
            setActiveTool(OverlayAnnotationTool::None);
            setCursor(Qt::ArrowCursor);
        } else {
            finish(false);
        }
        return;
    }
    if (event->button() != Qt::LeftButton) return;

    if (m_editMode && m_activeTool != OverlayAnnotationTool::None) {
        // 编辑模式下的绘制
        const QRect sel = selectedRect();
        if (!sel.contains(event->pos())) return;

        if (m_activeTool == OverlayAnnotationTool::Text) {
            // 文字工具：按住拖动定位，松开后输入文字（在 mouseReleaseEvent 处理）
            m_drawing = true;
            m_drawStart = event->pos();
            m_currentAnnotation = {};
            m_currentAnnotation.tool = OverlayAnnotationTool::Text;
            m_currentAnnotation.color = m_penColor;
            m_currentAnnotation.width = m_penWidth;
            m_currentAnnotation.start = m_drawStart;
            m_currentAnnotation.end = m_drawStart;
            m_currentAnnotation.text = QStringLiteral("\u6587\u5B57");
            return;
        }

        m_drawing = true;
        m_drawStart = event->pos();
        m_currentAnnotation = {};
        m_currentAnnotation.tool = m_activeTool;
        m_currentAnnotation.color = m_penColor;
        m_currentAnnotation.width = m_penWidth;
        m_currentAnnotation.start = m_drawStart;
        m_currentAnnotation.end = m_drawStart;

        if (m_activeTool == OverlayAnnotationTool::Freehand) {
            m_currentAnnotation.path.moveTo(m_drawStart);
        }
        return;
    }

    if (!m_editMode) {
        // 选区模式
        m_origin = event->pos();
        m_current = event->pos();
        m_selecting = true;
        m_hasSelection = false;
        m_editMode = false;
        m_annotations.clear();
        m_toolbar->hide();
        m_sizeLabel->hide();
        update();
    }
}

void ScreenshotOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (m_drawing) {
        const QPointF pos = event->pos();
        m_currentAnnotation.end = pos;

        if (m_activeTool == OverlayAnnotationTool::Text) {
            // 文字拖动：移动预览位置
            m_currentAnnotation.start = pos;
            update();
            return;
        }
        if (m_activeTool == OverlayAnnotationTool::Freehand) {
            m_currentAnnotation.path.lineTo(pos);
        } else if (m_activeTool == OverlayAnnotationTool::Mosaic) {
            const QRectF r = QRectF(m_currentAnnotation.start, pos).normalized();
            const QRect intRect = r.toAlignedRect().intersected(m_fullScreen.rect());
            if (!intRect.isEmpty()) {
                QImage region = m_fullScreen.copy(intRect);
                const int bsz = 10;
                const int sw = qMax(1, region.width() / bsz);
                const int sh = qMax(1, region.height() / bsz);
                m_currentAnnotation.mosaicImage =
                    region.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                          .scaled(region.width(), region.height(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
                m_currentAnnotation.mosaicRect = r;
            }
        }
        update();
        return;
    }

    if (m_selecting) {
        m_current = event->pos();
        updateSizeLabel();
        update();
    }
}

void ScreenshotOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;

    if (m_drawing) {
        m_drawing = false;
        m_currentAnnotation.end = event->pos();

        // 文字工具：松开后弹出输入框
        if (m_currentAnnotation.tool == OverlayAnnotationTool::Text) {
            const QPointF textPos = m_currentAnnotation.end;
            bool ok = false;
            QPointer<ScreenshotOverlay> guard(this);
            const QString text = QInputDialog::getText(
                this, QStringLiteral("\u6DFB\u52A0\u6587\u5B57"),
                QStringLiteral("\u8BF7\u8F93\u5165\u6807\u6CE8:"),
                QLineEdit::Normal, QString(), &ok);
            if (!guard) return;
            if (ok && !text.trimmed().isEmpty()) {
                m_currentAnnotation.start = textPos;
                m_currentAnnotation.text = text;
                m_annotations.append(m_currentAnnotation);
            }
            update();
            return;
        }

        // 其他工具：只保留有效标注
        const QRectF r = QRectF(m_currentAnnotation.start, m_currentAnnotation.end).normalized();
        if (r.width() > 2 || r.height() > 2
            || m_currentAnnotation.tool == OverlayAnnotationTool::Freehand) {
            m_annotations.append(m_currentAnnotation);
        }
        update();
        return;
    }

    if (!m_selecting) return;
    m_selecting = false;
    m_current = event->pos();

    const QRect sel = selectedRect();
    if (sel.width() < 3 || sel.height() < 3) {
        m_hasSelection = false;
        m_sizeLabel->hide();
        m_toolbar->hide();
        update();
        return;
    }

    m_hasSelection = true;
    updateSizeLabel();
    enterEditMode();
    update();
}

void ScreenshotOverlay::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_editMode && m_activeTool != OverlayAnnotationTool::None) {
            setActiveTool(OverlayAnnotationTool::None);
            setCursor(Qt::ArrowCursor);
        } else {
            finish(false);
        }
    } else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
               && m_hasSelection) {
        finish(true);
    } else if (event->key() == Qt::Key_Z && (event->modifiers() & Qt::ControlModifier)) {
        if (!m_annotations.isEmpty()) {
            m_annotations.removeLast();
            update();
        }
    }
}

// ─── 完成 ──────────────────────────────────────────────────
void ScreenshotOverlay::finish(bool accept)
{
    if (m_finishing) return;  // 防止重入（QInputDialog 嵌套事件循环 + Enter 键传播）
    m_finishing = true;
    releaseKeyboard();
    m_accepted = accept;
    if (accept && m_hasSelection) {
        m_cropped = renderFinalImage();
    }
    hide();
    emit accept ? accepted() : rejected();
    close();
}

QImage ScreenshotOverlay::renderFinalImage() const
{
    const QRect sel = selectedRect();
    if (!sel.isValid() || m_fullScreen.isNull()) return {};

    QImage result = m_fullScreen.copy(sel.intersected(m_fullScreen.rect()));
    if (!m_annotations.isEmpty()) {
        QPainter painter(&result);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.translate(-sel.topLeft());
        for (const auto& ann : m_annotations) {
            drawAnnotation(painter, ann);
        }
    }
    return result;
}

void ScreenshotOverlay::updateSizeLabel()
{
    const QRect sel = selectedRect();
    if (sel.width() < 1 || sel.height() < 1) {
        m_sizeLabel->hide();
        return;
    }
    m_sizeLabel->setText(QStringLiteral("%1 \u00D7 %2").arg(sel.width()).arg(sel.height()));
    m_sizeLabel->adjustSize();
    int lx = sel.left();
    int ly = sel.top() - m_sizeLabel->height() - 4;
    if (ly < 0) ly = sel.top() + 4;
    m_sizeLabel->move(lx, ly);
    m_sizeLabel->show();
    m_sizeLabel->raise();
}
