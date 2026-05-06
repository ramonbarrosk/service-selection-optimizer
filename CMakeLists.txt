cmake_minimum_required(VERSION 3.16)
project(service-selection-optimizer
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/util/MathOperations.hpp
    src/util/RandomUtil.hpp
    src/validator/SolutionValidator.hpp
    src/search/GenericSearcher.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/basic
    ${CMAKE_SOURCE_DIR}/src/enum
    ${CMAKE_SOURCE_DIR}/src/instance
    ${CMAKE_SOURCE_DIR}/src/util
    ${CMAKE_SOURCE_DIR}/src/validator
    ${CMAKE_SOURCE_DIR}/src/search
)
