#include "StickerManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMovie>
#include <QPixmapCache>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>

StickerManager& StickerManager::instance()
{
    static StickerManager mgr;
    return mgr;
}

StickerManager::StickerManager(QObject* parent)
    : QObject(parent)
{
    m_builtinDir = QCoreApplication::applicationDirPath() + QStringLiteral("/stickers");
    m_userDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                + QStringLiteral("/stickers");
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                 + QStringLiteral("/sticker-cache");
    ensureUserDir();
    loadPacks();
}

void StickerManager::ensureUserDir()
{
    QDir().mkpath(m_userDir);
    QDir().mkpath(m_cacheDir);
}

void StickerManager::loadPacks()
{
    m_packs.clear();
    loadPacksFromDir(m_builtinDir, true);
    loadPacksFromDir(m_userDir, false);
    loadPacksFromDir(m_cacheDir, true);
}

void StickerManager::loadPacksFromDir(const QString& dir, bool readOnly)
{
    QDir baseDir(dir);
    if (!baseDir.exists()) return;

    const auto subDirs = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& dirName : subDirs) {
        const QString packDir = dir + QStringLiteral("/") + dirName;
        const QString manifestPath = packDir + QStringLiteral("/manifest.json");

        QFile file(manifestPath);
        if (!file.open(QIODevice::ReadOnly)) {
            // 无 manifest：扫描目录中的图片自动构建
            QDir d(packDir);
            const QStringList gifs = d.entryList(
                {QStringLiteral("*.gif"), QStringLiteral("*.png"),
                 QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
                 QStringLiteral("*.webp")},
                QDir::Files);
            if (gifs.isEmpty()) continue;

            StickerPack pack;
            pack.id = dirName;
            pack.name = dirName;
            pack.readOnly = readOnly;
            pack.iconPath = packDir + QStringLiteral("/") + gifs.first();
            for (const QString& gif : gifs) {
                StickerInfo info;
                info.id = QFileInfo(gif).baseName();
                info.packId = pack.id;
                info.filePath = packDir + QStringLiteral("/") + gif;
                info.label = info.id;
                pack.stickers.append(info);
            }
            m_packs.append(pack);
            continue;
        }

        const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        StickerPack pack;
        pack.id = root.value(QStringLiteral("id")).toString(dirName);
        pack.name = root.value(QStringLiteral("name")).toString(dirName);
        pack.readOnly = readOnly;
        pack.iconPath = packDir + QStringLiteral("/")
                        + root.value(QStringLiteral("icon")).toString();

        const QJsonArray arr = root.value(QStringLiteral("stickers")).toArray();
        for (const QJsonValue& val : arr) {
            const QJsonObject obj = val.toObject();
            StickerInfo info;
            info.id = obj.value(QStringLiteral("id")).toString();
            info.packId = pack.id;
            info.filePath = packDir + QStringLiteral("/")
                            + obj.value(QStringLiteral("file")).toString();
            info.emoji = obj.value(QStringLiteral("emoji")).toString();
            info.label = obj.value(QStringLiteral("label")).toString();
            if (QFile::exists(info.filePath))
                pack.stickers.append(info);
        }

        if (!pack.stickers.isEmpty())
            m_packs.append(pack);
    }
}

const QVector<StickerPack>& StickerManager::packs() const
{
    return m_packs;
}

QString StickerManager::stickerFilePath(const QString& packId, const QString& stickerId) const
{
    for (const auto& pack : m_packs) {
        if (pack.id != packId) continue;
        for (const auto& s : pack.stickers) {
            if (s.id == stickerId) return s.filePath;
        }
    }
    return {};
}

QPixmap StickerManager::stickerThumbnail(const QString& packId, const QString& stickerId,
                                          int size) const
{
    const QString path = stickerFilePath(packId, stickerId);
    if (path.isEmpty()) return {};

    const QString cacheKey = QStringLiteral("sticker-thumb|%1|%2|%3")
                                 .arg(packId, stickerId, QString::number(size));
    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached))
        return cached;

    QImageReader reader(path);
    reader.setAutoTransform(true);
    // 解码时降采样避免大图占用内存
    const QSize origSize = reader.size();
    if (origSize.isValid() && (origSize.width() > size * 2 || origSize.height() > size * 2)) {
        reader.setScaledSize(origSize.scaled(QSize(size * 2, size * 2), Qt::KeepAspectRatio));
    }
    const QImage img = reader.read();
    if (img.isNull()) return {};

    const QPixmap pm = QPixmap::fromImage(img).scaled(
        size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmapCache::insert(cacheKey, pm);
    return pm;
}

QMovie* StickerManager::createMovie(const QString& packId, const QString& stickerId,
                                     QObject* parent) const
{
    const QString path = stickerFilePath(packId, stickerId);
    if (path.isEmpty()) return nullptr;

    auto* movie = new QMovie(path, QByteArray(), parent);
    if (!movie->isValid()) {
        delete movie;
        return nullptr;
    }
    return movie;
}

QByteArray StickerManager::readStickerData(const QString& packId, const QString& stickerId) const
{
    const QString path = stickerFilePath(packId, stickerId);
    if (path.isEmpty()) return {};

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

// ── 导入 ──────────────────────────────────────────────────────

QString StickerManager::generatePackId(const QString& name) const
{
    // 用名称 + 短 UUID 避免冲突
    const QString safe = QString(name).replace(QRegularExpression(QStringLiteral("[^\\w]")),
                                               QStringLiteral("_"));
    return safe + QStringLiteral("_") + QUuid::createUuid().toString(QUuid::Id128).left(8);
}

QString StickerManager::importFromFolder(const QString& folderPath, const QString& packName)
{
    QDir srcDir(folderPath);
    const QStringList gifs = srcDir.entryList(
        {QStringLiteral("*.gif"), QStringLiteral("*.png"),
         QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
         QStringLiteral("*.webp")},
        QDir::Files);
    if (gifs.isEmpty()) return {};

    const QString packId = generatePackId(packName);
    const QString destDir = m_userDir + QStringLiteral("/") + packId;
    QDir().mkpath(destDir);

    QJsonArray stickersArr;
    for (const QString& gif : gifs) {
        const QString destFile = destDir + QStringLiteral("/") + gif;
        QFile::copy(srcDir.absoluteFilePath(gif), destFile);
        const QString id = QFileInfo(gif).baseName();
        stickersArr.append(QJsonObject{
            {QStringLiteral("id"), id},
            {QStringLiteral("file"), gif},
            {QStringLiteral("label"), id}
        });
    }

    const QJsonObject manifest{
        {QStringLiteral("id"), packId},
        {QStringLiteral("name"), packName},
        {QStringLiteral("icon"), gifs.first()},
        {QStringLiteral("stickers"), stickersArr}
    };
    QFile f(destDir + QStringLiteral("/manifest.json"));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(manifest).toJson(QJsonDocument::Compact));

    loadPacks();
    emit packsChanged();
    return packId;
}

QString StickerManager::importToCustomPack(const QStringList& filePaths)
{
    if (filePaths.isEmpty()) return {};

    // 所有用户导入的图片统一放入 "custom" 目录
    const QString packId = QStringLiteral("custom");
    const QString destDir = m_userDir + QStringLiteral("/") + packId;
    QDir().mkpath(destDir);

    // 读取已有 manifest（如果有）
    QJsonArray stickersArr;
    QSet<QString> existingIds;
    const QString manifestPath = destDir + QStringLiteral("/manifest.json");
    {
        QFile mf(manifestPath);
        if (mf.open(QIODevice::ReadOnly)) {
            const QJsonObject root = QJsonDocument::fromJson(mf.readAll()).object();
            stickersArr = root.value(QStringLiteral("stickers")).toArray();
            for (const auto& v : stickersArr)
                existingIds.insert(v.toObject().value(QStringLiteral("id")).toString());
        }
    }

    static const QStringList kStickerExts = {
        QStringLiteral("gif"), QStringLiteral("png"),
        QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("webp")
    };
    int added = 0;
    for (const QString& srcPath : filePaths) {
        const QFileInfo fi(srcPath);
        if (!kStickerExts.contains(fi.suffix(), Qt::CaseInsensitive))
            continue;
        // 用 baseName 作为 id，若重复则加序号
        QString baseId = fi.baseName();
        QString finalId = baseId;
        int seq = 1;
        while (existingIds.contains(finalId)) {
            finalId = baseId + QStringLiteral("_%1").arg(seq++);
        }
        const QString destFileName = finalId + QStringLiteral(".") + fi.suffix().toLower();
        QFile::copy(srcPath, destDir + QStringLiteral("/") + destFileName);
        existingIds.insert(finalId);
        stickersArr.append(QJsonObject{
            {QStringLiteral("id"), finalId},
            {QStringLiteral("file"), destFileName},
            {QStringLiteral("label"), fi.baseName()}
        });
        ++added;
    }
    if (added == 0) return {};

    const QString iconFile = stickersArr.first().toObject()
                                 .value(QStringLiteral("file")).toString();
    const QJsonObject manifest{
        {QStringLiteral("id"), packId},
        {QStringLiteral("name"), QStringLiteral("\u81EA\u5B9A\u4E49")},
        {QStringLiteral("icon"), iconFile},
        {QStringLiteral("stickers"), stickersArr}
    };
    // 必须在 loadPacks() 之前关闭文件，确保数据完全落盘
    {
        QFile f(manifestPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(QJsonDocument(manifest).toJson(QJsonDocument::Compact));
    } // f 析构 → close() → 数据落盘

    loadPacks();
    emit packsChanged();
    return packId;
}

// ── 收藏 ──────────────────────────────────────────────────────

static QString detectImageExtension(const QByteArray& data)
{
    if (data.size() >= 4 && data[0] == '\x89' && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')
        return QStringLiteral(".png");
    if (data.size() >= 3 && data[0] == '\xFF' && data[1] == '\xD8' && data[2] == '\xFF')
        return QStringLiteral(".jpg");
    if (data.size() >= 4 && data.left(4) == "RIFF" && data.size() >= 12 && data.mid(8, 4) == "WEBP")
        return QStringLiteral(".webp");
    return QStringLiteral(".gif");
}

bool StickerManager::addToFavorites(const QString& stickerId, const QByteArray& gifData)
{
    if (stickerId.isEmpty() || gifData.isEmpty()) return false;

    const QString favDir = m_userDir + QStringLiteral("/favorites");
    QDir().mkpath(favDir);

    const QString ext = detectImageExtension(gifData);
    const QString fileName = stickerId + ext;
    const QString filePath = favDir + QStringLiteral("/") + fileName;

    // 已存在则跳过
    if (QFile::exists(filePath)) return true;

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(gifData);
    f.close();

    writeFavoritesManifest(favDir);

    loadPacks();
    emit packsChanged();
    return true;
}

void StickerManager::writeFavoritesManifest(const QString& favDir)
{
    QDir d(favDir);
    const QStringList gifs = d.entryList(
        {QStringLiteral("*.gif"), QStringLiteral("*.png"),
         QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
         QStringLiteral("*.webp")},
        QDir::Files);

    QJsonArray stickersArr;
    for (const QString& gif : gifs) {
        stickersArr.append(QJsonObject{
            {QStringLiteral("id"), QFileInfo(gif).baseName()},
            {QStringLiteral("file"), gif},
            {QStringLiteral("label"), QFileInfo(gif).baseName()}
        });
    }

    const QJsonObject manifest{
        {QStringLiteral("id"), QStringLiteral("favorites")},
        {QStringLiteral("name"), QStringLiteral("收藏")},
        {QStringLiteral("icon"), gifs.isEmpty() ? QString() : gifs.first()},
        {QStringLiteral("stickers"), stickersArr}
    };

    QFile f(favDir + QStringLiteral("/manifest.json"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(manifest).toJson(QJsonDocument::Compact));
}

// ── 缓存接收的贴纸 ──────────────────────────────────────────────

QString StickerManager::cacheReceivedSticker(const QString& packId, const QString& stickerId,
                                              const QByteArray& gifData)
{
    if (stickerId.isEmpty() || gifData.isEmpty()) return {};

    // 如果本地已有该贴纸，直接返回
    const QString existing = stickerFilePath(packId, stickerId);
    if (!existing.isEmpty()) return existing;

    const QString cachePackDir = m_cacheDir + QStringLiteral("/") + packId;
    QDir().mkpath(cachePackDir);

    const QString ext = detectImageExtension(gifData);
    const QString fileName = stickerId + ext;
    const QString filePath = cachePackDir + QStringLiteral("/") + fileName;

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(gifData);
    f.close();

    // 重新加载以识别新缓存的贴纸
    loadPacks();
    return filePath;
}

// ── 删除 ──────────────────────────────────────────────────────

bool StickerManager::removeUserPack(const QString& packId)
{
    const QString packDir = m_userDir + QStringLiteral("/") + packId;
    if (!QDir(packDir).exists()) return false;

    QDir(packDir).removeRecursively();
    loadPacks();
    emit packsChanged();
    return true;
}

bool StickerManager::removeSticker(const QString& packId, const QString& stickerId)
{
    // 只允许删除非内置贴纸
    for (auto& pack : m_packs) {
        if (pack.id != packId) continue;
        if (pack.readOnly) return false;
        break;
    }

    const QString filePath = stickerFilePath(packId, stickerId);
    if (filePath.isEmpty()) return false;

    QFile::remove(filePath);

    // 更新 manifest
    const QString packDir = m_userDir + QStringLiteral("/") + packId;
    const QString manifestPath = packDir + QStringLiteral("/manifest.json");
    QFile mf(manifestPath);
    if (mf.open(QIODevice::ReadOnly)) {
        QJsonObject root = QJsonDocument::fromJson(mf.readAll()).object();
        mf.close();
        QJsonArray arr = root.value(QStringLiteral("stickers")).toArray();
        QJsonArray newArr;
        for (const auto& v : arr) {
            if (v.toObject().value(QStringLiteral("id")).toString() != stickerId)
                newArr.append(v);
        }
        root[QStringLiteral("stickers")] = newArr;
        QFile wf(manifestPath);
        if (wf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            wf.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }

    loadPacks();
    emit packsChanged();
    return true;
}

QVector<StickerInfo> StickerManager::customStickers() const
{
    QVector<StickerInfo> result;
    for (const auto& pack : m_packs) {
        // 跳过内置的 default 包
        if (pack.id == QStringLiteral("default"))
            continue;
        // 跳过缓存包（readOnly 且非 default）
        if (pack.readOnly)
            continue;
        result.append(pack.stickers);
    }
    return result;
}
