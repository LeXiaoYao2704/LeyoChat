#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct FrequentContact {
    QString id;       // clientId 或 groupId
    QString title;    // 显示名
    int kind;         // 0=联系人, 1=群组
    int hitCount = 0; // 使用频次
};

class GlobalSearchHistory {
public:
    explicit GlobalSearchHistory(const QString& orgName, const QString& appName);

    // 搜索历史
    QStringList recentKeywords() const;
    void addKeyword(const QString& keyword);
    void removeKeyword(const QString& keyword);
    void clearKeywords();

    // 常用联系人/群
    QVector<FrequentContact> frequentContacts() const;
    void recordContactHit(const QString& id, const QString& title, int kind);
    void removeFrequentContact(const QString& id);
    void clearFrequentContacts();

private:
    QString m_orgName;
    QString m_appName;
    static constexpr int kMaxKeywords = 20;
    static constexpr int kMaxFrequent = 15;
};
