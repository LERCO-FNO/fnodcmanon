cmake_minimum_required(VERSION 3.28)
project(fnodcmanon)

set(CMAKE_CXX_STANDARD 20)

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

target_link_libraries(${PROJECT_NAME} PRIVATE
                      fmt::fmt
                      DCMTK::DCMTK
                      SQLite::SQLite3)