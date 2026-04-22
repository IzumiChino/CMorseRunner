cmake_minimum_required(VERSION 3.28)
project(MorseRunner VERSION 1.68 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

set(SOURCES
    src/main.cpp
    src/audio/snd_types.cpp
    src/audio/morse_key.cpp
    src/audio/rnd_func.cpp
    src/audio/mixers.cpp
    src/audio/mov_avg.cpp
    src/audio/quick_avg.cpp
    src/audio/volume_ctl.cpp
    src/audio/qsb.cpp
    src/audio/wav_file.cpp
    src/audio/audio_output.cpp
    src/core/ini.cpp
    src/core/station.cpp
    src/core/my_station.cpp
    src/core/dx_oper.cpp
    src/core/dx_station.cpp
    src/core/qrn_stn.cpp
    src/core/qrm_stn.cpp
    src/core/stn_coll.cpp
    src/core/call_list.cpp
    src/core/log.cpp
    src/core/contest.cpp
    src/ui/main_window.cpp
    src/ui/score_dialog.cpp
)

add_executable(MorseRunner ${SOURCES})

target_include_directories(MorseRunner PRIVATE
    src
    ${PORTAUDIO_INCLUDE_DIRS}
)

target_link_libraries(MorseRunner PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    ${PORTAUDIO_LIBRARIES}
)

target_compile_options(MorseRunner PRIVATE
    -Wall -Wextra
    ${PORTAUDIO_CFLAGS_OTHER}
)

# Copy Master.dta if it exists
if(EXISTS "${CMAKE_SOURCE_DIR}/Master.dta")
    configure_file(Master.dta ${CMAKE_BINARY_DIR}/Master.dta COPYONLY)
endif()
