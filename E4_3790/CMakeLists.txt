cmake_minimum_required(VERSION 3.29)
project(E3_3790)

set(CMAKE_CXX_STANDARD 23)

aux_source_directory(src SRC_LIST)
aux_source_directory(src/lib LIB_LIST)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../bin)

set(SOURCES)

if (SRC_LIST)
    list(APPEND SOURCES ${SRC_LIST})
else ()
    message(FATAL_ERROR "No source files found in SRC_LIST for target: ${PROJECT_NAME}")
endif ()

if (LIB_LIST)
    list(APPEND SOURCES ${LIB_LIST})
endif ()

include_directories("C:/msys64/mingw64/include/pcap")
link_libraries("C:/msys64/mingw64/bin/libpcap.dll")
link_libraries("C:/msys64/mingw64/lib/x64/wpcap.lib")

find_package(Threads REQUIRED)
link_libraries(ws2_32)


# 生成可执行文件
add_executable(ip src/ip.cpp)
add_executable(ftp src/ftp.cpp)
