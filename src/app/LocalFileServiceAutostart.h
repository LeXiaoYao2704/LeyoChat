#pragma once

#include <QString>

class GroupRepository;

void autoStartLocalFileServices(const GroupRepository& groupRepo,
                                const QString& localClientId);
