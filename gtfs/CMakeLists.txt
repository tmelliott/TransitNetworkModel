file (GLOB SOURCES *.cpp)
add_library (gtfs ${SOURCES})
target_link_libraries (gtfs proto)
