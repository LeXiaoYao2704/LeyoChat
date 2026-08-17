#pragma once

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QtGlobal>

struct TestModeContext {
    bool enabled = false;
    QString profile;
    QString dataRoot;
    quint16 listenPort = 0;
    QString clientId;
    QString displayName;

    static TestModeContext fromArguments(const QStringList& arguments);
    static TestModeContext current();

    void applyToApplication(QCoreApplication& application) const;

    QString settingsOrganizationName() const;
    QString settingsApplicationName() const;
    QString singleInstanceKey() const;
    QString lockFileName() const;
    QString windowTitleSuffix() const;

    QString appDataRoot() const;
    QString appLocalDataRoot() const;
    QString databasePath() const;
    QString avatarDirectoryPath() const;
    QString logsDirectoryPath() const;
    QString crashDirectoryPath() const;
    QString screenshotsDirectoryPath() const;
    QString runtimeDirectoryPath() const;
    QString incomingFilesDirectoryPath() const;
};
