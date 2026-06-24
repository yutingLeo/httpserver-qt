# Copyright (C) 2026 liyuting.xa@163.com

cmake_minimum_required(VERSION 3.16)
project(qyhttpserver LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui HttpServer Concurrent)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(qyhttpserver
    main.cpp
    types.h
    utils.h
    handler.h
    session.h
    storage.h
)

qt_add_resources(qyhttpserver "assets"
    PREFIX "/"
    FILES
    assets/colors.json
    assets/users.json
    assets/sessions.json
    assets/img/1-image.jpg
    assets/img/2-image.jpg
    assets/img/3-image.jpg
    assets/img/4-image.jpg
    assets/img/5-image.jpg
    assets/img/6-image.jpg
    assets/img/7-image.jpg
    assets/img/8-image.jpg
    assets/img/9-image.jpg
    assets/img/10-image.jpg
    assets/img/11-image.jpg
    assets/img/12-image.jpg
)

target_link_libraries(qyhttpserver  PRIVATE
    Qt::Concurrent
    Qt6::Gui
    Qt6::HttpServer
)

# Windows打包输出
qt_finalize_executable(qyhttpserver)
