#pragma once
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QVector>

class QMovie;

struct StickerInfo {
    QString id;         // "laugh"
    QString packId;     // "default"
    QString filePath;   // 绝对路径
    QString emoji;      // "😂" 降级显示
    QString label;      // "大笑"
};

struct StickerPack {
    QString id;
    QString name;
    QString iconPath;
    bool readOnly = true;
    QVector<StickerInfo> stickers;
};

class StickerManager : public QObject
{
    Q_OBJECT
public:
    static StickerManager& instance();

    const QVector<StickerPack>& packs() const;

    QString stickerFilePath(const QString& packId, const QString& stickerId) const;

    QPixmap stickerThumbnail(const QString& packId, const QString& stickerId, int size = 80) const;

    QMovie* createMovie(const QString& packId, const QString& stickerId, QObject* parent = nullptr) const;

    // 导入
    QString importFromFolder(const QString& folderPath, const QString& packName);
    QString importToCustomPack(const QStringList& filePaths);

    // 收藏
    bool addToFavorites(const QString& stickerId, const QByteArray& gifData);

    // 缓存收到的贴纸
    QString cacheReceivedSticker(const QString& packId, const QString& stickerId,
                                 const QByteArray& gifData);

    bool removeUserPack(const QString& packId);
    bool removeSticker(const QString& packId, const QString& stickerId);

    // 获取所有非内置贴纸（合并 custom / favorites 等）
    QVector<StickerInfo> customStickers() const;

    QByteArray readStickerData(const QString& packId, const QString& stickerId) const;

signals:
    void packsChanged();

private:
    explicit StickerManager(QObject* parent = nullptr);
    void loadPacks();
    void loadPacksFromDir(const QString& dir, bool readOnly);
    void ensureUserDir();
    QString generatePackId(const QString& name) const;
    void writeFavoritesManifest(const QString& favDir);

    QVector<StickerPack> m_packs;
    QString m_builtinDir;
    QString m_userDir;
    QString m_cacheDir;
};
