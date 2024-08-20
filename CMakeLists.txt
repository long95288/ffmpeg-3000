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


add_library(capture_screen
        ${CMAKE_CURRENT_SOURCE_DIR}/capture_screen/capture_screen_windows.c
        ${CMAKE_CURRENT_SOURCE_DIR}/capture_screen/video_encode.c
        ${CMAKE_CURRENT_SOURCE_DIR}/capture_screen/video_scale.c
        )
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

#test
add_executable(audio_resample_test codec_audio/audio_resample_test.c)
target_link_libraries(audio_resample_test ${ffmpeg_lib})

add_executable(simple_read_g711a_file_test codec_audio/simple_read_g711a_file_test.c)
target_link_libraries(simple_read_g711a_file_test ${ffmpeg_lib})

add_executable(simple_read_h265_file_test codec_video/simple_read_h265_file_test.c)
target_link_libraries(simple_read_h265_file_test ${ffmpeg_lib})

add_executable(simple_read_h265_file_test2 codec_video/simple_read_h265_file_test2.c)
target_link_libraries(simple_read_h265_file_test2 ${ffmpeg_lib})

include_directories(capture_screen)
add_executable(video_capture_mux_out_mp4 muxing/video_capture_mux_out_mp4.c)
target_link_libraries(video_capture_mux_out_mp4 capture_screen ${ffmpeg_lib})


