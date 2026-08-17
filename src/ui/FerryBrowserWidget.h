#pragma once
#include <QWidget>

class QWebEngineView;
class QWebEngineProfile;
class ElaText;
class ElaPushButton;

class FerryBrowserWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FerryBrowserWidget(QWidget* parent = nullptr);

    void detachToWindow();
    void reattachToPanel();
    bool isDetached() const;

signals:
    void detachRequested();
    void reattachRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void onLoadFinished(bool ok);
    void onRenderProcessTerminated(int terminationStatus, int exitCode);

    QWebEngineView*    m_webView         = nullptr;
    QWebEngineProfile* m_profile         = nullptr;
    ElaText*            m_statusLabel     = nullptr;
    ElaPushButton*       m_detachBtn       = nullptr;
    ElaPushButton*       m_refreshBtn      = nullptr;
    QWidget*           m_detachedWindow  = nullptr;
    bool               m_serviceConfigured = false;
};
