#ifndef UTILS_H
#define UTILS_H

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QFile>
#include <QtCore/QCryptographicHash>
#include <QtCore/QRandomGenerator>
#include <QtHttpServer/QHttpServerResponse>
#include <optional>
#include <QJsonDocument>
#include <QJsonObject>

#include "storage.h"

// 密码哈希：返回 "salt$hash" 格式（SHA-256 + 随机盐）
inline QString hashPassword(const QString &password) {
    QByteArray salt(16, '\0');
    for (int i = 0; i < salt.size(); ++i)
        salt[i] = char(QRandomGenerator::global()->bounded(256));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(salt);
    hash.addData(password.toUtf8());
    return QString::fromLatin1(salt.toHex()) + "$" +
           QString::fromLatin1(hash.result().toHex());
}

// 校验密码：stored 为 "salt$hash" 格式
inline bool verifyPassword(const QString &password, const QString &stored) {
    QStringList parts = stored.split('$');
    if (parts.size() != 2) return false;
    QByteArray salt = QByteArray::fromHex(parts[0].toLatin1());
    QByteArray expected = QByteArray::fromHex(parts[1].toLatin1());
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(salt);
    hash.addData(password.toUtf8());
    return hash.result().toHex() == expected;
}

inline std::optional<QJsonObject> parseJsonObject(const QByteArray &body) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;
    return doc.object();
}

inline std::optional<QJsonArray> parseJsonArray(const QByteArray &body) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return std::nullopt;
    return doc.array();
}

// 通用分页响应
template<typename Container>
QHttpServerResponse paginateResponse(const Container &items, int page, int perPage) {
    if (page < 1) page = 1;
    if (perPage < 1) perPage = 10;
    int total = items.size();
    int start = (page - 1) * perPage;
    if (start >= total) {
        // 空页返回空数据
        QJsonObject resp;
        resp["page"] = page;
        resp["per_page"] = perPage;
        resp["total"] = total;
        resp["total_pages"] = (total + perPage - 1) / perPage;
        resp["data"] = QJsonArray();
        return QHttpServerResponse(resp);
    }
    int end = qMin(start + perPage, total);
    QJsonArray data;
    for (int i = start; i < end; ++i) {
        data.append(items[i].toJson());
    }
    QJsonObject resp;
    resp["page"] = page;
    resp["per_page"] = perPage;
    resp["total"] = total;
    resp["total_pages"] = (total + perPage - 1) / perPage;
    resp["data"] = data;
    return QHttpServerResponse(resp);
}

// 从资源文件加载JSON数组并填充存储
template<typename T>
void loadFromResource(const QString &resourcePath, ThreadSafeStorage<T> &storage,
                      std::function<T(const QJsonObject&)> fromJson) {
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open" << resourcePath;
        return;
    }
    QByteArray data = file.readAll();
    auto array = parseJsonArray(data);
    if (!array) {
        qWarning() << "Invalid JSON array in" << resourcePath;
        return;
    }
    storage.loadFromJsonArray(*array, fromJson);
}

#endif // UTILS_H