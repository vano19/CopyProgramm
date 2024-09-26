set(SRC_FILES
    CopyManager.cc
    FileSource.cc
    FileDestination.cc
)

add_library(CopyManager OBJECT ${SRC_FILES})

target_include_directories(CopyManager 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(copy copy.cc $<TARGET_OBJECTS:CopyManager>)

target_link_libraries(copy PRIVATE Threads::Threads)