cmake_minimum_required(VERSION 3.7)
project(transcode)

set(CMAKE_CXX_STANDARD 11)

include_directories(/usr/local/Cellar/ffmpeg/3.3.2/include /usr/local/Cellar/x264/r2748/include /usr/local/Cellar/fdk-aac/0.1.5/include)
#link_directories(/usr/local/Cellar/ffmpeg/3./lib /usr/local/Cellar/x264/r2748/lib /usr/local/Cellar/fdk-aac/0.1.5/lib)

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
        fdk-aac
)