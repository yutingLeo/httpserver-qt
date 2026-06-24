#ifndef STORAGE_H
#define STORAGE_H

#include <QtCore/QHash>
#include <QtCore/QReadWriteLock>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <optional>

template<typename T>
class ThreadSafeStorage {
public:
    bool insert(const T &item) {
        QWriteLocker locker(&m_lock);
        if (m_items.contains(item.id) ) return false;
        m_items[item.id] = item;
        return true;
    }

    bool update(qint64 id, const T &item) {
        QWriteLocker locker(&m_lock);
        if (!m_items.contains(id)) return false;
        m_items[id] = item;
        return true;
    }

    bool remove(qint64 id) {
        QWriteLocker locker(&m_lock);
        return m_items.remove(id);
    }

    std::optional<T> get(qint64 id) const {
        QReadLocker locker(&m_lock);
        if (auto it = m_items.find(id); it != m_items.end())
            return *it;
        return std::nullopt;
    }

    QList<T> getAll() const {
        QReadLocker locker(&m_lock);
        return m_items.values();
    }

    qint64 nextId() const {
        QReadLocker locker(&m_lock);
        return maxIdUnsafe() + 1;
    }

    // 原子地分配新 ID 并插入，避免 nextId()+insert() 之间的竞态
    qint64 insertWithNewId(T &item) {
        QWriteLocker locker(&m_lock);
        item.id = maxIdUnsafe() + 1;
        m_items[item.id] = item;
        return item.id;
    }

    // 批量加载（用于初始化）
    void loadFromJsonArray(const QJsonArray &array, std::function<T(const QJsonObject&)> fromJson) {
        QWriteLocker locker(&m_lock);
        for (const auto &val : array) {
            if (val.isObject()) {
                T item = fromJson(val.toObject());
                if (item.id == 0) {
                    item.id = maxIdUnsafe() + 1;
                }
                m_items[item.id] = item;
            }
        }
    }

private:
    // 无锁版本的 maxId 获取，由调用者负责加锁
    qint64 maxIdUnsafe() const {
        qint64 maxId = 0;
        for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it)
            if (it.key() > maxId) maxId = it.key();
        return maxId;
    }
    QHash<qint64, T> m_items;
    mutable QReadWriteLock m_lock;
};

#endif // STORAGE_H