cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED APP_VERSION OR APP_VERSION STREQUAL "")
    set(APP_VERSION "1.0.0")
endif()

project(ChromaZ VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_compile_definitions(CHROMAZ_VERSION="${APP_VERSION}")

set(BIONET_FILE "${CMAKE_CURRENT_SOURCE_DIR}/bionetc.txt")
if(NOT EXISTS "${BIONET_FILE}")
    message(STATUS "REBASE database not found locally. Downloading link_bionetc...")
    file(DOWNLOAD
        "https://rebase.neb.com/rebase/link_bionetc"
        "${BIONET_FILE}"
        STATUS download_status
        TLS_VERIFY OFF
    )
    list(GET download_status 0 status_code)
    list(GET download_status 1 error_msg)
    if(NOT status_code EQUAL 0)
        message(WARNING "Failed to download REBASE database: ${error_msg}. Will build without bundled database.")
    else()
        message(STATUS "REBASE database downloaded successfully to ${BIONET_FILE}.")
    endif()
endif()

if(CMAKE_CROSSCOMPILING)
    set(Vulkan_INCLUDE_DIR "" CACHE PATH "" FORCE)
    
    find_program(NATIVE_MOC moc PATHS /usr/lib/qt6/libexec /usr/bin NO_DEFAULT_PATH)
    find_program(NATIVE_UIC uic PATHS /usr/lib/qt6/libexec /usr/bin NO_DEFAULT_PATH)
    find_program(NATIVE_RCC rcc PATHS /usr/lib/qt6/libexec /usr/bin NO_DEFAULT_PATH)

    if(NATIVE_MOC)
        set(CMAKE_AUTOMOC_EXECUTABLE "${NATIVE_MOC}")
    endif()
    if(NATIVE_UIC)
        set(CMAKE_AUTOUIC_EXECUTABLE "${NATIVE_UIC}")
    endif()
    if(NATIVE_RCC)
        set(CMAKE_AUTORCC_EXECUTABLE "${NATIVE_RCC}")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test TextToSpeech PrintSupport)

if(CMAKE_CROSSCOMPILING)
    foreach(qt_target Qt6::Core Qt6::Gui Qt6::Widgets Qt6::TextToSpeech Qt6::PrintSupport)
        if(TARGET ${qt_target})
            get_target_property(target_incs ${qt_target} INTERFACE_INCLUDE_DIRECTORIES)
            if(target_incs)
                list(REMOVE_ITEM target_incs "/usr/include")
                set_target_properties(${qt_target} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${target_incs}")
            endif()
        endif()
    endforeach()
endif()

add_executable(ChromaZ WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
    contigwidget.cpp
    contigwidget.h
    ab1parser.cpp
    ab1parser.h
    alignmentengine.cpp
    alignmentengine.h
    resources.qrc
)

if(APPLE)
    set_target_properties(ChromaZ PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "ChromaZ"
        MACOSX_BUNDLE_GUI_IDENTIFIER "net.magisters.chromaz"
        MACOSX_BUNDLE_BUNDLE_VERSION "${APP_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
    )
endif()

target_link_libraries(ChromaZ PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::TextToSpeech Qt6::PrintSupport)

enable_testing()

add_executable(ChromaZTests
    tests/test_main.cpp
    ab1parser.cpp
    ab1parser.h
    alignmentengine.cpp
    alignmentengine.h
)

target_link_libraries(ChromaZTests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME CoreLogicTests COMMAND ChromaZTests)