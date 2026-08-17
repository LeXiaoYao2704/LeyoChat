#pragma once
#include <QString>
#include <QVector>

struct RemoteFileInfo {
    QString fileId;
    QString workspaceId;
    QString fileName;
    QString currentVersion;
    QString uploadedById;
    QString uploadedByName;
    qint64  createdAtMs = 0;
    qint64  updatedAtMs = 0;
};

struct RemoteFileVersion {
    QString versionId;
    QString fileId;
    int     versionNumber = 0;
    QString versionLabel;
    QString uploaderId;
    QString uploaderName;
    qint64  uploadedAtMs = 0;
    qint64  fileSizeBytes = 0;
    QString changeNote;
};

struct RemoteFileUploadResult {
    QString fileId;
    QString versionId;
};
