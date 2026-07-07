# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(
	WombCare_Firmwire
	VERSION 1.0
	LANGUAGES C CXX ASM
)

# Include the definition of the slc_WombCare_Firmwire target,
# which contains the content of the SLC project
include(WombCare_Firmwire.cmake)

add_executable(WombCare_Firmwire
    # Add additional sources here
)

target_include_directories(WombCare_Firmwire PUBLIC
    # Add additional include paths here
)

target_compile_definitions(WombCare_Firmwire PUBLIC
    # Add additional macros here
)

target_compile_options(WombCare_Firmwire PUBLIC
    # Set additional compiler flags here
)

target_link_options(WombCare_Firmwire PUBLIC
    # Set additional linker flags here
)

# Link with the content defined in the SLC project
target_link_libraries(WombCare_Firmwire PRIVATE
    slc
)

# Include managed project content if available
include(WombCare_Firmwire_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
    message(STATUS "Using managed project content from ${managed_project}")
endif()

# Force the gcc linker command
set_target_properties(WombCare_Firmwire PROPERTIES LINKER_LANGUAGE C)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(TARGET WombCare_Firmwire
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:WombCare_Firmwire>" "$<TARGET_FILE_DIR:WombCare_Firmwire>/$<TARGET_FILE_BASE_NAME:WombCare_Firmwire>.s37"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:WombCare_Firmwire>" "$<TARGET_FILE_DIR:WombCare_Firmwire>/$<TARGET_FILE_BASE_NAME:WombCare_Firmwire>.hex"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_BIN_CMD}  "$<TARGET_FILE:WombCare_Firmwire>" "$<TARGET_FILE_DIR:WombCare_Firmwire>/$<TARGET_FILE_BASE_NAME:WombCare_Firmwire>.bin" 
)

# Run post-build pipeline to perform additional post-processing
if(post_build_command)
add_custom_command(TARGET WombCare_Firmwire
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
    COMMAND ${post_build_command}
)
endif()