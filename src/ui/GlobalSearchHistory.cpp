#include "GlobalSearchHistory.h"

#include <QSettings>
#include <algorithm>

GlobalSearchHistory::GlobalSearchHistory(const QString& orgName, const QString& appName)
    : m_orgName(orgName), m_appName(appName) {}

QStringList GlobalSearchHistory::recentKeywords() const
{
    QSettings s(m_orgName, m_appName);
    return s.value(QStringLiteral("globalSearch/keywords")).toStringList();
}

void GlobalSearchHistory::addKeyword(const QString& keyword)
{
    if (keyword.trimmed().isEmpty()) return;
    QSettings s(m_orgName, m_appName);
    QStringList list = s.value(QStringLiteral("globalSearch/keywords")).toStringList();
    list.removeAll(keyword);
    list.prepend(keyword);
    if (list.size() > kMaxKeywords) list = list.mid(0, kMaxKeywords);
    s.setValue(QStringLiteral("globalSearch/keywords"), list);
}

void GlobalSearchHistory::removeKeyword(const QString& keyword)
{
    QSettings s(m_orgName, m_appName);
    QStringList list = s.value(QStringLiteral("globalSearch/keywords")).toStringList();
    list.removeAll(keyword);
    s.setValue(QStringLiteral("globalSearch/keywords"), list);
}

void GlobalSearchHistory::clearKeywords()
{
    QSettings s(m_orgName, m_appName);
    s.remove(QStringLiteral("globalSearch/keywords"));
}

QVector<FrequentContact> GlobalSearchHistory::frequentContacts() const
{
    QSettings s(m_orgName, m_appName);
    const int count = s.beginReadArray(QStringLiteral("globalSearch/frequent"));
    QVector<FrequentContact> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        FrequentContact c;
        c.id = s.value(QStringLiteral("id")).toString();
        c.title = s.value(QStringLiteral("title")).toString();
        c.kind = s.value(QStringLiteral("kind")).toInt();
        c.hitCount = s.value(QStringLiteral("hits")).toInt();
        result.append(c);
    }
    s.endArray();
    return result;
}

void GlobalSearchHistory::recordContactHit(const QString& id, const QString& title, int kind)
{
    auto list = frequentContacts();
    auto it = std::find_if(list.begin(), list.end(),
        [&](const FrequentContact& c) { return c.id == id; });
    if (it != list.end()) {
        it->hitCount++;
        it->title = title;
    } else {
        list.append({id, title, kind, 1});
    }
    std::sort(list.begin(), list.end(),
        [](const FrequentContact& a, const FrequentContact& b) {
            return a.hitCount > b.hitCount;
        });
    if (list.size() > kMaxFrequent) list.resize(kMaxFrequent);

    QSettings s(m_orgName, m_appName);
    s.beginWriteArray(QStringLiteral("globalSearch/frequent"), list.size());
    for (int i = 0; i < list.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("id"), list[i].id);
        s.setValue(QStringLiteral("title"), list[i].title);
        s.setValue(QStringLiteral("kind"), list[i].kind);
        s.setValue(QStringLiteral("hits"), list[i].hitCount);
    }
    s.endArray();
}

void GlobalSearchHistory::removeFrequentContact(const QString& id)
{
    auto list = frequentContacts();
    list.erase(std::remove_if(list.begin(), list.end(),
        [&](const FrequentContact& c) { return c.id == id; }), list.end());
    QSettings s(m_orgName, m_appName);
    s.beginWriteArray(QStringLiteral("globalSearch/frequent"), list.size());
    for (int i = 0; i < list.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("id"), list[i].id);
        s.setValue(QStringLiteral("title"), list[i].title);
        s.setValue(QStringLiteral("kind"), list[i].kind);
        s.setValue(QStringLiteral("hits"), list[i].hitCount);
    }
    s.endArray();
}

void GlobalSearchHistory::clearFrequentContacts()
{
    QSettings s(m_orgName, m_appName);
    s.remove(QStringLiteral("globalSearch/frequent"));
}
