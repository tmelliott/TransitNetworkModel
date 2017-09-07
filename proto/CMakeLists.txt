file (GLOB ProtoFiles "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")
PROTOBUF_GENERATE_CPP (ProtoSources ProtoHeaders ${ProtoFiles})
add_library (proto STATIC ${ProtoSources} ${ProtoHeaders})
target_link_libraries (proto ${Boost_LIBRARIES} ${PROTOBUF_LIBRARY})

target_include_directories (proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
