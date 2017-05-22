cmake_minimum_required (VERSION 3.5)
project (TransitNetworkModel)

set (CMAKE_CXX_STANDARD 11)

find_package(Boost REQUIRED COMPONENTS program_options)
include_directories(${Boost_INCLUDE_DIR})

find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})

include_directories(include)

include_directories ("${PROJECT_SOURCE_DIR}/protobuf")
add_subdirectory (protobuf)

include_directories ("${PROJECT_SOURCE_DIR}/gps")
add_subdirectory (gps)

include_directories ("${PROJECT_SOURCE_DIR}/sampling")
add_subdirectory (sampling)

include_directories ("${PROJECT_SOURCE_DIR}/gtfs")
add_subdirectory (gtfs)

add_executable(transit_network_model src/transit_network_model.cpp ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(transit_network_model
	gtfs
	gtfs-realtime
	${PROTOBUF_LIBRARIES}
	sampling
	gps
	Boost::program_options
)

add_executable(load_gtfs src/load_gtfs.cpp)
target_link_libraries(load_gtfs
	gps
)




if (APPLE)
	set(CXXTEST_INCLUDE_DIR /usr/local/bin/cxxtest-4.4)
endif ()
find_package(CxxTest)
if(CXXTEST_FOUND)
	include_directories(${CXXTEST_INCLUDE_DIR})
	enable_testing()

	CXXTEST_ADD_TEST(unittest_gps test_gps.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gps.h)
	target_link_libraries(unittest_gps gps)

	CXXTEST_ADD_TEST(unittest_sampling test_sampling.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sampling.h)
	target_link_libraries(unittest_sampling sampling)

	CXXTEST_ADD_TEST(unittest_gtfs test_gtfs.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gtfs.h)
	target_link_libraries(unittest_gtfs gtfs gtfs-realtime ${PROTOBUF_LIBRARIES} sampling gps)
endif()
