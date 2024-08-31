cmake_minimum_required(VERSION 3.10)
project(AduioEncoder)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)

# 设置CMake安装目录
set(CMAKE_PREFIX_PATH "/usr")

# 导入CUDA包
find_package(CUDA REQUIRED)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)  # 将cmake目录添加到CMAKE_MODULE_PATH
find_package(FFmpeg REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE SWRESAMPLE)        # 导入FFmpeg库

# 添加可执行文件
file(GLOB SOURCE_FILES "src/main.cpp" "src/encoder/audio_encoder_aac.cpp" "src/encoder/audio_encoder_mp3.cpp" "src/decoder/audio_decoder_aac.cpp" "src/decoder/audio_decoder_mp3.cpp")
add_executable(AduioEncoder ${SOURCE_FILES})     # 添加可执行文件

# 添加头文件和链接库
include_directories(${CMAKE_SOURCE_DIR}/include/encoder ${CMAKE_SOURCE_DIR}/include/decoder ${FFMPEG_INCLUDE_DIRS_AVCODEC} ${FFMPEG_INCLUDE_DIRS_AVFORMAT} ${FFMPEG_INCLUDE_DIRS_AVUTIL} ${FFMPEG_INCLUDE_DIRS_SWSCALE} ${FFMPEG_INCLUDE_DIRS_SWRESAMPLE} ${CUDA_INCLUDE_DIRS})
target_link_libraries(AduioEncoder PRIVATE ${FFmpeg_LIBRARIES} ${CUDA_LIBRARIES})

# 设置可执行文件的输出路径
set_target_properties(AduioEncoder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)