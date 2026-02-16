cmake_minimum_required(VERSION 3.20)
project(knotepad VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(QT_DEFAULT_MAJOR_VERSION 6)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets PrintSupport)
find_package(KF6 REQUIRED COMPONENTS
    CoreAddons
    I18n
    XmlGui
    WidgetsAddons
    Config
    ConfigWidgets
    KIO
    ColorScheme
)

add_executable(knotepad
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/documenttab.cpp
    src/documenttab.h
    src/rtfhandler.cpp
    src/rtfhandler.h
    src/sessionmanager.cpp
    src/sessionmanager.h
)

target_link_libraries(knotepad
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    KF6::CoreAddons
    KF6::I18n
    KF6::XmlGui
    KF6::WidgetsAddons
    KF6::ConfigCore
    KF6::ConfigWidgets
    KF6::KIOWidgets
    KF6::ColorScheme
)

install(TARGETS knotepad ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
