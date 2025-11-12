cmake_minimum_required(VERSION 3.20)
project(SimpleLabel VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/permissive- /Zc:__cplusplus)
    add_compile_options(/utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets PrintSupport Sql)

set(SIMPLELABEL_ENABLE_QXLSX ON CACHE BOOL "Build with embedded QXlsx library")
set(SIMPLELABEL_ENABLE_ZXING ON CACHE BOOL "Build with embedded ZXing library")

if(SIMPLELABEL_ENABLE_QXLSX)
    add_subdirectory(third_party/QXlsx/QXlsx)
endif()

if(SIMPLELABEL_ENABLE_ZXING)
    set(ZXING_BUILD_TESTS OFF CACHE BOOL "Disable ZXing tests" FORCE)
    set(ZXING_BUILD_EXAMPLES OFF CACHE BOOL "Disable ZXing examples" FORCE)
    add_subdirectory(third_party/zxing-cpp EXCLUDE_FROM_ALL)
endif()

set(QR_CODE_SOURCES
    third_party/QR-Code-generator/cpp/qrcodegen.cpp
    third_party/QR-Code-generator/cpp/qrcodegen.hpp
)

file(GLOB_RECURSE SIMPLELABEL_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
)

file(GLOB_RECURSE SIMPLELABEL_FORMS CONFIGURE_DEPENDS
    src/*.ui
)

set(SIMPLELABEL_RESOURCES
    icons/icons.qrc
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(SimpleLabel
        MANUAL_FINALIZATION
        ${SIMPLELABEL_SOURCES}
        ${SIMPLELABEL_FORMS}
        ${QR_CODE_SOURCES}
        ${SIMPLELABEL_RESOURCES}
    )
else()
    add_executable(SimpleLabel
        ${SIMPLELABEL_SOURCES}
        ${SIMPLELABEL_FORMS}
        ${QR_CODE_SOURCES}
        ${SIMPLELABEL_RESOURCES}
    )
endif()

if(SIMPLELABEL_ENABLE_ZXING)
    target_link_libraries(SimpleLabel PRIVATE ZXing::ZXing)
endif()

if(SIMPLELABEL_ENABLE_QXLSX)
    target_link_libraries(SimpleLabel PRIVATE QXlsx::QXlsx)
    target_compile_definitions(SimpleLabel PRIVATE HAVE_QXLSX)
endif()

target_link_libraries(SimpleLabel PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::PrintSupport
    Qt::Sql
)

target_include_directories(SimpleLabel PRIVATE src)

install(TARGETS SimpleLabel RUNTIME DESTINATION bin)
install(FILES translations/SimpleLabel_en_US.qm translations/SimpleLabel_zh_CN.qm
        DESTINATION share/simplelabel/translations OPTIONAL)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_finalize_executable(SimpleLabel)
endif()
