cmake_minimum_required(VERSION 3.8)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(HunterGate)
HunterGate(
        URL "https://github.com/ruslo/hunter/archive/v0.19.144.tar.gz"
        SHA1 "e3efd470f8b60468db8730eac8c04792f77fe1b1"
)

project(WavRead)

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
        implement_indent_write.hpp
        implement_indent_write_base.hpp
        project.hpp
        main.cpp)

add_executable(wavread ${SOURCE_FILES})
include(AddBoost)
AddBoost(wavread COMPONENTS filesystem system log thread iostreams)
add_subdirectory(sample)
