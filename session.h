#ifndef SESSION_H
#define SESSION_H

#include <QtCore/QHash>
#include <QtCore/QUuid>
#include <QtCore/QReadWriteLock>
#include <optional>

class SessionManager {
public:
    QString startSession(qint64 userId) {
        QWriteLocker locker(&m_lock);
        QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_tokenToUserId[token] = userId;
        m_userIdToToken[userId] = token;
        return token;
    }

    void endSession(const QString &token) {
        QWriteLocker locker(&m_lock);
        if (auto it = m_tokenToUserId.find(token); it != m_tokenToUserId.end()) {
            m_userIdToToken.remove(it.value());
            m_tokenToUserId.erase(it);
        }
    }

    std::optional<qint64> validateToken(const QString &token) const {
        QReadLocker locker(&m_lock);
        auto it = m_tokenToUserId.find(token);
        if (it != m_tokenToUserId.end())
            return it.value();
        return std::nullopt;
    }

    // 清除用户所有会话（例如修改密码后）
    void clearUserSessions(qint64 userId) {
        QWriteLocker locker(&m_lock);
        auto it = m_userIdToToken.find(userId);
        if (it != m_userIdToToken.end()) {
            m_tokenToUserId.remove(it.value());
            m_userIdToToken.erase(it);
        }
    }

private:
    QHash<QString, qint64> m_tokenToUserId;
    QHash<qint64, QString> m_userIdToToken;
    mutable QReadWriteLock m_lock;
};

#endif // SESSION_H