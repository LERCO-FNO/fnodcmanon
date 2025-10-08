cmake_minimum_required(VERSION 3.28)
project(fnodcmanon)

set(CMAKE_CXX_STANDARD 20)

set(SOURCES
    main.cpp
    src/DicomAnonymizer.cpp
    src/include/DicomAnonymizer.hpp
    )

find_package(fmt REQUIRED)
find_package(DCMTK REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
                           src/include)

target_link_libraries(${PROJECT_NAME} PRIVATE
                      fmt::fmt
                      DCMTK::DCMTK)
