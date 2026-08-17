#include "ui/ScreenshotEditorWindow.h"
#include "ui/AppStyle.h"

#include <QAction>
#include <QActionGroup>
#include <QColorDialog>
#include <QGraphicsPixmapItem>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <ElaPushButton.h>
#include <QSpinBox>
#include <QToolBar>
#include <QVBoxLayout>
#include <QtMath>
#include <QGraphicsDropShadowEffect>

// ═══════════════════════════════════════════════════════════════
//   Custom Graphics Items
// ═══════════════════════════════════════════════════════════════

RectAnnotationItem::RectAnnotationItem(const QRectF& rect, const QPen& pen, QGraphicsItem* parent)
    : QGraphicsRectItem(rect, parent)
{
    setPen(pen);
    setBrush(Qt::NoBrush);
}

EllipseAnnotationItem::EllipseAnnotationItem(const QRectF& rect, const QPen& pen, QGraphicsItem* parent)
    : QGraphicsEllipseItem(rect, parent)
{
    setPen(pen);
    setBrush(Qt::NoBrush);
}

ArrowAnnotationItem::ArrowAnnotationItem(const QLineF& line, const QPen& pen, QGraphicsItem* parent)
    : QGraphicsLineItem(line, parent)
{
    setPen(pen);
}

void ArrowAnnotationItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);
    const QPen p = pen();
    painter->setPen(p);

    const QLineF l = line();
    painter->drawLine(l);

    // Draw arrowhead
    const qreal arrowSize = qMax(12.0, p.widthF() * 4.0);
    const qreal angle = std::atan2(-(l.dy()), l.dx());

    const QPointF arrowP1 = l.p2() - QPointF(std::cos(angle - M_PI / 6.0) * arrowSize,
                                               -std::sin(angle - M_PI / 6.0) * arrowSize);
    const QPointF arrowP2 = l.p2() - QPointF(std::cos(angle + M_PI / 6.0) * arrowSize,
                                               -std::sin(angle + M_PI / 6.0) * arrowSize);

    painter->setBrush(p.color());
    painter->drawPolygon(QPolygonF({l.p2(), arrowP1, arrowP2}));
}

FreehandAnnotationItem::FreehandAnnotationItem(const QPen& pen, QGraphicsItem* parent)
    : QGraphicsPathItem(parent)
{
    setPen(pen);
    setBrush(Qt::NoBrush);
}

void FreehandAnnotationItem::addPoint(const QPointF& point)
{
    if (m_path.elementCount() == 0) {
        m_path.moveTo(point);
    } else {
        m_path.lineTo(point);
    }
    setPath(m_path);
}

MosaicAnnotationItem::MosaicAnnotationItem(const QImage& sourceImage, const QRectF& rect,
                                             int blockSize, QGraphicsItem* parent)
    : QGraphicsItem(parent), m_rect(rect)
{
    // Generate pixelated version of the region
    const QRect intRect = rect.toAlignedRect().intersected(sourceImage.rect());
    if (intRect.isEmpty()) {
        return;
    }
    QImage region = sourceImage.copy(intRect);
    const int smallW = qMax(1, region.width() / blockSize);
    const int smallH = qMax(1, region.height() / blockSize);
    m_mosaicImage = region.scaled(smallW, smallH, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                         .scaled(region.width(), region.height(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

QRectF MosaicAnnotationItem::boundingRect() const
{
    return m_rect;
}

void MosaicAnnotationItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (!m_mosaicImage.isNull()) {
        painter->drawImage(m_rect, m_mosaicImage);
    }
}

// ═══════════════════════════════════════════════════════════════
//   Undo Command
// ═══════════════════════════════════════════════════════════════

AddAnnotationCommand::AddAnnotationCommand(QGraphicsScene* scene, QGraphicsItem* item, QUndoCommand* parent)
    : QUndoCommand(parent), m_scene(scene), m_item(item)
{
    setText(QStringLiteral("Add annotation"));
}

AddAnnotationCommand::~AddAnnotationCommand()
{
    if (m_ownsItem) {
        delete m_item;
    }
}

void AddAnnotationCommand::undo()
{
    m_scene->removeItem(m_item);
    m_ownsItem = true;
}

void AddAnnotationCommand::redo()
{
    if (m_ownsItem) {
        m_scene->addItem(m_item);
        m_ownsItem = false;
    }
}

// ═══════════════════════════════════════════════════════════════
//   Editor View
// ═══════════════════════════════════════════════════════════════

ScreenshotEditorView::ScreenshotEditorView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent)
{
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
}

void ScreenshotEditorView::setActiveTool(AnnotationTool tool) { m_activeTool = tool; }
void ScreenshotEditorView::setPenColor(const QColor& color) { m_penColor = color; }
void ScreenshotEditorView::setPenWidth(int width) { m_penWidth = width; }
void ScreenshotEditorView::setUndoStack(QUndoStack* stack) { m_undoStack = stack; }
void ScreenshotEditorView::setSourceImage(const QImage& image) { m_sourceImage = image; }

void ScreenshotEditorView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_activeTool == AnnotationTool::None) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    m_drawing = true;
    m_startPoint = mapToScene(event->pos());
    const QPen pen(m_penColor, m_penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    switch (m_activeTool) {
    case AnnotationTool::Rectangle: {
        auto* item = new RectAnnotationItem(QRectF(m_startPoint, QSizeF(0, 0)), pen);
        scene()->addItem(item);
        m_currentItem = item;
        break;
    }
    case AnnotationTool::Ellipse: {
        auto* item = new EllipseAnnotationItem(QRectF(m_startPoint, QSizeF(0, 0)), pen);
        scene()->addItem(item);
        m_currentItem = item;
        break;
    }
    case AnnotationTool::Arrow: {
        auto* item = new ArrowAnnotationItem(QLineF(m_startPoint, m_startPoint), pen);
        scene()->addItem(item);
        m_currentItem = item;
        break;
    }
    case AnnotationTool::Freehand: {
        auto* item = new FreehandAnnotationItem(pen);
        item->addPoint(m_startPoint);
        scene()->addItem(item);
        m_currentItem = item;
        break;
    }
    case AnnotationTool::Text: {
        m_drawing = false;
        emit textAnnotationRequested(m_startPoint);
        break;
    }
    case AnnotationTool::Mosaic: {
        auto* item = new MosaicAnnotationItem(m_sourceImage,
                                               QRectF(m_startPoint, QSizeF(0, 0)));
        scene()->addItem(item);
        m_currentItem = item;
        break;
    }
    default:
        m_drawing = false;
        break;
    }
}

void ScreenshotEditorView::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_drawing || !m_currentItem) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    const QPointF currentPoint = mapToScene(event->pos());
    const QRectF rect = QRectF(m_startPoint, currentPoint).normalized();

    switch (m_activeTool) {
    case AnnotationTool::Rectangle:
        static_cast<RectAnnotationItem*>(m_currentItem)->setRect(rect);
        break;
    case AnnotationTool::Ellipse:
        static_cast<EllipseAnnotationItem*>(m_currentItem)->setRect(rect);
        break;
    case AnnotationTool::Arrow:
        static_cast<ArrowAnnotationItem*>(m_currentItem)->setLine(QLineF(m_startPoint, currentPoint));
        break;
    case AnnotationTool::Freehand:
        static_cast<FreehandAnnotationItem*>(m_currentItem)->addPoint(currentPoint);
        break;
    case AnnotationTool::Mosaic: {
        // Remove current and recreate with updated rect
        scene()->removeItem(m_currentItem);
        delete m_currentItem;
        auto* item = new MosaicAnnotationItem(m_sourceImage, rect);
        scene()->addItem(item);
        m_currentItem = item;
        break;
    }
    default:
        break;
    }
}

void ScreenshotEditorView::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_drawing || event->button() != Qt::LeftButton) {
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    m_drawing = false;

    if (m_currentItem && m_undoStack) {
        // Item is already in scene; first redo() is a no-op (m_ownsItem=false)
        m_undoStack->push(new AddAnnotationCommand(scene(), m_currentItem));
    }
    m_currentItem = nullptr;
}

// ═══════════════════════════════════════════════════════════════
//   Editor Window
// ═══════════════════════════════════════════════════════════════

ScreenshotEditorWindow::ScreenshotEditorWindow(const QImage& image, QWidget* parent)
    : ElaDialog(parent), m_sourceImage(image)
{
    setWindowTitle(QStringLiteral("\u622A\u56FE\u7F16\u8F91"));
    setMinimumSize(640, 480);
    resize(qMin(image.width() + 80, 1200), qMin(image.height() + 160, 800));
    setStyleSheet(QStringLiteral("QDialog{background:%1;}").arg(AppStyle::windowBg()));

    m_undoStack = new QUndoStack(this);

    // Scene + View
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(0, 0, image.width(), image.height());
    m_scene->addPixmap(QPixmap::fromImage(image));

    m_view = new ScreenshotEditorView(m_scene, this);
    m_view->setUndoStack(m_undoStack);
    m_view->setSourceImage(image);
    m_view->setStyleSheet(QStringLiteral(
        "QGraphicsView{border:none;background:%1;}")
        .arg(AppStyle::surfaceAlt()));

    connect(m_view, &ScreenshotEditorView::textAnnotationRequested,
            this, &ScreenshotEditorWindow::onTextAnnotation);

    // Layout: view 占满，浮动工具栏叠加在底部
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view, 1);

    buildFloatingToolbar();
}

// ─── 浮动工具栏 ─────────────────────────────────────────────
void ScreenshotEditorWindow::buildFloatingToolbar()
{
    m_floatingBar = new QWidget(this);
    m_floatingBar->setFixedHeight(44);
    m_floatingBar->setStyleSheet(
        QStringLiteral("QWidget#floatingBar{"
                       "background:rgba(30,38,50,0.92);"
                       "border-radius:10px;"
                       "}"));
    m_floatingBar->setObjectName(QStringLiteral("floatingBar"));

    // 阴影效果
    auto* shadow = new QGraphicsDropShadowEffect(m_floatingBar);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 60));
    shadow->setOffset(0, 4);
    m_floatingBar->setGraphicsEffect(shadow);

    auto* barLayout = new QHBoxLayout(m_floatingBar);
    barLayout->setContentsMargins(10, 6, 10, 6);
    barLayout->setSpacing(3);

    // ── 工具按钮样式 ──
    const QString toolBtnStyle = QStringLiteral(
        "QPushButton{background:transparent;color:#B6C3D8;border:none;"
        "border-radius:6px;font-size:15px;padding:4px 8px;min-width:32px;min-height:28px;}"
        "QPushButton:hover{background:rgba(255,255,255,0.12);color:#E8EEF8;}"
        "QPushButton:checked{background:rgba(47,111,237,0.35);color:#72A4FF;}");

    struct ToolDef {
        QString icon;
        AnnotationTool tool;
    };
    const ToolDef tools[] = {
        {QStringLiteral("\u25AD"),  AnnotationTool::Rectangle},  // ▭ 矩形
        {QStringLiteral("\u25CB"),  AnnotationTool::Ellipse},    // ○ 椭圆
        {QStringLiteral("\u2192"),  AnnotationTool::Arrow},      // → 箭头
        {QStringLiteral("\u270F"),  AnnotationTool::Freehand},   // ✏ 画笔
        {QStringLiteral("T"),      AnnotationTool::Text},        // T 文字
        {QStringLiteral("\u2592"), AnnotationTool::Mosaic},      // ▒ 马赛克
    };

    for (const auto& def : tools) {
        auto* btn = new ElaPushButton(def.icon, m_floatingBar);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(toolBtnStyle);
        btn->setProperty("annotationTool", static_cast<int>(def.tool));
        connect(btn, &QPushButton::clicked, this, [this, tool = def.tool]() {
            onToolSelected(tool);
        });
        barLayout->addWidget(btn);
        m_toolButtons.append(btn);
    }

    // ── 分隔线 ──
    auto addSep = [&]() {
        auto* sep = new QWidget(m_floatingBar);
        sep->setFixedSize(1, 22);
        sep->setStyleSheet(QStringLiteral("background:rgba(255,255,255,0.15);"));
        barLayout->addWidget(sep);
    };

    addSep();

    // ── 快捷颜色面板：6 色圆点 ──
    const QColor presetColors[] = {
        QColor(Qt::red),
        QColor(0x2F, 0x6F, 0xED),  // 蓝
        QColor(0x00, 0xC8, 0x53),  // 绿
        QColor(0xF5, 0x9E, 0x0B),  // 黄
        QColor(Qt::white),
        QColor(0x11, 0x18, 0x27),  // 黑
    };

    for (const auto& color : presetColors) {
        auto* btn = new ElaPushButton(m_floatingBar);
        btn->setFixedSize(22, 22);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QStringLiteral("QPushButton{background:%1;border:2px solid transparent;"
                           "border-radius:11px;min-width:22px;min-height:22px;}"
                           "QPushButton:hover{border-color:rgba(255,255,255,0.5);}")
                .arg(color.name()));
        btn->setProperty("colorValue", color);
        connect(btn, &QPushButton::clicked, this, [this, color]() {
            onColorPick(color);
        });
        barLayout->addWidget(btn);
        m_colorButtons.append(btn);
    }

    addSep();

    // ── 线宽：3 个预设 ──
    const int presetWidths[] = {2, 4, 7};
    const QString widthLabels[] = {
        QStringLiteral("\u2022"),    // 小圆点
        QStringLiteral("\u25CF"),    // 中圆点
        QStringLiteral("\u2B24"),    // 大圆点
    };

    for (int i = 0; i < 3; ++i) {
        auto* btn = new ElaPushButton(widthLabels[i], m_floatingBar);
        btn->setFixedSize(28, 28);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            QStringLiteral("QPushButton{background:transparent;color:#B6C3D8;border:none;"
                           "border-radius:6px;font-size:%1px;min-width:28px;min-height:28px;}"
                           "QPushButton:hover{background:rgba(255,255,255,0.12);color:#E8EEF8;}"
                           "QPushButton:checked{background:rgba(47,111,237,0.35);color:#72A4FF;}")
                .arg(10 + i * 4));
        btn->setProperty("widthValue", presetWidths[i]);
        connect(btn, &QPushButton::clicked, this, [this, w = presetWidths[i]]() {
            onWidthPick(w);
        });
        barLayout->addWidget(btn);
        m_widthButtons.append(btn);
    }

    addSep();

    // ── 撤销/重做 ──
    auto* undoBtn = new ElaPushButton(QStringLiteral("\u21A9"), m_floatingBar);
    undoBtn->setCursor(Qt::PointingHandCursor);
    undoBtn->setStyleSheet(toolBtnStyle);
    undoBtn->setToolTip(QStringLiteral("\u64A4\u9500"));
    connect(undoBtn, &QPushButton::clicked, m_undoStack, &QUndoStack::undo);
    barLayout->addWidget(undoBtn);

    auto* redoBtn = new ElaPushButton(QStringLiteral("\u21AA"), m_floatingBar);
    redoBtn->setCursor(Qt::PointingHandCursor);
    redoBtn->setStyleSheet(toolBtnStyle);
    redoBtn->setToolTip(QStringLiteral("\u91CD\u505A"));
    connect(redoBtn, &QPushButton::clicked, m_undoStack, &QUndoStack::redo);
    barLayout->addWidget(redoBtn);

    addSep();

    // ── 取消 / 确认 ──
    auto* cancelBtn = new ElaPushButton(QStringLiteral("\u2715"), m_floatingBar);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setFixedSize(32, 28);
    cancelBtn->setStyleSheet(
        QStringLiteral("QPushButton{background:rgba(255,255,255,0.08);color:#FF6C68;"
                       "border:none;border-radius:6px;font-size:14px;font-weight:700;"
                       "min-width:32px;min-height:28px;}"
                       "QPushButton:hover{background:rgba(255,108,104,0.2);}"));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    barLayout->addWidget(cancelBtn);

    auto* confirmBtn = new ElaPushButton(QStringLiteral("\u2713"), m_floatingBar);
    confirmBtn->setCursor(Qt::PointingHandCursor);
    confirmBtn->setFixedSize(32, 28);
    confirmBtn->setStyleSheet(
        QStringLiteral("QPushButton{background:#2F6FED;color:white;border:none;"
                       "border-radius:6px;font-size:14px;font-weight:700;"
                       "min-width:32px;min-height:28px;}"
                       "QPushButton:hover{background:#417CF0;}"));
    connect(confirmBtn, &QPushButton::clicked, this, &QDialog::accept);
    barLayout->addWidget(confirmBtn);

    m_floatingBar->adjustSize();

    // 初始选中状态
    onColorPick(m_currentColor);
    onWidthPick(m_currentWidth);
}

// ─── 重写 resizeEvent 使浮动栏跟随居中 ──────────────────────
void ScreenshotEditorWindow::resizeEvent(QResizeEvent* event)
{
    ElaDialog::resizeEvent(event);
    if (m_floatingBar) {
        const int bw = m_floatingBar->sizeHint().width();
        const int bx = (width() - bw) / 2;
        const int by = height() - m_floatingBar->height() - 16;
        m_floatingBar->setGeometry(bx, by, bw, m_floatingBar->height());
        m_floatingBar->raise();
    }
}

QImage ScreenshotEditorWindow::editedImage() const
{
    QImage result(m_sourceImage.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    m_scene->render(&painter);
    painter.end();
    return result;
}

void ScreenshotEditorWindow::onToolSelected(AnnotationTool tool)
{
    m_activeTool = tool;
    m_view->setActiveTool(tool);
    m_view->setPenColor(m_currentColor);
    m_view->setPenWidth(m_currentWidth);
    updateToolButtonStates();
}

void ScreenshotEditorWindow::onColorPick(const QColor& color)
{
    m_currentColor = color;
    m_view->setPenColor(color);
    updateColorButtonStates();
}

void ScreenshotEditorWindow::onWidthPick(int width)
{
    m_currentWidth = width;
    m_view->setPenWidth(width);
    updateWidthButtonStates();
}

void ScreenshotEditorWindow::onTextAnnotation(const QPointF& scenePos)
{
    bool ok = false;
    const QString text = QInputDialog::getText(this,
                                                QStringLiteral("\u6DFB\u52A0\u6587\u5B57\u6807\u6CE8"),
                                                QStringLiteral("\u8BF7\u8F93\u5165\u6807\u6CE8\u6587\u5B57:"),
                                                QLineEdit::Normal,
                                                QString(),
                                                &ok);
    if (!ok || text.trimmed().isEmpty()) {
        return;
    }

    auto* textItem = new QGraphicsTextItem(text);
    textItem->setDefaultTextColor(m_currentColor);
    QFont textFont;
    textFont.setPointSize(qMax(12, m_currentWidth * 4));
    textItem->setFont(textFont);
    textItem->setPos(scenePos);

    if (m_undoStack) {
        m_undoStack->push(new AddAnnotationCommand(m_scene, textItem));
    } else {
        m_scene->addItem(textItem);
    }
}

void ScreenshotEditorWindow::updateToolButtonStates()
{
    for (auto* btn : m_toolButtons) {
        const auto tool = static_cast<AnnotationTool>(btn->property("annotationTool").toInt());
        btn->setChecked(tool == m_activeTool);
    }
}

void ScreenshotEditorWindow::updateColorButtonStates()
{
    for (auto* btn : m_colorButtons) {
        const QColor c = btn->property("colorValue").value<QColor>();
        const bool selected = (c == m_currentColor);
        btn->setStyleSheet(
            QStringLiteral("QPushButton{background:%1;border:2px solid %2;"
                           "border-radius:11px;min-width:22px;min-height:22px;}"
                           "QPushButton:hover{border-color:rgba(255,255,255,0.5);}")
                .arg(c.name(), selected ? QStringLiteral("#72A4FF") : QStringLiteral("transparent")));
    }
}

void ScreenshotEditorWindow::updateWidthButtonStates()
{
    for (auto* btn : m_widthButtons) {
        const int w = btn->property("widthValue").toInt();
        btn->setChecked(w == m_currentWidth);
    }
}
