#pragma once

#include <QFrame>
#include <QString>
#include <QTimer>

class ElaPushButton;
class ElaText;

class ProfileCardPopup : public QFrame {
    Q_OBJECT

public:
    struct ProfileInfo {
        QString clientId;
        QString displayName;
        QString host;
        quint16 port = 0;
        QString signature;
        bool isOnline = false;
        QString department;
        QString jobTitle;
        QString phoneNumber;
        QString gender;
        QString email;
    };

    explicit ProfileCardPopup(QWidget* parent = nullptr);

    void showProfile(const ProfileInfo& info, const QPoint& globalPos);

    /// 延迟隐藏（300ms），允许用户将鼠标移入弹窗
    void scheduleHide();
    /// 取消延迟隐藏（鼠标进入弹窗或新的 hover 触发时）
    void cancelHide();

signals:
    void sendMessageRequested(const QString& clientId);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyThemeStyle();

    ElaText* m_avatarLabel = nullptr;
    ElaText* m_nameLabel = nullptr;
    ElaText* m_statusLabel = nullptr;
    ElaText* m_signatureLabel = nullptr;
    ElaText* m_idLabel = nullptr;
    ElaText* m_hostLabel = nullptr;
    ElaText* m_portLabel = nullptr;
    ElaText* m_departmentLabel = nullptr;
    ElaText* m_jobTitleLabel = nullptr;
    ElaText* m_phoneLabel = nullptr;
    ElaText* m_genderLabel = nullptr;
    ElaText* m_emailLabel = nullptr;
    ElaPushButton* m_sendMessageBtn = nullptr;
    QString m_currentClientId;
    QTimer m_hideTimer;

    bool event(QEvent* e) override;
};
