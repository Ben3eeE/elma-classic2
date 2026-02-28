# ESP-IDF top-level project CMakeLists.txt for Elma Classic Badge Port
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(elma-badge)
