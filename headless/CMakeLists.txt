cmake_minimum_required(VERSION 3.10)
project(dftfringe-cli VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(dftfringe-cli
    main.cpp
    vortex.cpp
    punwrap.cpp
    zernikepolar.cpp
    zernfit.cpp
    base64.cpp
    protocol.cpp
    sidecar.cpp
)

target_include_directories(dftfringe-cli PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(dftfringe-cli PRIVATE
    ${OpenCV_LIBS}
)

install(TARGETS dftfringe-cli DESTINATION bin)
