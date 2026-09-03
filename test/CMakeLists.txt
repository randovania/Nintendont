cmake_minimum_required(VERSION 3.23)

project(NintendontTest)

add_library(test_headers)
set_property(TARGET test_headers PROPERTY C_STANDARD 90)
target_sources(test_headers 
    PRIVATE
        ../kernel/network/net_memory_operation.c
        stubs/syscalls.c

    PUBLIC
        FILE_SET nintendont_headers
        TYPE HEADERS
        BASE_DIRS 
            stubs
            ../kernel
            
        FILES
            stubs/global.h
            ../kernel/network/net_memory_operation.h
            
)


add_library(unity)
set_target_properties(unity PROPERTIES LINKER_LANGUAGE C)
target_sources(unity
    PRIVATE
        Unity/src/unity.c

    PUBLIC
        FILE_SET unity_headers
        TYPE HEADERS
        BASE_DIRS 
            Unity/src

        FILES
            Unity/src/unity.h

)


add_executable(test_exec)
set_property(TARGET test_exec PROPERTY C_STANDARD 90)
target_sources(test_exec
    PRIVATE
        test_net_memory_operation.c

)
target_link_libraries(test_exec 
    PUBLIC
        unity
        test_headers
)