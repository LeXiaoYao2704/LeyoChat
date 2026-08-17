// MessageThumbnailCache.cpp — 图片缩略图异步缓存实现
#include "ui/MessageThumbnailCache.h"

#include <QImageReader>
#include <QMetaObject>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QRunnable>

namespace {

constexpr int kRoundedRadius = 12;

QImage decodeAndScale(const QString& localPath, const QSize& targetSize)
{
    if (targetSize.width() <= 0 || targetSize.height() <= 0) return {};

    QImageReader reader(localPath);
    reader.setAutoTransform(true);
    const QSize orig = reader.size();
    // 解码时降采样：如果原始尺寸 > 目标的 2 倍，先缩小再读取
    if (orig.isValid() && (orig.width() > targetSize.width() * 2
                           || orig.height() > targetSize.height() * 2))
        reader.setScaledSize(orig.scaled(targetSize * 2, Qt::KeepAspectRatio));

    QImage img = reader.read();
    if (img.isNull()) return {};

    return img.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QImage applyRoundedCorners(const QImage& src)
{
    if (src.isNull()) return src;
    QImage result(src.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(result.rect()), kRoundedRadius, kRoundedRadius);
    p.setClipPath(path);
    p.drawImage(0, 0, src);
    return result;
}

// ── 后台解码任务 ──
class ThumbnailTask : public QRunnable
{
public:
    ThumbnailTask(const QString& localPath, const QSize& targetSize,
                  const QString& cacheKey, MessageThumbnailCache* cache)
        : m_localPath(localPath), m_targetSize(targetSize),
          m_cacheKey(cacheKey), m_cache(cache)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        QImage img = decodeAndScale(m_localPath, m_targetSize);
        if (img.isNull()) return;
        img = applyRoundedCorners(img);
        if (!m_cache) return; // cache 已析构
        QMetaObject::invokeMethod(m_cache, "onTaskFinished",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, m_cacheKey),
                                  Q_ARG(QImage, img));
    }

private:
    QString m_localPath;
    QSize m_targetSize;
    QString m_cacheKey;
    QPointer<MessageThumbnailCache> m_cache;
};

} // namespace

// ── 单例 ──
MessageThumbnailCache& MessageThumbnailCache::instance()
{
    static MessageThumbnailCache s;
    return s;
}

MessageThumbnailCache::MessageThumbnailCache(QObject* parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(2);
}

MessageThumbnailCache::~MessageThumbnailCache()
{
    m_pool.clear();
    m_pool.waitForDone();
    clearCache();
}

QString MessageThumbnailCache::cacheKey(const QString& localPath, const QSize& sz)
{
    return QStringLiteral("%1|%2x%3").arg(localPath).arg(sz.width()).arg(sz.height());
}

QPixmap MessageThumbnailCache::generateBlurPlaceholder(const QString& localPath,
                                                        const QSize& targetSize)
{
    Q_UNUSED(localPath);

    // 主线程只返回轻量占位图，避免会话切换时对每张图片同步读盘。
    const QString key = QStringLiteral("%1x%2")
                            .arg(qMax(1, targetSize.width()))
                            .arg(qMax(1, targetSize.height()));
    static QHash<QString, QPixmap> s_placeholderCache;
    const auto it = s_placeholderCache.constFind(key);
    if (it != s_placeholderCache.constEnd()) {
        return it.value();
    }

    QPixmap pm(targetSize);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient g(0, 0, qMax(1, targetSize.width()), qMax(1, targetSize.height()));
    g.setColorAt(0.0, QColor(230, 236, 244, 220));
    g.setColorAt(1.0, QColor(210, 218, 230, 210));

    QPainterPath path;
    path.addRoundedRect(QRectF(pm.rect()), kRoundedRadius, kRoundedRadius);
    painter.fillPath(path, g);
    painter.end();

    s_placeholderCache.insert(key, pm);
    return pm;
}

QPixmap MessageThumbnailCache::requestThumbnail(const QString& localPath,
                                                 const QSize& targetSize,
                                                 const QString& requesterId)
{
    const QString key = cacheKey(localPath, targetSize);

    // 1. 缓存命中
    if (auto* node = m_cache.value(key, nullptr)) {
        touchNode(node);
        m_pendingByRequester.remove(requesterId);
        return node->pixmap;
    }

    // 2. 记录 requester
    m_pendingByRequester[requesterId] = key;

    // 3. 避免重复提交同一 key
    if (!m_pendingKeys.contains(key)) {
        m_pendingKeys.insert(key);
        auto* task = new ThumbnailTask(localPath, targetSize, key, this);
        m_pool.start(task);
    }

    // 4. 返回模糊占位
    return generateBlurPlaceholder(localPath, targetSize);
}

void MessageThumbnailCache::cancelRequest(const QString& requesterId)
{
    m_pendingByRequester.remove(requesterId);
}

void MessageThumbnailCache::clearCache()
{
    while (m_lruHead) {
        auto* next = m_lruHead->next;
        delete m_lruHead;
        m_lruHead = next;
    }
    m_lruTail = nullptr;
    m_cache.clear();
    m_totalBytes = 0;
    m_pendingByRequester.clear();
    m_pendingKeys.clear();
}

void MessageThumbnailCache::onTaskFinished(const QString& cacheKey, const QImage& image)
{
    m_pendingKeys.remove(cacheKey);
    if (image.isNull()) return;

    QPixmap pm = QPixmap::fromImage(image);
    insertToCache(cacheKey, pm);

    bool hasActiveRequester = false;
    for (auto it = m_pendingByRequester.begin(); it != m_pendingByRequester.end(); ) {
        if (it.value() == cacheKey) {
            hasActiveRequester = true;
            it = m_pendingByRequester.erase(it);
        } else {
            ++it;
        }
    }

    if (hasActiveRequester) {
        emit thumbnailReady(cacheKey, pm);
    }
}

// ── LRU 操作 ──
void MessageThumbnailCache::insertToCache(const QString& key, const QPixmap& pm)
{
    if (auto* old = m_cache.value(key, nullptr)) {
        evictNode(old);
    }

    const int byteSize = pm.width() * pm.height() * 4;
    evictUntilFits(byteSize);

    auto* node = new CacheNode{key, pm, byteSize, nullptr, m_lruHead};
    if (m_lruHead) m_lruHead->prev = node;
    m_lruHead = node;
    if (!m_lruTail) m_lruTail = node;
    m_cache.insert(key, node);
    m_totalBytes += byteSize;
}

void MessageThumbnailCache::touchNode(CacheNode* node)
{
    if (node == m_lruHead) return;

    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    if (node == m_lruTail) m_lruTail = node->prev;

    node->prev = nullptr;
    node->next = m_lruHead;
    if (m_lruHead) m_lruHead->prev = node;
    m_lruHead = node;
}

void MessageThumbnailCache::evictNode(CacheNode* node)
{
    if (node->prev) node->prev->next = node->next;
    else m_lruHead = node->next;
    if (node->next) node->next->prev = node->prev;
    else m_lruTail = node->prev;
    m_cache.remove(node->key);
    m_totalBytes -= node->byteSize;
    delete node;
}

void MessageThumbnailCache::evictUntilFits(int newBytes)
{
    while (m_lruTail && m_totalBytes + newBytes > kMaxCacheBytes) {
        evictNode(m_lruTail);
    }
}
