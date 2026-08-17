#include "storage/ProfileRepository.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

ProfileRepository::ProfileRepository(QString connectionName)
    : m_connectionName(connectionName) {}

std::unique_ptr<Profile> ProfileRepository::loadProfile() const {
    std::wstring clientId;
    std::wstring displayName;
    std::wstring employeeCode;
    std::wstring signature;
    std::wstring department;
    std::wstring jobTitle;
    std::wstring phoneNumber;
    std::wstring gender;
    std::wstring email;
    quint16 listenPort = 0;
    bool found = false;

    {
        QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
        if (!query.exec(QStringLiteral(
                "SELECT client_id, display_name, employee_code, listen_port, signature, "
                "department, job_title, phone_number, gender, email "
                "FROM profile LIMIT 1"))) {
            return nullptr;
        }

        if (query.next()) {
            clientId = query.value(0).toString().toStdWString();
            displayName = query.value(1).toString().toStdWString();
            employeeCode = query.value(2).toString().toStdWString();
            listenPort = static_cast<quint16>(query.value(3).toUInt());
            signature = query.value(4).toString().toStdWString();
            department = query.value(5).toString().toStdWString();
            jobTitle = query.value(6).toString().toStdWString();
            phoneNumber = query.value(7).toString().toStdWString();
            gender = query.value(8).toString().toStdWString();
            email = query.value(9).toString().toStdWString();
            found = true;
        }

        query.finish();
    }

    if (!found) {
        return nullptr;
    }

    return std::make_unique<Profile>(Profile{
        clientId,
        displayName,
        employeeCode,
        signature,
        listenPort,
        department,
        jobTitle,
        phoneNumber,
        gender,
        email
    });
}

bool ProfileRepository::saveProfile(const Profile& profile) const {
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.isValid()) {
        return false;
    }

    if (!database.transaction()) {
        return false;
    }

    QSqlQuery clearQuery(database);
    if (!clearQuery.exec(QStringLiteral("DELETE FROM profile"))) {
        database.rollback();
        return false;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO profile (client_id, display_name, employee_code, listen_port, signature, "
        "department, job_title, phone_number, gender, email) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
    ));
    query.addBindValue(QString::fromStdWString(profile.clientId));
    query.addBindValue(QString::fromStdWString(profile.displayName));
    query.addBindValue(QString::fromStdWString(profile.employeeCode));
    query.addBindValue(profile.listenPort);
    query.addBindValue(QString::fromStdWString(profile.signature));
    query.addBindValue(QString::fromStdWString(profile.department));
    query.addBindValue(QString::fromStdWString(profile.jobTitle));
    query.addBindValue(QString::fromStdWString(profile.phoneNumber));
    query.addBindValue(QString::fromStdWString(profile.gender));
    query.addBindValue(QString::fromStdWString(profile.email));
    if (!query.exec()) {
        database.rollback();
        return false;
    }

    return database.commit();
}
