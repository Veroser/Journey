#ifndef AUTHMODELS_H
#define AUTHMODELS_H

#include <QString>

struct CityData {
    int id_city = 0;
    QString prefix;
    QString translate_key;
    QString timezone_name;
    QString country_code;
    int market_status = 0;
    QString name;
};

struct AuthData {
    QString access_token;
    QString refresh_token;
    qint64 expires_in_access = 0;   // unix timestamp
    qint64 expires_in_refresh = 0;  // unix timestamp
    int user_type = 0;              // из JWT: apiUserTypeId
    QString user_role;              // "student", "teacher", etc.
    int user_id = 0;                // из JWT: userId
    CityData city_data;
    QString display_name;

    bool isValid() const {
        return !access_token.isEmpty();
    }

    bool isExpired() const {
        return QDateTime::currentSecsSinceEpoch() >= expires_in_access;
    }
};

#endif // AUTHMODELS_H