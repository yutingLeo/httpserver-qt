#include <QtCore/QCoreApplication>
#include <QtHttpServer/QHttpServer>
#include <QtCore/QCommandLineParser>
#include <QtCore/QtDebug>
#include <memory>
#include <QtNetwork/QTcpServer>

#include "storage.h"
#include "session.h"
#include "handler.h"
#include "utils.h"
#include "types.h"

constexpr quint16 DEFAULT_PORT = 49425;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addOptions({
                       {{"p", "port"}, "Port to listen on", "port"},
                       });
    parser.addHelpOption();
    parser.process(app);

    quint16 port = DEFAULT_PORT;
    if (!parser.value("port").isEmpty())
        port = parser.value("port").toUShort();

    // 初始化存储
    ThreadSafeStorage<User> userStorage;
    ThreadSafeStorage<Color> colorStorage;
    SessionManager sessionManager;

    // 从资源文件加载初始数据（如果存在）
    loadFromResource<User>(":/assets/users.json", userStorage,
                           [](const QJsonObject &obj) -> User {
                               User u;
                               u.id = obj["id"].toInteger();
                               u.email = obj["email"].toString();
                               u.firstName = obj["first_name"].toString();
                               u.lastName = obj["last_name"].toString();
                               u.avatarUrl = QUrl(obj["avatar"].toString());
                               u.passwordHash = hashPassword(obj["password"].toString());
                               u.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODateWithMs);
                               u.updatedAt = QDateTime::fromString(obj["updatedAt"].toString(), Qt::ISODateWithMs);
                               return u;
                           });
    loadFromResource<Color>(":/assets/colors.json", colorStorage,
                            [](const QJsonObject &obj) -> Color {
                                Color c;
                                c.id = obj["id"].toInteger();
                                c.name = obj["name"].toString();
                                c.color = QColor(obj["color"].toString());
                                c.pantone = obj["pantone_value"].toString();
                                c.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODateWithMs);
                                c.updatedAt = QDateTime::fromString(obj["updatedAt"].toString(), Qt::ISODateWithMs);
                                return c;
                            });

    // 创建处理器（传递指针，生命周期由main管理）
    UserHandler userHandler(&userStorage, &sessionManager);
    ColorHandler colorHandler(&colorStorage, &sessionManager);
    AuthHandler authHandler(&userStorage, &sessionManager);

    QHttpServer server;

    // ----- 用户资源 -----
    // GET /api/users?page=1&per_page=10
    server.route("/api/users", QHttpServerRequest::Method::Get,
                 [&userHandler](const QHttpServerRequest &req) {
                     int page = req.query().queryItemValue("page").toInt();
                     int perPage = req.query().queryItemValue("per_page").toInt();
                     if (page < 1) page = 1;
                     if (perPage < 1) perPage = 10;
                     return userHandler.list(page, perPage);
                 });
    // GET /users/<id>
    server.route("/api/users/<arg>", QHttpServerRequest::Method::Get,
                 [&userHandler](qint64 id) {
                     return userHandler.get(id);
                 });
    // POST /users (需要认证)
    server.route("/api/users", QHttpServerRequest::Method::Post,
                 [&userHandler](const QHttpServerRequest &req) {
                     auto json = parseJsonObject(req.body());
                     if (!json) return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
                     QString token = extractToken(req);
                     return userHandler.create(*json, token);
                 });
    // PUT /users/<id> (全量更新，需要认证)
    server.route("/api/users/<arg>", QHttpServerRequest::Method::Put,
                 [&userHandler](qint64 id, const QHttpServerRequest &req) {
                     auto json = parseJsonObject(req.body());
                     if (!json) return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
                     QString token = extractToken(req);
                     return userHandler.update(id, *json, token, false);
                 });
    // PATCH /users/<id> (部分更新，需要认证)
    server.route("/api/users/<arg>", QHttpServerRequest::Method::Patch,
                 [&userHandler](qint64 id, const QHttpServerRequest &req) {
                     auto json = parseJsonObject(req.body());
                     if (!json) return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
                     QString token = extractToken(req);
                     return userHandler.update(id, *json, token, true);
                 });
    // DELETE /users/<id> (需要认证)
    server.route("/api/users/<arg>", QHttpServerRequest::Method::Delete,
                 [&userHandler](qint64 id, const QHttpServerRequest &req) {
                     QString token = extractToken(req);
                     return userHandler.remove(id, token);
                 });

    // ----- 颜色资源 -----
    server.route("/api/unknown", QHttpServerRequest::Method::Get,
                 [&colorHandler](const QHttpServerRequest &req) {
                     int page = req.query().queryItemValue("page").toInt();
                     int perPage = req.query().queryItemValue("per_page").toInt();
                     if (page < 1) page = 1;
                     if (perPage < 1) perPage = 10;
                     return colorHandler.list(page, perPage);
                 });
    server.route("/api/unknown/<arg>", QHttpServerRequest::Method::Get,
                 [&colorHandler](qint64 id) {
                     return colorHandler.get(id);
                 });
    server.route("/api/unknown", QHttpServerRequest::Method::Post,
                 [&colorHandler](const QHttpServerRequest &req) {
                     auto json = parseJsonObject(req.body());
                     if (!json) return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
                     QString token = extractToken(req);
                     return colorHandler.create(*json, token);
                 });
    server.route("/api/unknown/<arg>", QHttpServerRequest::Method::Put,
                 [&colorHandler](qint64 id, const QHttpServerRequest &req) {
                     auto json = parseJsonObject(req.body());
                     if (!json) return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
                     QString token = extractToken(req);
                     return colorHandler.update(id, *json, token, false);
                 });
    server.route("/api/unknown/<arg>", QHttpServerRequest::Method::Patch,
                 [&colorHandler](qint64 id, const QHttpServerRequest &req) {
                     auto json = parseJsonObject(req.body());
                     if (!json) return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
                     QString token = extractToken(req);
                     return colorHandler.update(id, *json, token, true);
                 });
    server.route("/api/unknown/<arg>", QHttpServerRequest::Method::Delete,
                 [&colorHandler](qint64 id, const QHttpServerRequest &req) {
                     QString token = extractToken(req);
                     return colorHandler.remove(id, token);
                 });

    // ----- 认证路由 -----
    server.route("/api/register", QHttpServerRequest::Method::Post,
                 [&authHandler](const QHttpServerRequest &req) {
                     auto json = parseJsonObject(req.body());
                     if (!json) return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
                     return authHandler.registerUser(*json);
                 });
    server.route("/api/login", QHttpServerRequest::Method::Post,
                 [&authHandler](const QHttpServerRequest &req) {
                     auto json = parseJsonObject(req.body());
                     if (!json) return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
                     return authHandler.login(*json);
                 });
    server.route("/api/logout", QHttpServerRequest::Method::Post,
                 [&authHandler](const QHttpServerRequest &req) {
                     QString token = extractToken(req);
                     return authHandler.logout(token);
                 });

    // ----- 静态图片服务 -----
    server.route("/img/faces/<arg>-image.jpg", QHttpServerRequest::Method::Get,
                 [](qint64 imageId) {
                     QString path = QString(":/assets/img/%1-image.jpg").arg(imageId);
                     return QHttpServerResponse::fromFile(path);
                 });

    // 根路径
    server.route("/", []() {
        return "Production-ready REST API server (Qt HttpServer)";
    });

    // 启动服务器
    auto tcpServer = std::make_unique<QTcpServer>();
    if (!tcpServer->listen(QHostAddress::Any, port)) {
        qCritical() << "Failed to listen on port" << port;
        return 1;
    }
    if (!server.bind(tcpServer.get())) {
        qCritical() << "Failed to bind server to TCP socket";
        return 1;
    }
    // 释放所有权给server？ QHttpServer::bind不接管所有权，我们需要手动管理，但server生命周期长，直接泄漏也可接受；但更好的做法：
    tcpServer.release(); // 让server管理？实际上没有，但为了不析构，泄漏内存可接受（程序退出时OS回收）
    // 更干净的做法：使用QSharedPointer，但为了简洁，这里不深究。

    qDebug() << "Server listening on http://127.0.0.1:" << port;
    return app.exec();
}