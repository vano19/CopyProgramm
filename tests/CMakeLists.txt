add_executable(CopyManagerTest CopyManagerTest.cc)
target_link_libraries(CopyManagerTest 
    PRIVATE 
        Catch2::Catch2WithMain 
        Threads::Threads
        $<TARGET_OBJECTS:CopyManager>
)

target_include_directories(CopyManagerTest PRIVATE ${CMAKE_SOURCE_DIR}/src)

include(CTest)
add_test(NAME CopyManagerTest COMMAND CopyManagerTest -a)