// MessageThumbnailCache.h — 图片缩略图异步缓存
#pragma once

#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QSize>
#include <QThreadPool>

class MessageThumbnailCache : public QObject
{
    Q_OBJECT
public:
    static MessageThumbnailCache& instance();

    // 主线程调用。缓存命中返回高清；未命中返回模糊占位并提交后台解码
    QPixmap requestThumbnail(const QString& localPath, const QSize& targetSize,
                             const QString& requesterId);

    // widget 回收时取消未完成任务
    void cancelRequest(const QString& requesterId);

    // 清空全部缓存
    void clearCache();

    // 测试用
    int totalCacheBytes() const { return m_totalBytes; }
    int cacheCount() const { return m_cache.size(); }

signals:
    void thumbnailReady(const QString& cacheKey, const QPixmap& pixmap);

private:
    explicit MessageThumbnailCache(QObject* parent = nullptr);
    ~MessageThumbnailCache() override;

    Q_INVOKABLE void onTaskFinished(const QString& cacheKey, const QImage& image);

    static QString cacheKey(const QString& localPath, const QSize& sz);
    QPixmap generateBlurPlaceholder(const QString& localPath, const QSize& targetSize);

    // LRU 双向链表节点
    struct CacheNode {
        QString key;
        QPixmap pixmap;
        int byteSize = 0;
        CacheNode* prev = nullptr;
        CacheNode* next = nullptr;
    };

    void insertToCache(const QString& key, const QPixmap& pm);
    void touchNode(CacheNode* node);
    void evictNode(CacheNode* node);
    void evictUntilFits(int newBytes);

    QHash<QString, CacheNode*> m_cache;
    CacheNode* m_lruHead = nullptr;
    CacheNode* m_lruTail = nullptr;
    int m_totalBytes = 0;
    static constexpr int kMaxCacheBytes = 30 * 1024 * 1024; // 30MB

    QHash<QString, QString> m_pendingByRequester; // requesterId → cacheKey
    QSet<QString> m_pendingKeys;                   // 正在解码的 key

    QThreadPool m_pool;
};
