cmake_minimum_required (VERSION 3.5)
project (TransitNetworkModel)

set (CMAKE_CXX_STANDARD 11)

find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})

include_directories(include)

file (GLOB SOURCES src/*.cpp)

include_directories ("${PROJECT_SOURCE_DIR}/protobuf")
add_subdirectory (protobuf)

include_directories ("${PROJECT_SOURCE_DIR}/gps")
add_subdirectory (gps)

add_executable(transit_network_model ${SOURCES}  ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(transit_network_model gtfs-realtime)
target_link_libraries(transit_network_model ${PROTOBUF_LIBRARIES})
target_link_libraries(transit_network_model gps)
