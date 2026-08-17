#pragma once
#include <QWidget>

class QWebEngineView;
class QWebEngineProfile;
class QLabel;

class OnlineEditorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit OnlineEditorWidget(const QString& editorUrl,
                                 const QString& fileName,
                                 QWidget* parent = nullptr);

    /// Call once at app startup to pre-warm the Chromium subprocess.
    /// Subsequent editor opens will skip the cold-start delay (~2-5 s).
    static void warmUp();

signals:
    void editorClosed();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUi(const QString& editorUrl, const QString& fileName);
    void onLoadFinished(bool ok);
    void onRenderProcessTerminated(int terminationStatus, int exitCode);

    QWebEngineView* m_webView = nullptr;
    QLabel* m_statusLabel = nullptr;
};
