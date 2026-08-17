#pragma once

#include <ElaDialog.h>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QUndoStack>

class QToolBar;
class QAction;
class QActionGroup;
class QColorDialog;
class QSpinBox;
class ElaPushButton;

// ─── Annotation tool type ────────────────────────────────────
enum class AnnotationTool {
    None,
    Rectangle,
    Ellipse,
    Arrow,
    Freehand,
    Text,
    Mosaic
};

// ─── Custom graphics items ──────────────────────────────────
class RectAnnotationItem : public QGraphicsRectItem {
public:
    explicit RectAnnotationItem(const QRectF& rect, const QPen& pen, QGraphicsItem* parent = nullptr);
};

class EllipseAnnotationItem : public QGraphicsEllipseItem {
public:
    explicit EllipseAnnotationItem(const QRectF& rect, const QPen& pen, QGraphicsItem* parent = nullptr);
};

class ArrowAnnotationItem : public QGraphicsLineItem {
public:
    explicit ArrowAnnotationItem(const QLineF& line, const QPen& pen, QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class FreehandAnnotationItem : public QGraphicsPathItem {
public:
    explicit FreehandAnnotationItem(const QPen& pen, QGraphicsItem* parent = nullptr);
    void addPoint(const QPointF& point);
private:
    QPainterPath m_path;
};

class MosaicAnnotationItem : public QGraphicsItem {
public:
    explicit MosaicAnnotationItem(const QImage& sourceImage, const QRectF& rect,
                                   int blockSize = 10, QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
private:
    QImage m_mosaicImage;
    QRectF m_rect;
};

// ─── Undo commands ───────────────────────────────────────────
class AddAnnotationCommand : public QUndoCommand {
public:
    AddAnnotationCommand(QGraphicsScene* scene, QGraphicsItem* item, QUndoCommand* parent = nullptr);
    ~AddAnnotationCommand() override;
    void undo() override;
    void redo() override;
private:
    QGraphicsScene* m_scene;
    QGraphicsItem* m_item;
    bool m_ownsItem = false;
};

// ─── Editor view (intercepts mouse for drawing) ─────────────
class ScreenshotEditorView : public QGraphicsView {
    Q_OBJECT
public:
    explicit ScreenshotEditorView(QGraphicsScene* scene, QWidget* parent = nullptr);

    void setActiveTool(AnnotationTool tool);
    void setPenColor(const QColor& color);
    void setPenWidth(int width);
    void setUndoStack(QUndoStack* stack);
    void setSourceImage(const QImage& image);

signals:
    void textAnnotationRequested(const QPointF& scenePos);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    AnnotationTool m_activeTool = AnnotationTool::None;
    QColor m_penColor = Qt::red;
    int m_penWidth = 3;
    QUndoStack* m_undoStack = nullptr;
    QImage m_sourceImage;

    bool m_drawing = false;
    QPointF m_startPoint;
    QGraphicsItem* m_currentItem = nullptr;
};

// ─── Main editor window ─────────────────────────────────────
class ScreenshotEditorWindow : public ElaDialog {
    Q_OBJECT
public:
    explicit ScreenshotEditorWindow(const QImage& image, QWidget* parent = nullptr);

    QImage editedImage() const;

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onToolSelected(AnnotationTool tool);
    void onColorPick(const QColor& color);
    void onWidthPick(int width);
    void onTextAnnotation(const QPointF& scenePos);

private:
    void buildFloatingToolbar();
    void updateToolButtonStates();
    void updateColorButtonStates();
    void updateWidthButtonStates();

    QGraphicsScene* m_scene = nullptr;
    ScreenshotEditorView* m_view = nullptr;
    QUndoStack* m_undoStack = nullptr;
    QImage m_sourceImage;

    QWidget* m_floatingBar = nullptr;
    QList<ElaPushButton*> m_toolButtons;
    QList<ElaPushButton*> m_colorButtons;
    QList<ElaPushButton*> m_widthButtons;

    AnnotationTool m_activeTool = AnnotationTool::None;
    QColor m_currentColor = Qt::red;
    int m_currentWidth = 3;
};
