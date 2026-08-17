#pragma once

#include <string>
#include <QtGlobal>

struct Profile {
    std::wstring clientId;
    std::wstring displayName;
    std::wstring employeeCode;
    std::wstring signature;
    quint16 listenPort = 0;
    std::wstring department;
    std::wstring jobTitle;
    std::wstring phoneNumber;
    std::wstring gender;
    std::wstring email;
};
