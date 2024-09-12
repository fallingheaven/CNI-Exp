cmake_minimum_required(VERSION 3.26)
project(E1_3790)

set(CMAKE_CXX_STANDARD 20)

# 定义源文件目录
aux_source_directory(src SRC_LIST)
aux_source_directory(src/lib LIB_LIST)

set(SOURCES)

if (SRC_LIST)
    list(APPEND SOURCES ${SRC_LIST})
else ()
    message(FATAL_ERROR "No source files found in SRC_LIST for target: ${PROJECT_NAME}")
endif ()

if (LIB_LIST)
    list(APPEND SOURCES ${LIB_LIST})
endif ()

# 生成可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 输出目录（可执行文件）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)

# 头文件
include_directories($src/lib)