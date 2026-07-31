cmake_minimum_required(VERSION 3.14)
project(zltui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Library: zltui ───────────────────────────────────────
add_library(zltui STATIC src/zltui.cpp include/zltui.h)
target_include_directories(zltui PUBLIC include)

# ── Example: hello ───────────────────────────────────────
add_executable(hello sample/hello/main.cpp)
target_link_libraries(hello PRIVATE zltui)
