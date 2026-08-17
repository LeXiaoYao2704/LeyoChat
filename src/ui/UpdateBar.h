#pragma once
#include <QWidget>

class QLabel;
class ElaPushButton;
class QProgressBar;

class UpdateBar : public QWidget {
    Q_OBJECT
public:
    explicit UpdateBar(QWidget* parent);

    void showUpdate(const QString& version);
    void showProgress(int percent);
    void showReady();
    void hideBar();

signals:
    void updateClicked();

private:
    ElaPushButton* m_button = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QString m_pendingVersion;
};
