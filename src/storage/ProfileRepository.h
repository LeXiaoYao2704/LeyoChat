#pragma once

#include <memory>

#include <QString>

#include "domain/Profile.h"

class ProfileRepository {
public:
    explicit ProfileRepository(QString connectionName);

    std::unique_ptr<Profile> loadProfile() const;
    bool saveProfile(const Profile& profile) const;

private:
    QString m_connectionName;
};
