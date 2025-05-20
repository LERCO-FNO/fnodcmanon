cmake_minimum_required(VERSION 3.28)
project(fnodcmanon)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-static -static-libgcc -static-libstdc++")
set(sqlite3_dll_path ${sqlite3_dll})

set(SOURCES
    main.cpp
    src/DicomAnonymizer.cpp
    src/include/Database.hpp
    src/Database.cpp
    )

find_package(fmt REQUIRED)
find_package(DCMTK REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
                           src/include
                           ${sqlmodern_DIR})

link_directories(${SQLITE_LIB})

target_link_libraries(${PROJECT_NAME} PRIVATE
                      fmt::fmt
                      DCMTK::DCMTK
                      SQLite::SQLite3)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_target(copy_sqlite3_dll ALL
                      COMMAND ${CMAKE_COMMAND} -E copy_if_different
                      "${sqlite3_dll}" $<TARGET_FILE_DIR:${PROJECT_NAME}>
                      COMMENT "Copying SQLite3 DLL for Release build")
    add_dependencies(${PROJECT_NAME} copy_sqlite3_dll)
endif ()