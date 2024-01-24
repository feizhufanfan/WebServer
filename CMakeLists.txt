cmake_minimum_required(VERSION 3.10)
project(WebServer VERSION 1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 11)





include(CheckStructHasMember)
include(CheckSymbolExists)

list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_struct_has_member("struct mmsghdr" msg_hdr sys/socket.h HAVE_MMSG_HDR)
check_symbol_exists(sendmmsg sys/socket.h HAVE_SENDMMSG_API)
check_symbol_exists(recvmmsg sys/socket.h HAVE_RECVMMSG_API)

if(HAVE_MMSG_HDR)
    add_definitions(-DHAVE_MMSG_HDR)
endif()
if(HAVE_SENDMMSG_API)
    add_definitions(-DHAVE_SENDMMSG_API)
endif()
if(HAVE_RECVMMSG_API)
    add_definitions(-DHAVE_RECVMMSG_API)
endif()

FILE(GLOB SOURECES src/main.cpp src/WebApi.cpp
        ${CMAKE_SOURCE_DIR}/src/httpMode/*.cpp
        ${CMAKE_SOURCE_DIR}/3rdpart/Tools/include/Common/*.cpp
        ${CMAKE_SOURCE_DIR}/3rdpart/Tools/src/*.cpp
        ${CMAKE_SOURCE_DIR}/3rdpart/Tools/src/*.c
        )

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    MESSAGE(STATUS "add NacosClient Suppert")
    LIST(APPEND SOURECES "src/NacosClient.cpp")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")


ENDIF()


add_executable(${PROJECT_NAME} ${SOURECES})


add_subdirectory(3rdpart/jsoncpp jsoncpp_lib)
add_dependencies(${PROJECT_NAME} jsoncpp_lib)
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    MESSAGE(STATUS "current platform: Linux ")
    target_include_directories(${PROJECT_NAME} PRIVATE
            src/include
            ${CMAKE_SOURCE_DIR}/3rdpart/nacos-sdk-cpp/include
            ${CMAKE_SOURCE_DIR}/3rdpart/Tools/include
            ${CMAKE_SOURCE_DIR}/src/httpMode
            ${CMAKE_SOURCE_DIR}/3rdpart/jsoncpp/include
            ${JsonCpp_INCLUDE_DIR}
            )
    target_link_directories(${PROJECT_NAME} PRIVATE
            ${CMAKE_SOURCE_DIR}/3rdpart/nacos-sdk-cpp/lib

            ${CMAKE_BINARY_DIR}/lib
            )

    target_link_libraries(${PROJECT_NAME} PRIVATE
            nacos-cli
            jsoncpp
            pthread
            )
    add_subdirectory(3rdpart/nacos-sdk-cpp nacos-cli)
    add_dependencies(${PROJECT_NAME} nacos-cli)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    MESSAGE(STATUS "current platform: Windows")
    target_include_directories(${PROJECT_NAME} PRIVATE
            src/include
            ${CMAKE_SOURCE_DIR}/3rdpart/Tools/include
            ${CMAKE_SOURCE_DIR}/src/httpMode
            ${CMAKE_SOURCE_DIR}/3rdpart/jsoncpp/include
            )
    target_link_directories(${PROJECT_NAME} PRIVATE
            ${CURL_LIBRARY_DIRS}
            ${CMAKE_BINARY_DIR}/lib
            )

    target_link_libraries(${PROJECT_NAME} PRIVATE
            jsoncpp
            ${CURL_LIBRARIES}
            )
    target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
ENDIF (CMAKE_SYSTEM_NAME MATCHES "Linux")


set_target_properties(${PROJECT_NAME} #目标名称
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
        )

