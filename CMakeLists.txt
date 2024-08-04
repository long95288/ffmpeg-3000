cmake_minimum_required(VERSION 3.16)
project(ffmpeg-3000)


include_directories(dep/ffmpeg-6.1.1-full_build-shared/include)
link_directories(dep/ffmpeg-6.1.1-full_build-shared/lib)

#set(ffmpeg_dep_lib mp3lame z lzma Xext X11 vdpau sndio asound xcb xcb-shm X11-xcb xcb-xfixes xcb-sync xcb-shape bz2 x265 x264 postproc crypt)
set(ffmpeg_lib
        -Wl,--start-group
        libavdevice.a libavfilter.a libswresample.a libswscale.a libavutil.a libavformat.a libavcodec.a
        -Wl,--end-group
        )


add_library(capture_screen ${CMAKE_CURRENT_SOURCE_DIR}/capture_screen/capture_screen.c)
#
add_executable(capture_screen_test  ${CMAKE_CURRENT_SOURCE_DIR}/capture_screen/capture_screen_test.c )
target_link_libraries(capture_screen_test capture_screen ${ffmpeg_lib})
#

add_library(capture_audio ${CMAKE_CURRENT_SOURCE_DIR}/capture_audio/capture_audio_windows.c)
add_executable(capture_audio_test  ${CMAKE_CURRENT_SOURCE_DIR}/capture_audio/capture_audio_test.c)
target_link_libraries(capture_audio_test capture_audio  ${ffmpeg_lib})


add_library(decode_audio ${CMAKE_CURRENT_SOURCE_DIR}/codec_audio/decode_audio.c)
add_executable(decode_audio_test  ${CMAKE_CURRENT_SOURCE_DIR}/codec_audio/decode_audio_test.c)
target_link_libraries(decode_audio_test decode_audio  ${ffmpeg_lib})
