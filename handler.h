#ifndef HANDLER_H
#define HANDLER_H

#include "storage.h"
#include "session.h"
#include "types.h"
#include "utils.h"

#include <QtHttpServer/QHttpServerResponse>
#include <QtHttpServer/QHttpServerRequest>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

class UserHandler {
public:
    explicit UserHandler(ThreadSafeStorage<User> *storage, SessionManager *sessions)
        : m_storage(storage), m_sessions(sessions) {}

    QHttpServerResponse list(int page, int perPage) {
        auto users = m_storage->getAll();
        return paginateResponse(users, page, perPage);
    }

    QHttpServerResponse get(qint64 id) {
        auto user = m_storage->get(id);
        if (!user) return QHttpServerResponse(QHttpServerResponder::StatusCode::NotFound);
        return QHttpServerResponse(user->toJson());
    }

    QHttpServerResponse create(const QJsonObject &json, const QString &token) {
        if (!m_sessions->validateToken(token).has_value())
            return QHttpServerResponse(QHttpServerResponder::StatusCode::Unauthorized);

        // 验证必要字段
        if (!json.contains("email") || !json.contains("first_name") ||
            !json.contains("last_name") || !json.contains("avatar")) {
            return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
        }

        User newUser;
        newUser.email = json["email"].toString();
        newUser.firstName = json["first_name"].toString();
        newUser.lastName = json["last_name"].toString();
        newUser.avatarUrl = QUrl(json["avatar"].toString());
        newUser.createdAt = QDateTime::currentDateTimeUtc();
        newUser.updatedAt = newUser.createdAt;
        newUser.passwordHash = hashPassword(json["password"].toString());

        m_storage->insertWithNewId(newUser);
        return QHttpServerResponse(newUser.toJson(), QHttpServerResponder::StatusCode::Created);
    }

    QHttpServerResponse update(qint64 id, const QJsonObject &json, const QString &token, bool partial) {
        if (!m_sessions->validateToken(token).has_value())
            return QHttpServerResponse(QHttpServerResponder::StatusCode::Unauthorized);

        auto existing = m_storage->get(id);
        if (!existing) return QHttpServerResponse(QHttpServerResponder::StatusCode::NotFound);

        if (!partial) {
            // 全量更新：必须所有字段存在
            if (!json.contains("email") || !json.contains("first_name") ||
                !json.contains("last_name") || !json.contains("avatar")) {
                return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
            }
            existing->email = json["email"].toString();
            existing->firstName = json["first_name"].toString();
            existing->lastName = json["last_name"].toString();
            existing->avatarUrl = QUrl(json["avatar"].toString());
        } else {
            // 部分更新
            if (json.contains("email")) existing->email = json["email"].toString();
            if (json.contains("first_name")) existing->firstName = json["first_name"].toString();
            if (json.contains("last_name")) existing->lastName = json["last_name"].toString();
            if (json.contains("avatar")) existing->avatarUrl = QUrl(json["avatar"].toString());
        }
        existing->updatedAt = QDateTime::currentDateTimeUtc();
        m_storage->update(id, *existing);
        return QHttpServerResponse(existing->toJson());
    }

    QHttpServerResponse remove(qint64 id, const QString &token) {
        if (!m_sessions->validateToken(token).has_value())
            return QHttpServerResponse(QHttpServerResponder::StatusCode::Unauthorized);
        if (!m_storage->remove(id))
            return QHttpServerResponse(QHttpServerResponder::StatusCode::NotFound);
        return QHttpServerResponse(QHttpServerResponder::StatusCode::Ok);
    }

private:
    ThreadSafeStorage<User> *m_storage;
    SessionManager *m_sessions;
};

class ColorHandler {
public:
    explicit ColorHandler(ThreadSafeStorage<Color> *storage, SessionManager *sessions)
        : m_storage(storage), m_sessions(sessions) {}

    QHttpServerResponse list(int page, int perPage) {
        auto colors = m_storage->getAll();
        return paginateResponse(colors, page, perPage);
    }

    QHttpServerResponse get(qint64 id) {
        auto color = m_storage->get(id);
        if (!color) return QHttpServerResponse(QHttpServerResponder::StatusCode::NotFound);
        return QHttpServerResponse(color->toJson());
    }

    QHttpServerResponse create(const QJsonObject &json, const QString &token) {
        if (!m_sessions->validateToken(token).has_value())
            return QHttpServerResponse(QHttpServerResponder::StatusCode::Unauthorized);

        if (!json.contains("name") || !json.contains("color") || !json.contains("pantone_value"))
            return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);

        Color newColor;
        newColor.name = json["name"].toString();
        newColor.color = QColor(json["color"].toString());
        newColor.pantone = json["pantone_value"].toString();
        newColor.createdAt = QDateTime::currentDateTimeUtc();
        newColor.updatedAt = newColor.createdAt;

        m_storage->insertWithNewId(newColor);
        return QHttpServerResponse(newColor.toJson(), QHttpServerResponder::StatusCode::Created);
    }

    QHttpServerResponse update(qint64 id, const QJsonObject &json, const QString &token, bool partial) {
        if (!m_sessions->validateToken(token).has_value())
            return QHttpServerResponse(QHttpServerResponder::StatusCode::Unauthorized);

        auto existing = m_storage->get(id);
        if (!existing) return QHttpServerResponse(QHttpServerResponder::StatusCode::NotFound);

        if (!partial) {
            if (!json.contains("name") || !json.contains("color") || !json.contains("pantone_value"))
                return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
            existing->name = json["name"].toString();
            existing->color = QColor(json["color"].toString());
            existing->pantone = json["pantone_value"].toString();
        } else {
            if (json.contains("name")) existing->name = json["name"].toString();
            if (json.contains("color")) existing->color = QColor(json["color"].toString());
            if (json.contains("pantone_value")) existing->pantone = json["pantone_value"].toString();
        }
        existing->updatedAt = QDateTime::currentDateTimeUtc();
        m_storage->update(id, *existing);
        return QHttpServerResponse(existing->toJson());
    }

    QHttpServerResponse remove(qint64 id, const QString &token) {
        if (!m_sessions->validateToken(token).has_value())
            return QHttpServerResponse(QHttpServerResponder::StatusCode::Unauthorized);
        if (!m_storage->remove(id))
            return QHttpServerResponse(QHttpServerResponder::StatusCode::NotFound);
        return QHttpServerResponse(QHttpServerResponder::StatusCode::Ok);
    }

private:
    ThreadSafeStorage<Color> *m_storage;
    SessionManager *m_sessions;
};

// 处理会话相关请求（注册、登录、登出）
class AuthHandler {
public:
    AuthHandler(ThreadSafeStorage<User> *userStorage, SessionManager *sessions)
        : m_userStorage(userStorage), m_sessions(sessions) {}

    // 注册新用户（同时创建会话并返回token）
    QHttpServerResponse registerUser(const QJsonObject &json) {
        if (!json.contains("email") || !json.contains("password") ||
            !json.contains("first_name") || !json.contains("last_name") ||
            !json.contains("avatar")) {
            return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
        }

        // 检查邮箱是否已存在
        auto users = m_userStorage->getAll();
        for (const auto &u : users) {
            if (u.email == json["email"].toString()) {
                return QHttpServerResponse(QHttpServerResponder::StatusCode::Conflict);
            }
        }

        User newUser;
        newUser.email = json["email"].toString();
        newUser.firstName = json["first_name"].toString();
        newUser.lastName = json["last_name"].toString();
        newUser.avatarUrl = QUrl(json["avatar"].toString());
        newUser.passwordHash = hashPassword(json["password"].toString());
        newUser.createdAt = QDateTime::currentDateTimeUtc();
        newUser.updatedAt = newUser.createdAt;

        m_userStorage->insertWithNewId(newUser);
        // 签发 token（含 access_token/token_type/expires_in，符合 RFC 6749）
        auto tokenInfo = m_sessions->startSession(newUser.id);
        QJsonObject resp = newUser.toJson();
        for (const auto &k : tokenInfo.toJson().keys())
            resp[k] = tokenInfo.toJson()[k];
        return QHttpServerResponse(resp, QHttpServerResponder::StatusCode::Created);
    }

    // 登录：验证凭证，返回用户信息+token
    QHttpServerResponse login(const QJsonObject &json) {
        if (!json.contains("email") || !json.contains("password"))
            return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);

        auto users = m_userStorage->getAll();
        for (const auto &user : users) {
            if (user.email == json["email"].toString() &&
                verifyPassword(json["password"].toString(), user.passwordHash)) {
                // 多设备登录：不清除旧会话，每次登录签发独立 token
                // 旧 token 在过期或显式 logout 时失效
                // clearUserSessions 保留给"修改密码后强制下线所有设备"等场景
                auto tokenInfo = m_sessions->startSession(user.id);
                QJsonObject resp = user.toJson();
                for (const auto &k : tokenInfo.toJson().keys())
                    resp[k] = tokenInfo.toJson()[k];
                return QHttpServerResponse(resp);
            }
        }
        return QHttpServerResponse(QHttpServerResponder::StatusCode::Unauthorized);
    }

    // 登出：使token失效
    QHttpServerResponse logout(const QString &token) {
        if (token.isEmpty())
            return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
        auto userId = m_sessions->validateToken(token);
        if (userId.has_value()) {
            m_sessions->endSession(token);
            return QHttpServerResponse(QHttpServerResponder::StatusCode::Ok);
        }
        return QHttpServerResponse(QHttpServerResponder::StatusCode::Unauthorized);
    }

private:
    ThreadSafeStorage<User> *m_userStorage;
    SessionManager *m_sessions;
};

// 辅助函数：从QHttpServerRequest提取token
static QString extractToken(const QHttpServerRequest &request) {
    auto headers = request.headers();
    auto tokenBytes = headers.value("token");
    if (!tokenBytes.isEmpty())
        return QString::fromUtf8(tokenBytes);
    // 也可以从Authorization头提取 Bearer token
    auto authBytes = headers.value("Authorization");
    if (!authBytes.isEmpty()) {
        QString auth = QString::fromUtf8(authBytes);
        if (auth.startsWith("Bearer ", Qt::CaseInsensitive))
            return auth.mid(7);
    }
    return {};
}

#endif // HANDLER_H
