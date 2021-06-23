cmake_minimum_required(VERSION 3.16)

project(wav2mp3_converter)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Werror -Wall -Wextra" )

    find_library(lame_lib libmp3lame.a)
    set(thread_lib pthread)

    # fallback in case of lib is not installed
    if(${lame_lib} STREQUAL "lame_lib-NOTFOUND")
        set(lame_lib mp3lame.a)
        link_directories(lib)
        include_directories(lame)
        message(WARNING "Using lib from repo "  ${lame_lib} )
    endif()
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    find_library(thread_lib pthreadGC2.dll)
    find_library(lame_lib libmp3lame64.lib)

    # fallback in case of lib is not installed
    if(${lame_lib} STREQUAL "lame_lib-NOTFOUND")
        link_directories(lib)
        set(lame_lib libmp3lame64.lib)
        include_directories(lame)
        message(WARNING "Using lib from repo "  ${lame_lib} )
    endif()

    if(${thread_lib} STREQUAL "thread_lib-NOTFOUND")
        link_directories(lib)
        set(thread_lib pthreadGC2.dll)
        message(WARNING "Using lib from repo "  ${thread_lib} )
    endif()
endif ()

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} ${lame_lib} ${thread_lib})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
