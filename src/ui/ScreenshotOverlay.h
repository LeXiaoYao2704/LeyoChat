#pragma once

#include <QImage>
#include <QPainterPath>
#include <QWidget>

class ElaText;
class ElaPushButton;

/// 标注工具类型
enum class OverlayAnnotationTool {
    None,
    Rectangle,
    Ellipse,
    Arrow,
    Freehand,
    Text,
    Mosaic
};

/// 标注项数据
struct OverlayAnnotation {
    OverlayAnnotationTool tool = OverlayAnnotationTool::None;
    QColor color = Qt::red;
    int width = 3;
    QPointF start;
    QPointF end;
    QPainterPath path;       // Freehand
    QString text;            // Text
    QImage mosaicImage;      // Mosaic 预渲染
    QRectF mosaicRect;
};

/// 全屏截图覆盖层：抓取屏幕 → 半透明遮罩 → 鼠标框选 → 原地编辑标注 → 裁切返回
class ScreenshotOverlay : public QWidget {
    Q_OBJECT
public:
    explicit ScreenshotOverlay(QWidget* parent = nullptr);

    QImage croppedImage() const;
    bool wasAccepted() const;

signals:
    void accepted();
    void rejected();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void finish(bool accept);
    void updateSizeLabel();
    void buildToolbar();
    void repositionToolbar();
    void enterEditMode();
    void setActiveTool(OverlayAnnotationTool tool);
    void updateToolButtonStates();
    void drawAnnotation(QPainter& painter, const OverlayAnnotation& ann) const;
    QImage renderFinalImage() const;
    QRect selectedRect() const;

    // 选区阶段
    QImage m_fullScreen;
    QPoint m_origin;
    QPoint m_current;
    bool m_selecting = false;
    bool m_hasSelection = false;
    bool m_accepted = false;
    bool m_finishing = false;
    QImage m_cropped;
    ElaText* m_sizeLabel = nullptr;

    // 编辑阶段
    bool m_editMode = false;
    OverlayAnnotationTool m_activeTool = OverlayAnnotationTool::None;
    QColor m_penColor = Qt::red;
    int m_penWidth = 3;
    bool m_drawing = false;
    QPointF m_drawStart;
    OverlayAnnotation m_currentAnnotation;
    QList<OverlayAnnotation> m_annotations;

    // 工具栏
    QWidget* m_toolbar = nullptr;
    QList<ElaPushButton*> m_toolButtons;
    QList<ElaPushButton*> m_colorButtons;
};
