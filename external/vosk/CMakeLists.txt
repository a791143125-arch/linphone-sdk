cmake_minimum_required(VERSION 3.10)
project(vosk)

# Allow override from root
if(NOT DEFINED VOSK_LIB_PATH)
    set(VOSK_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Imported shared library
add_library(vosk SHARED IMPORTED GLOBAL)

set_target_properties(vosk PROPERTIES
    IMPORTED_LOCATION "${VOSK_LIB_PATH}/libvosk.so"
    INTERFACE_INCLUDE_DIRECTORIES "${VOSK_LIB_PATH}"
)