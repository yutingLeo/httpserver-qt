#ifndef SESSION_H
#define SESSION_H

#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QUuid>
#include <QtCore/QDateTime>
#include <QtCore/QJsonObject>
#include <QtCore/QReadWriteLock>
#include <optional>

// 会话有效期（秒），用于滑动续期
constexpr qint64 SESSION_TTL_SECONDS = 3600;

// 会话实体：服务器内部存储的完整会话信息
// 一个 Session 对应一次登录（一个设备/一次登录调用）
// token 是 Session 的主键，userId 是外键
struct Session {
    QString token;          // 会话凭证（主键，用于 m_sessions 索引）
    qint64 userId = 0;      // 所属用户（外键，用于 m_userTokens 反查）
    QDateTime issuedAt;     // 签发时间
    QDateTime expiresAt;    // 过期时间（滑动续期时更新）
};

// Token 响应 DTO：下发给客户端的标准化字段（RFC 6749 §4.1.4）
// 从 Session 转换而来，只含客户端需要的字段，不含服务器内部状态
struct TokenInfo {
    QString accessToken;
    QString tokenType = QStringLiteral("Bearer");  // 告知客户端如何使用（RFC 6750）
    qint64 expiresIn = SESSION_TTL_SECONDS;        // 有效期（秒）

    // 从会话实体构造响应
    static TokenInfo fromSession(const Session &s) {
        TokenInfo info;
        info.accessToken = s.token;
        info.expiresIn = qint64(s.issuedAt.secsTo(s.expiresAt));
        return info;
    }

    // 序列化为 RFC 6749 标准 JSON，可直接合并到登录/注册响应
    QJsonObject toJson() const {
        return {
            {"access_token", accessToken},
            {"token_type", tokenType},
            {"expires_in", expiresIn}
        };
    }
};

// 有状态会话管理：本地内存缓存 + 滑动续期
// - 每个 Session 由 token 唯一索引（m_sessions）
// - 每个用户可有多个 Session（多设备登录），由 m_userTokens 反向索引
// - 每次合法访问自动延长 expiresAt（滑动窗口）
// - 过期 Session 在 validate 时惰性清理
class SessionManager {
public:
    // 创建会话，返回 TokenInfo（含标准字段，handler 可直接 toJson 合并到响应）
    TokenInfo startSession(qint64 userId) {
        QWriteLocker locker(&m_lock);
        Session s;
        s.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.userId = userId;
        s.issuedAt = QDateTime::currentDateTimeUtc();
        s.expiresAt = s.issuedAt.addSecs(SESSION_TTL_SECONDS);
        m_sessions.insert(s.token, s);
        m_userTokens[userId].insert(s.token);
        return TokenInfo::fromSession(s);
    }

    // 登出：删除指定会话（不影响该用户的其他会话/设备）
    void endSession(const QString &token) {
        QWriteLocker locker(&m_lock);
        auto it = m_sessions.find(token);
        if (it != m_sessions.end()) {
            m_userTokens[it->userId].remove(token);
            m_sessions.erase(it);
        }
    }

    // 验证 token 并滑动续期
    // - token 不存在或已过期：返回 nullopt（过期项会被惰性清理）
    // - token 有效：续期 TTL，返回 userId
    // 注意：非 const，因为滑动续期需修改 expiresAt
    std::optional<qint64> validateToken(const QString &token) {
        QWriteLocker locker(&m_lock);
        auto it = m_sessions.find(token);
        if (it == m_sessions.end())
            return std::nullopt;
        if (QDateTime::currentDateTimeUtc() >= it->expiresAt) {
            // 过期：惰性清理
            m_userTokens[it->userId].remove(token);
            m_sessions.erase(it);
            return std::nullopt;
        }
        // 滑动续期：每次合法访问延长 TTL
        it->expiresAt = QDateTime::currentDateTimeUtc().addSecs(SESSION_TTL_SECONDS);
        return it->userId;
    }

    // 清除用户所有会话（例如修改密码后强制下线所有设备）
    void clearUserSessions(qint64 userId) {
        QWriteLocker locker(&m_lock);
        auto it = m_userTokens.find(userId);
        if (it != m_userTokens.end()) {
            for (const QString &token : it.value())
                m_sessions.remove(token);
            m_userTokens.erase(it);
        }
    }

private:
    // 主存储：token -> Session（token 是主键）
    QHash<QString, Session> m_sessions;
    // 反向索引：userId -> tokens（用于按用户批量操作，如强制下线所有设备）
    QHash<qint64, QSet<QString>> m_userTokens;
    mutable QReadWriteLock m_lock;
};

#endif // SESSION_H
