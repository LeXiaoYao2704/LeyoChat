#pragma once

#include <QWidget>
#include <QPixmap>
#include <QTimer>

class ElaText;
class ElaScrollArea;
class ElaToolButton;

class ImageViewerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ImageViewerWidget(const QString& filePath, const QString& title, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent* event) override;
#else
    void enterEvent(QEvent* event) override;
#endif
    void leaveEvent(QEvent* event) override;

private:
    void fitToWindow();
    void updateDisplay();
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void updateZoomLabel();
    void layoutOverlays();
    void showOverlays();
    void hideOverlays();

    ElaText* m_imageLabel = nullptr;
    ElaText* m_zoomLabel = nullptr;
    QPixmap m_originalPixmap;
    double m_zoomFactor = 0.0;
    QPoint m_dragStart;
    QPoint m_scrollStart;
    bool m_dragging = false;
    bool m_firstShow = true;
    class ElaScrollArea* m_scrollArea = nullptr;
    QWidget* m_topOverlay = nullptr;    // 右上角关闭/最小化
    QWidget* m_bottomBar = nullptr;     // 底部悬浮工具栏
    QTimer m_hideTimer;
};
