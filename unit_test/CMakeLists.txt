# Unit test executable
add_executable(unit_test
    main.cpp
)
target_include_directories(unit_test PRIVATE ../include)

# Link against the shared core library from the parent project
target_link_libraries(unit_test PRIVATE zltui)

if(MSVC)
    target_compile_options(unit_test PRIVATE /utf-8 /wd4566)
endif()

