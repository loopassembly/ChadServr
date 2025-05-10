cmake_minimum_required(VERSION 3.10)
project(ChadServr VERSION 1.0.0)

# Specify the C++ standard to use. We'll be using C++17 features.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON) # Ensure the standard is supported
set(CMAKE_CXX_EXTENSIONS OFF)      # Disable compiler extensions for better portability

# Add some helpful compiler flags for better warnings and code correctness.
# You might want to adjust these based on your compiler and preferences.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

# If the build type isn\'t explicitly set (e.g., with -DCMAKE_BUILD_TYPE=Debug),
# default to Release. Release builds are optimized for performance.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Define where the built executables and libraries will be placed.
# This helps keep the build directory organized.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Add our \'include\' directory so the compiler can find our header files.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Look for essential libraries we need.
# Boost is used for networking (asio) and threading.
# Threads is needed for standard C++ threading support.
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

# Include the nlohmann/json library.
# We are using FetchContent to download it directly if it\'s not
# already available on the system. This simplifies setup.
include(FetchContent)
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.10.5/json.tar.xz
)
FetchContent_MakeAvailable(json)

# List all the source files (.cpp) that make up our server application.
set(SERVER_SOURCES
    src/config.cpp
    src/logger.cpp
    src/thread_pool.cpp
    src/video_processor.cpp
    src/http_server.cpp
    src/storage_manager.cpp
    main.cpp
)

# Define our executable target. It will be named \'chadservr\'.
add_executable(chadservr ${SERVER_SOURCES})

# Link the necessary libraries to our executable.
# PRIVATE means these libraries are only needed for building \'chadservr\',
# not for any other target that might link against \'chadservr\' (though
# in this simple project, there are none).
target_link_libraries(chadservr
    PRIVATE
        Boost::system      # For Boost.Asio
        Boost::thread      # For Boost threading (used internally by Boost.Asio)
        nlohmann_json::nlohmann_json # For JSON handling
        Threads::Threads   # Standard C++ threads
        stdc++fs           # For filesystem operations in C++17
)

# Create the directories that our server expects to find at runtime.
# These will be created inside the build output directory.
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/config)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/logs)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/storage/processed)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/storage/temp)

# Copy the default configuration file into the output directory
# so it\'s available when running the server.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/server_config.json
    ${CMAKE_BINARY_DIR}/bin/config/server_config.json
    COPYONLY
)

# Define installation rules (optional, but good practice).
# This  specifies where \'make install\' would put the files.
install(TARGETS chadservr DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config/ DESTINATION config
    FILES_MATCHING PATTERN \"*.json\" # Only install JSON config files
)

# Print some helpful information at the end of the CMake configuration step.
message(STATUS \"Build configuration complete.\")
message(STATUS \"  C++ Flags: ${CMAKE_CXX_FLAGS}\")
message(STATUS \"  Build Type: ${CMAKE_BUILD_TYPE}\")
message(STATUS \"  Executable will be in: ${CMAKE_BINARY_DIR}/bin\")