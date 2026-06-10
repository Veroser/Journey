cmake_minimum_required(VERSION 3.16)

project(JournalTest VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Sql)

set(PROJECT_SOURCES
    main.cpp

    # UI - Login
    ui/login/loginwindow.cpp
    ui/login/loginwindow.h
    ui/login/loginwindow.ui

    # UI - Main
    ui/main/mainwindow.cpp
    ui/main/mainwindow.h
    ui/main/mainwindow.ui
    ui/main/ScheduleWidget.cpp
    ui/main/ScheduleWidget.h

    # Core - Auth
    core/auth/apiclient.cpp
    core/auth/apiclient.h
    core/auth/authmanager.cpp
    core/auth/authmanager.h
    core/auth/AuthModels.h

    # Core - Schedule
    core/schedule/ScheduleManager.cpp
    core/schedule/ScheduleManager.h
    core/schedule/ScheduleModels.h
    core/schedule/parserrunner.cpp
    core/schedule/ParserRunner.h
)

qt_add_executable(JournalTest
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    data/db/parser_homework.py
)

target_include_directories(JournalTest PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ui/login
    ${CMAKE_SOURCE_DIR}/ui/main
    ${CMAKE_SOURCE_DIR}/core/auth
    ${CMAKE_SOURCE_DIR}/core/schedule
)

target_link_libraries(JournalTest PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
)

add_custom_command(TARGET JournalTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data/db
    $<TARGET_FILE_DIR:JournalTest>/db
    COMMENT "Копирование файлов базы данных..."
)

set_target_properties(JournalTest PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.topacademy.journaltest
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

qt_finalize_executable(JournalTest)

# Установка
include(GNUInstallDirs)
install(TARGETS JournalTest
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Установка базы данных
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/db/
    DESTINATION db
    FILES_MATCHING PATTERN "*.py"
    PATTERN "*.db" EXCLUDE
)