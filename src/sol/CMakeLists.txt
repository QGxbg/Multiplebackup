aux_source_directory(. DIR_LIB_SRCS)
add_library (sol ${DIR_LIB_SRCS})
target_link_libraries(sol common ec util protocol pthread)
