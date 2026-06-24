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

// CRTP 基类：通用 CRUD 逻辑
template <typename Derived, typename Entity>
class BaseHandler {
public:
    explicit BaseHandler(ThreadSafeStorage<Entity> *storage, SessionManager *sessions)
        : m_storage(storage), m_sessions(sessions) {}

    QHttpServerResponse list(int page, int perPage) {
        auto items = m_storage->getAll();
        return paginateResponse(items, page, perPage);
    }

    QHttpServerResponse get(qint64 id) {
        auto item = m_storage->get(id);
        if (!item) return QHttpServerResponse(QHttpServerResponder::StatusCode::NotFound);
        return QHttpServerResponse(item->toJson());
    }

    QHttpServerResponse create(const QJsonObject &json, const QString &token) {
        if (!m_sessions->validateToken(token).has_value())
            return QHttpServerResponse(QHttpServerResponder::StatusCode::Unauthorized);

        // 委托给子类验证字段
        if (!static_cast<Derived*>(this)->validateCreateFields(json))
            return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);

        // 委托给子类构建实体
        Entity newEntity = static_cast<Derived*>(this)->buildEntity(json);
        newEntity.createdAt = QDateTime::currentDateTimeUtc();
        newEntity.updatedAt = newEntity.createdAt;

        m_storage->insertWithNewId(newEntity);
        return QHttpServerResponse(newEntity.toJson(), QHttpServerResponder::StatusCode::Created);
    }

    QHttpServerResponse update(qint64 id, const QJsonObject &json, const QString &token, bool partial) {
        if (!m_sessions->validateToken(token).has_value())
            return QHttpServerResponse(QHttpServerResponder::StatusCode::Unauthorized);

        auto existing = m_storage->get(id);
        if (!existing) return QHttpServerResponse(QHttpServerResponder::StatusCode::NotFound);

        // 委托给子类更新实体
        if (!static_cast<Derived*>(this)->updateEntity(existing.get(), json, partial))
            return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);

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

protected:
    ThreadSafeStorage<Entity> *m_storage;
    SessionManager *m_sessions;
};

// UserHandler：实现 User 特定逻辑
class UserHandler : public BaseHandler<UserHandler, User> {
    using Base = BaseHandler<UserHandler, User>;
public:
    explicit UserHandler(ThreadSafeStorage<User> *storage, SessionManager *sessions)
        : Base(storage, sessions) {}

    // 验证创建时的必填字段
    bool validateCreateFields(const QJsonObject &json) {
        return json.contains("email") && json.contains("first_name") &&
               json.contains("last_name") && json.contains("avatar") &&
               json.contains("password");
    }

    // 从 JSON 构建 User 对象
    User buildEntity(const QJsonObject &json) {
        User user;
        user.email = json["email"].toString();
        user.firstName = json["first_name"].toString();
        user.lastName = json["last_name"].toString();
        user.avatarUrl = QUrl(json["avatar"].toString());
        user.passwordHash = hashPassword(json["password"].toString());
        return user;
    }

    // 更新 User 对象
    bool updateEntity(User *existing, const QJsonObject &json, bool partial) {
        if (!partial) {
            if (!json.contains("email") || !json.contains("first_name") ||
                !json.contains("last_name") || !json.contains("avatar")) {
                return false;
            }
            existing->email = json["email"].toString();
            existing->firstName = json["first_name"].toString();
            existing->lastName = json["last_name"].toString();
            existing->avatarUrl = QUrl(json["avatar"].toString());
        } else {
            if (json.contains("email")) existing->email = json["email"].toString();
            if (json.contains("first_name")) existing->firstName = json["first_name"].toString();
            if (json.contains("last_name")) existing->lastName = json["last_name"].toString();
            if (json.contains("avatar")) existing->avatarUrl = QUrl(json["avatar"].toString());
        }
        return true;
    }
};

// ColorHandler：实现 Color 特定逻辑
class ColorHandler : public BaseHandler<ColorHandler, Color> {
    using Base = BaseHandler<ColorHandler, Color>;
public:
    explicit ColorHandler(ThreadSafeStorage<Color> *storage, SessionManager *sessions)
        : Base(storage, sessions) {}

    // 验证创建时的必填字段
    bool validateCreateFields(const QJsonObject &json) {
        return json.contains("name") && json.contains("color") &&
               json.contains("pantone_value");
    }

    // 从 JSON 构建 Color 对象
    Color buildEntity(const QJsonObject &json) {
        Color color;
        color.name = json["name"].toString();
        color.color = QColor(json["color"].toString());
        color.pantone = json["pantone_value"].toString();
        return color;
    }

    // 更新 Color 对象
    bool updateEntity(Color *existing, const QJsonObject &json, bool partial) {
        if (!partial) {
            if (!json.contains("name") || !json.contains("color") ||
                !json.contains("pantone_value")) {
                return false;
            }
            existing->name = json["name"].toString();
            existing->color = QColor(json["color"].toString());
            existing->pantone = json["pantone_value"].toString();
        } else {
            if (json.contains("name")) existing->name = json["name"].toString();
            if (json.contains("color")) existing->color = QColor(json["color"].toString());
            if (json.contains("pantone_value")) existing->pantone = json["pantone_value"].toString();
        }
        return true;
    }
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
        QString token = m_sessions->startSession(newUser.id);
        QJsonObject resp = newUser.toJson();
        resp["token"] = token;
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
                // 清除旧会话（可选，这里先清除再创建）
                m_sessions->clearUserSessions(user.id);
                QString token = m_sessions->startSession(user.id);
                QJsonObject resp = user.toJson();
                resp["token"] = token;
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
