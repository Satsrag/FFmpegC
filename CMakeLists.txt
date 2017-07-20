cmake_minimum_required(VERSION 3.7)
project(transcode)

set(CMAKE_CXX_STANDARD 11)

include_directories(/usr/local/Cellar/ffmpeg/3.3.1/include /usr/local/Cellar/x264/r2748/include)
link_directories(/usr/local/Cellar/ffmpeg/3.3.1/lib /usr/local/Cellar/x264/r2748/lib)

set(SOURCE_FILES Main.c Compress.c Compress.h Log.h)
add_executable(transcode ${SOURCE_FILES})

target_link_libraries(
        transcode
        avcodec
        avdevice
        avfilter
        avformat
        avresample
        avutil
        postproc
        swresample
        swscale
        x264
)