cmake_minimum_required(VERSION 3.20)
project(ShutdownTimer VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 auto-tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    LinguistTools
)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/TimerEngine.cpp
    src/ShutdownManager.cpp
    src/RegistryManager.cpp
    src/TaskScheduler.cpp
    src/LanguageManager.cpp
)

set(HEADERS
    src/MainWindow.h
    src/TimerEngine.h
    src/ShutdownManager.h
    src/RegistryManager.h
    src/TaskScheduler.h
    src/LanguageManager.h
)

# ── Windows resources (icon + version info + manifest embed) ─────────────────
set(WIN_RESOURCES resources.rc)

# ── Qt resource file (icon for tray / window) ─────────────────────────────────
set(QT_RESOURCES resources.qrc)

add_executable(ShutdownTimer WIN32
    ${SOURCES}
    ${HEADERS}
    ${WIN_RESOURCES}
    ${QT_RESOURCES}
)

# ── Translations ──────────────────────────────────────────────────────────────
# Compile .ts -> .qm using lrelease, output next to the exe in an i18n subfolder
find_program(LRELEASE_EXECUTABLE lrelease
    HINTS "${Qt6_DIR}/../../../bin"
)

set(TS_FILES
    i18n/app_en.ts
    i18n/app_ar.ts
    i18n/app_ko.ts
    i18n/app_es.ts
    i18n/app_fr.ts
    i18n/app_de.ts
    i18n/app_pt_BR.ts
    i18n/app_zh_CN.ts
    i18n/app_ja.ts
)

foreach(TS_FILE ${TS_FILES})
    get_filename_component(QM_BASE ${TS_FILE} NAME_WE)
    set(QM_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/i18n/${QM_BASE}.qm")

    add_custom_command(
        OUTPUT  ${QM_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/i18n"
        COMMAND ${LRELEASE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/${TS_FILE}"
                -qm "${QM_OUTPUT}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${TS_FILE}"
        COMMENT "Compiling ${TS_FILE} -> ${QM_OUTPUT}"
    )
    list(APPEND QM_FILES ${QM_OUTPUT})
endforeach()

add_custom_target(Translations ALL DEPENDS ${QM_FILES})
add_dependencies(ShutdownTimer Translations)

# Copy i18n folder next to the exe after build
add_custom_command(TARGET ShutdownTimer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/i18n"
        "$<TARGET_FILE_DIR:ShutdownTimer>/i18n"
    COMMENT "Copying translations to output directory"
)

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(ShutdownTimer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    advapi32
    user32
    shell32
    ole32
    oleaut32
    taskschd
    PowrProf
)

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(ShutdownTimer PRIVATE
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)

# ── Compiler warnings ─────────────────────────────────────────────────────────
target_compile_options(ShutdownTimer PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# ── Post-build message ────────────────────────────────────────────────────────
add_custom_command(TARGET ShutdownTimer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo
        "Build complete: $<TARGET_FILE:ShutdownTimer>"
)
