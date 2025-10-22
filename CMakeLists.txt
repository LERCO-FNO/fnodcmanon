cmake_minimum_required(VERSION 3.28)

project(fnodcmanon LANGUAGES CXX)

find_package(fmt REQUIRED)
find_package(DCMTK REQUIRED)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.cpp src/DicomAnonymizer.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/include)

target_link_libraries(${PROJECT_NAME} PRIVATE
                      fmt::fmt
                      DCMTK::DCMTK)

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX d)

target_link_libraries(${PROJECT_NAME} PRIVATE $<$<AND:$<BOOL:${MINGW}>,$<CONFIG:Release>>:-static>)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
