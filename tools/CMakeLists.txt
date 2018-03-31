find_package(Threads REQUIRED)
add_executable(docker docker.cc)
target_link_libraries(docker spectacles ${CMAKE_THREAD_LIBS_INIT})
