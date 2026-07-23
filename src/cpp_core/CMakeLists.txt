cmake_minimum_required(VERSION 3.15)
project(ChromiumDevToolsCore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src/cpp_core)

add_executable(devtools_core src/cpp_core/main.cpp)

if(EMSCRIPTEN)
    set_target_properties(devtools_core PROPERTIES OUTPUT_NAME "devtools_core")
    target_compile_options(devtools_core PRIVATE -O3 -sUSE_PTHREADS=1)
    target_link_options(devtools_core PRIVATE
        -O3
        -sWASM=1
        -sALLOW_MEMORY_GROWTH=1
        -sEXPORTED_RUNTIME_METHODS=['cwrap','ccall','getValue','setValue','UTF8ToString','stringToUTF8']
        -sEXPORT_ES6=1
        -sMODULARIZE=1
        -sEXPORT_NAME="DevToolsCoreModule"
        -sUSE_PTHREADS=1
        -sPTHREAD_POOL_SIZE=4
    )
endif()
