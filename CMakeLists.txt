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

link_directories(${SQLITE_LIB})

target_link_libraries(${PROJECT_NAME} PRIVATE
                      fmt::fmt
                      DCMTK::DCMTK
                      SQLite::SQLite3)

set_target_properties(${PROJECT_NAME} PROPERTIES
                      LINK_FLAGS_Release-static "-static -static-libgcc -static-libstdc++ -Wl,-Bstatic -lsqlite3 -Wl,-Bdynamic"
                      )