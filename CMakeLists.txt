cmake_minimum_required(VERSION 3.16)

project(PCGToVST_UI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(RAPIDJSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/rapidjson/include)

if(NOT EXISTS ${RAPIDJSON_INCLUDE_DIR})
    message(WARNING "RapidJSON not found at ${RAPIDJSON_INCLUDE_DIR}")
    message(WARNING "Please run: git submodule update --init --recursive")
endif()


set(SOURCES_CONVERTER
    PCGConverter/alchemist.cpp
    PCGConverter/alchemist.h
    PCGConverter/helpers.cpp
    PCGConverter/helpers.h
    PCGConverter/pcg_converter.cpp
    PCGConverter/pcg_converter.h
    PCGConverter/pcg_converter_combis.cpp
    PCGConverter/pcg_converter_effects.cpp
    PCGConverter/pcg_converter_gm.cpp
    PCGConverter/pcg_converter_programs.cpp
    PCGConverter/pcg_converter_user.cpp
    PCGConverter/unit_tests.cpp
    PCGConverter/unit_tests.h
)

set(SOURCES_QT_APP
    QtApp/main.cpp
    QtApp/QtPCGToVSTUI.cpp
    QtApp/QtPCGToVSTUI.h
    QtApp/QtPCGToVSTUI.qrc
    QtApp/QtPCGToVSTUI.ui
    QtApp/Worker.cpp
    QtApp/Worker.h
)

set(SOURCES_CLI_APP
    ConsoleApp/main.cpp
)

add_executable(${PROJECT_NAME}_cli
    ${SOURCES_CLI_APP}
    ${SOURCES_CONVERTER}
)

target_include_directories(${PROJECT_NAME}_cli PRIVATE
    ${RAPIDJSON_INCLUDE_DIR}
    ./PCGConverter
)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
)

add_executable(${PROJECT_NAME}_gui
    ${SOURCES_QT_APP}
    ${SOURCES_CONVERTER}
)

target_link_libraries(${PROJECT_NAME}_gui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_include_directories(${PROJECT_NAME}_gui PRIVATE
    ${RAPIDJSON_INCLUDE_DIR}
    ./PCGConverter
)

if(WIN32)
    set_target_properties(${PROJECT_NAME}_gui PROPERTIES
        WIN32_EXECUTABLE ON
    )
    
    get_target_property(QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
    get_filename_component(QT_INSTALL_DIR "${QT_BIN_DIR}" DIRECTORY)
    
    set(QT_DLLS
        Qt6Core$<$<CONFIG:Debug>:d>.dll
        Qt6Gui$<$<CONFIG:Debug>:d>.dll
        Qt6Widgets$<$<CONFIG:Debug>:d>.dll
    )
    
    foreach(QT_DLL ${QT_DLLS})
        add_custom_command(TARGET ${PROJECT_NAME}_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_BIN_DIR}/${QT_DLL}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}_gui>/"
            COMMENT "Copying ${QT_DLL}..."
        )
    endforeach()
    
    add_custom_command(TARGET ${PROJECT_NAME}_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory 
            "$<TARGET_FILE_DIR:${PROJECT_NAME}_gui>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_INSTALL_DIR}/plugins/platforms/qwindows$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}_gui>/platforms/"
        COMMENT "Copying platform plugin..."
    )
    
    add_custom_command(TARGET ${PROJECT_NAME}_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory 
            "$<TARGET_FILE_DIR:${PROJECT_NAME}_gui>/styles"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_INSTALL_DIR}/plugins/styles/qmodernwindowsstyle$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}_gui>/styles/"
        COMMENT "Copying styles plugin..."
    )

    file(GLOB BIN_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.*")
    if(BIN_RESOURCES)
        add_custom_command(TARGET ${PROJECT_NAME}_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}_gui>/Data"
            COMMENT "Creating Data directory..."
        )
        
        foreach(BIN_FILE ${BIN_RESOURCES})
            add_custom_command(TARGET ${PROJECT_NAME}_gui POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${BIN_FILE}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}_gui>/Data/"
                COMMENT "Copying ${BIN_FILE} to Data folder..."
            )
        endforeach()
    endif()
endif()


if(APPLE)
    set_target_properties(${PROJECT_NAME}_gui PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_GUI_IDENTIFIER com.yourcompany.${PROJECT_NAME}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    )

    file(GLOB BIN_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.*")
    if(BIN_RESOURCES)
        foreach(BIN_FILE ${BIN_RESOURCES})
            get_filename_component(BIN_FILENAME ${BIN_FILE} NAME)

            add_custom_command(TARGET ${PROJECT_NAME}_gui POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory 
                    "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}_gui>/Resources/Data"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${BIN_FILE}"
                    "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}_gui>/Resources/Data/${BIN_FILENAME}"
                COMMENT "Copying ${BIN_FILENAME} to app bundle Data folder..."
            )
        endforeach()
    endif()

    get_target_property(QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${QT_BIN_DIR}")
    
    add_custom_command(TARGET ${PROJECT_NAME}_gui POST_BUILD
        COMMAND "${MACDEPLOYQT_EXECUTABLE}"
            "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}_gui>"
            -always-overwrite
        COMMENT "Running macdeployqt to bundle Qt frameworks..."
    )
endif()

if(UNIX AND NOT APPLE)
    file(GLOB BIN_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.*")
    if(BIN_RESOURCES)
        add_custom_command(TARGET ${PROJECT_NAME}_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}_gui>/Data"
            COMMENT "Creating Data directory for Linux..."
        )

        foreach(BIN_FILE ${BIN_RESOURCES})
            add_custom_command(TARGET ${PROJECT_NAME}_gui POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${BIN_FILE}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}_gui>/Data/"
                COMMENT "Copying ${BIN_FILE} to Linux Data folder..."
            )
        endforeach()
    endif()
endif()

install(TARGETS ${PROJECT_NAME}_cli ${PROJECT_NAME}_gui
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
