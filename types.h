#ifndef TYPES_H
#define TYPES_H

#include <QtCore/QDateTime>
#include <QtCore/QJsonObject>
#include <QtCore/QUrl>
#include <QtGui/QColor>

struct User {
    qint64 id = 0;
    QString email;
    QString firstName;
    QString lastName;
    QUrl avatarUrl;
    QString passwordHash;  // 生产环境存哈希，这里简化
    QDateTime createdAt;
    QDateTime updatedAt;

    QJsonObject toJson() const {
        return {
            {"id", id},
            {"email", email},
            {"first_name", firstName},
            {"last_name", lastName},
            {"avatar", avatarUrl.toString()},
            {"createdAt", createdAt.toString(Qt::ISODateWithMs)},
            {"updatedAt", updatedAt.toString(Qt::ISODateWithMs)}
        };
    }

    void updateFromJson(const QJsonObject &json) {
        if (json.contains("email")) email = json["email"].toString();
        if (json.contains("first_name")) firstName = json["first_name"].toString();
        if (json.contains("last_name")) lastName = json["last_name"].toString();
        if (json.contains("avatar")) avatarUrl = QUrl(json["avatar"].toString());
        updatedAt = QDateTime::currentDateTimeUtc();
    }
};

struct Color {
    qint64 id = 0;
    QString name;
    QColor color;
    QString pantone;
    QDateTime createdAt;
    QDateTime updatedAt;

    QJsonObject toJson() const {
        return {
            {"id", id},
            {"name", name},
            {"color", color.name()},
            {"pantone_value", pantone},
            {"createdAt", createdAt.toString(Qt::ISODateWithMs)},
            {"updatedAt", updatedAt.toString(Qt::ISODateWithMs)}
        };
    }

    void updateFromJson(const QJsonObject &json) {
        if (json.contains("name")) name = json["name"].toString();
        if (json.contains("color")) color = QColor(json["color"].toString());
        if (json.contains("pantone_value")) pantone = json["pantone_value"].toString();
        updatedAt = QDateTime::currentDateTimeUtc();
    }
};

#endif // TYPES_H