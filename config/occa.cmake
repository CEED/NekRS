set(OCCA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/occa")

# If OCCA is not in source tree, download it in build directory
if (EXISTS ${OCCA_SOURCE_DIR}) 
  message(STATUS "Using OCCA source in ${OCCA_SOURCE_DIR}")
else()
  FetchContent_Declare(
    occa_content
    GIT_REPOSITORY https://github.com/Nek5000/occa.git
    GIT_TAG ${OCCA_TAG})
  FetchContent_GetProperties(occa_content)
  if(NOT occa_content_POPULATED)
    FetchContent_Populate(occa_content)
  endif()
  set(OCCA_SOURCE_DIR ${occa_content_SOURCE_DIR})
endif()

add_subdirectory(${OCCA_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${occa_content_BINARY_DIR})
target_compile_options(libocca PRIVATE "SHELL:${OCCA_CXXFLAGS}")
target_compile_options(occa PRIVATE "SHELL:${OCCA_CXXFLAGS}")

