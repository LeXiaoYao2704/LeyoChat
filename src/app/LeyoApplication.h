#pragma once

#include <memory>
#include <optional>

#include "architecture/RuntimeArchitectureSnapshot.h"
#include "recovery/ClientStartupOptions.h"
#include "update/UpdateChecker.h"

class QApplication;
class QSplashScreen;
class MainWindow;
class UpdateDownloader;

class LeyoApplication {
public:
    explicit LeyoApplication(QApplication& app, ClientStartupOptions startupOptions = {});
    ~LeyoApplication();

    void setSplashScreen(QSplashScreen* splash);
    int run();

private:
    QApplication& m_app;
    ClientStartupOptions m_startupOptions;
    QSplashScreen* m_splashScreen = nullptr;
    std::unique_ptr<MainWindow> m_mainWindow;
    std::optional<RuntimeArchitectureSnapshot> m_runtimeArchitectureSnapshot;
    UpdateChecker* m_updateChecker = nullptr;
    UpdateDownloader* m_updateDownloader = nullptr;
    UpdateChecker::UpdateInfo m_pendingUpdateInfo;
};
