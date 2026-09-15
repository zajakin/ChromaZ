cmake_minimum_required(VERSION 3.16)
project(ChromaZ LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Блокировка подтягивания Linux-заголовков Vulkan при кросс-компиляции
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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test)

# Принудительная фильтрация /usr/include из таргетов Qt
if(CMAKE_CROSSCOMPILING)
    foreach(qt_target Qt6::Core Qt6::Gui Qt6::Widgets)
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
)

if(APPLE)
    set_target_properties(ChromaZ PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "ChromaZ"
        MACOSX_BUNDLE_GUI_IDENTIFIER "net.magisters.chromaz"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    )
endif()

target_link_libraries(ChromaZ PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# --- Блок авто-тестов ---
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