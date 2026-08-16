# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(
	Wombcare_PreFinal
	VERSION 1.0
	LANGUAGES C CXX ASM
)

# Include the definition of the slc_Wombcare_PreFinal target,
# which contains the content of the SLC project
include(Wombcare_PreFinal.cmake)

# MVP-compiled kernels in autogen/ml/accelerator/mvpv1/ require IEEE FP16 on EFR32.
target_compile_options(slc PUBLIC
    $<$<COMPILE_LANGUAGE:C>:-mfp16-format=ieee>
    $<$<COMPILE_LANGUAGE:CXX>:-mfp16-format=ieee>
)

# CMSIS-DSP include path for wombcare_dsp.c
target_include_directories(slc PUBLIC
    "${CMAKE_SOURCE_DIR}/../util/third_party/cmsis/dsp/Include"
    "${CMAKE_SOURCE_DIR}/../util/third_party/cmsis/CMSIS/Core/Include"
)

add_executable(Wombcare_PreFinal
    # Add additional sources here
)

target_include_directories(Wombcare_PreFinal PUBLIC
    # Add additional include paths here
)

target_compile_definitions(Wombcare_PreFinal PUBLIC
    # Add additional macros here
)

target_compile_options(Wombcare_PreFinal PUBLIC
    # Set additional compiler flags here
)

target_link_options(Wombcare_PreFinal PUBLIC
    # Set additional linker flags here
)

# Link with the content defined in the SLC project
target_link_libraries(Wombcare_PreFinal PRIVATE
    slc
)

# Include managed project content if available
include(Wombcare_PreFinal_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
    message(STATUS "Using managed project content from ${managed_project}")
endif()

# Force the gcc linker command
set_target_properties(Wombcare_PreFinal PROPERTIES LINKER_LANGUAGE C)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(TARGET Wombcare_PreFinal
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:Wombcare_PreFinal>" "$<TARGET_FILE_DIR:Wombcare_PreFinal>/$<TARGET_FILE_BASE_NAME:Wombcare_PreFinal>.s37"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:Wombcare_PreFinal>" "$<TARGET_FILE_DIR:Wombcare_PreFinal>/$<TARGET_FILE_BASE_NAME:Wombcare_PreFinal>.hex"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_BIN_CMD}  "$<TARGET_FILE:Wombcare_PreFinal>" "$<TARGET_FILE_DIR:Wombcare_PreFinal>/$<TARGET_FILE_BASE_NAME:Wombcare_PreFinal>.bin" 
)

# Run post-build pipeline to perform additional post-processing
if(post_build_command)
add_custom_command(TARGET Wombcare_PreFinal
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
    COMMAND ${post_build_command}
)
endif()