# Find all the *.cpp files within the current source directory, and sort the list
file(GLOB TOOLS_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")
sort_list(TOOLS_SRCS)

# Set all the files to use C++ as well as set their compile flags
set_source_files_properties(${TOOLS_SRCS} PROPERTIES LANGUAGE CXX COMPILE_FLAGS "${CXXFLAGS}")

# Iterate through all the source files
foreach(SRC ${TOOLS_SRCS})
  # Convert the source file extension to have no extension
  string(REGEX REPLACE "\\.cpp$" "" EXE ${SRC})
  # Create skip variable
  set(SKIP)
  # Calculate the header file dependencies for the given source file
  calculate_depends(${SRC} SKIP)
  # Only continue if this file isn't skipped
  if(NOT SKIP)
    # Generate the executable and set its linker flags, also set it to depend on the main Anope executable to be built beforehand
    add_executable(${EXE} ${SRC})
    set_target_properties(${EXE} PROPERTIES LINKER_LANGUAGE CXX LINK_FLAGS "${LDFLAGS}")
    add_dependencies(${EXE} ${PROGRAM_NAME})
    # Only for Windows, set anopesmtp to require the wsock32 library
    if(WIN32 AND ${EXE} STREQUAL anopesmtp)
      target_link_libraries(${EXE} wsock32)
    endif(WIN32 AND ${EXE} STREQUAL anopesmtp)
    # Set the executable to be installed to the bin directory under the main directory
    install(TARGETS ${EXE}
      DESTINATION bin
    )
    # Add the executable to the list of files for CPack to ignore
    get_target_property(EXE_BINARY ${EXE} LOCATION)
    get_filename_component(EXE_BINARY ${EXE_BINARY} NAME)
    add_to_cpack_ignored_files("${EXE_BINARY}$" TRUE)
  endif(NOT SKIP)
endforeach(SRC)

# On non-Windows platforms, if RUNGROUP is set, change the permissions of the tools directory
if(NOT WIN32 AND RUNGROUP)
  install(CODE "execute_process(COMMAND ${CHMOD} 2770 \"\${CMAKE_INSTALL_PREFIX}/bin\")")
endif(NOT WIN32 AND RUNGROUP)
