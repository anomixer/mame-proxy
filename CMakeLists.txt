cmake_minimum_required(VERSION 3.10)

project(MameCloudRompath)

set(EXECUTABLE_NAME mcr)

set(CMAKE_CXX_STANDARD 17)

# Find WinFsp
# Assumes standard installation path. Users can override with -DWINFSP_PATH="..."
set(WINFSP_PATH "C:/Program Files (x86)/WinFsp" CACHE PATH "Path to WinFsp installation")

if(NOT EXISTS "${WINFSP_PATH}/inc/winfsp/winfsp.h")
    message(FATAL_ERROR "WinFsp not found at ${WINFSP_PATH}. Please install WinFsp or set WINFSP_PATH correctly.")
endif()

include_directories("${WINFSP_PATH}/inc")
link_directories("${WINFSP_PATH}/lib")

# Source files
set(SOURCES
    src/main.cpp
    src/MameFs.cpp
    src/MameFs.h
    src/Downloader.cpp
    src/Downloader.h
)

add_executable(${EXECUTABLE_NAME} ${SOURCES})

set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    CXX_STANDARD 17
    VS_DEBUGGER_COMMAND_ARGUMENTS "-m Z: -c C:/MameCache -u https://mdk.cab/download/"
)

# Link against WinFsp and WinHTTP
target_link_libraries(${EXECUTABLE_NAME} "${WINFSP_PATH}/lib/winfsp-x64.lib" winhttp.lib user32.lib advapi32.lib)

# Copy WinFsp DLL to output directory
add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${WINFSP_PATH}/bin/winfsp-x64.dll"
    $<TARGET_FILE_DIR:MameProxy>
)
