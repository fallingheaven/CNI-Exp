cmake_minimum_required(VERSION 3.26)
project(E1_3790)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
#让链接器静态链接GCC和C++标准库

# 定义源文件目录
aux_source_directory(src SRC_LIST)
aux_source_directory(src/lib LIB_LIST)

# 输出目录（可执行文件）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../bin)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
link_libraries(${OpenCV_LIBS})

set(SOURCES)

if (SRC_LIST)
    list(APPEND SOURCES ${SRC_LIST})
else ()
    message(FATAL_ERROR "No source files found in SRC_LIST for target: ${PROJECT_NAME}")
endif ()

if (LIB_LIST)
    list(APPEND SOURCES ${LIB_LIST})
endif ()

# 这个记得在add_executable之前
set(QrCodePath src/lib/QR-Code-generator/cpp)
add_library(qrcodegen STATIC ${QrCodePath}/qrcodegen.hpp ${QrCodePath}/qrcodegen.cpp)
include_directories(${QrCodePath})
link_libraries(qrcodegen)

find_library(QRENCODE_LIBRARY qrencode REQUIRED)
link_libraries(${QRENCODE_LIBRARY})

#find_package(Qt6 REQUIRED COMPONENTS Widgets)
#find_package(Qt6 REQUIRED COMPONENTS Core)
#include_directories(${Qt_INCLUDE_DIRS})
#link_libraries(Qt6::Widgets Qt6::Core)
#
#set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTORCC ON)
#set(CMAKE_AUTOUIC ON)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
link_libraries(${ZLIB_LIBRARIES})

include_directories(C:/msys64/mingw64/include/zbar)
link_libraries(C:/msys64/mingw64/lib/libzbar.dll.a)

# 生成可执行文件
add_executable(encode ${SOURCES})
# 区分是为了方便在IDE中测试
add_executable(decode ${SOURCES})

add_executable(QrCodeTest src/debug/QrCodeTest.cpp)
add_executable(Check_Img_Identical src/debug/check_img_identical.cpp)
add_executable(progress_bar src/debug/progress_bar.cpp)
