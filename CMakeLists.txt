cmake_minimum_required (VERSION 3.0)

project(spectacles)

set(CMAKE_CXX_STANDARD 11)
set(SPECTACLES_INCLUDE_DIR ${LIBSPECTACLES_INCLUDE_DIRS}/include)

include(ExternalProject)
include(FindZLIB)

add_subdirectory(tools)

find_library(UWS_FOUND uWS)
if (NOT UWS_FOUND)
    message(STATUS "Could NOT find UWS")
    ExternalProject_Add(
        uWS_ext

        GIT_REPOSITORY "https://github.com/uNetworking/uWebSockets.git"
        GIT_TAG "v0.14.5"

        CONFIGURE_COMMAND ""
        BUILD_COMMAND "make"
        BUILD_IN_SOURCE true
    )
endif(NOT UWS_FOUND)

find_library(RABBITMQ_FOUND rabbitmq)
if (NOT RABBITMQ_FOUND)
    message(STATUS "Could NOT find RABBITMQ")
    ExternalProject_Add(
        rabbitmq_ext

        # GIT_REPOSITORY "https://github.com/alanxz/rabbitmq-c.git"
        # GIT_TAG "v0.8.0"

        URL "https://github.com/alanxz/rabbitmq-c/releases/download/v0.8.0/rabbitmq-c-0.8.0.tar.gz"

        CMAKE_ARGS -DBUILD_EXAMPLES=OFF -DBUILD_STATIC_LIBS=OFF -DBUILD_TESTS=OFF -DBUILD_TOOLS=OFF -DBUILD_TOOLS_DOCS=OFF -DBUILD_API_DOCS=OFF -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    )
endif(NOT RABBITMQ_FOUND)

add_library(spectacles SHARED src/gateway.cc src/broker.cc include/utils.h src/utils.c)

if (NOT UWS_FOUND)
    add_dependencies(spectacles uWS_ext)
endif (NOT UWS_FOUND)

if (NOT RABBITMQ_FOUND)
    add_dependencies(spectacles rabbitmq_ext)
endif (NOT RABBITMQ_FOUND)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries(spectacles uWS rabbitmq ${ZLIB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS spectacles DESTINATION lib)
install(FILES include/spectacles.h include/broker.h include/gateway.h include/utils.h DESTINATION include/spectacles)
install(DIRECTORY include/etf DESTINATION include/spectacles)
