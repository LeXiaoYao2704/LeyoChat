#pragma once

#include <QJsonObject>
#include <QtGlobal>

#include <functional>

class FileServiceAuth;
class MessageEventBus;
class MessageServiceDatabase;
class MessageServiceOperations;
class MessageSessionRegistry;
class QHttpServer;

namespace MessageServiceHttpRoutes {

bool isAcceptedMessageRequestBodySize(qsizetype bodySize);

void registerRoutes(QHttpServer& server,
                    FileServiceAuth* auth,
                    MessageServiceDatabase* messages,
                    MessageEventBus* events = nullptr,
                    MessageServiceOperations* operations = nullptr,
                    MessageSessionRegistry* sessions = nullptr,
                    std::function<QJsonObject()> healthProvider = {});

}
