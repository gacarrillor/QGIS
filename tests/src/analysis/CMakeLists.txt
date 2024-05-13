# Standard includes and utils to compile into all tests.
set (util_SRCS)


#####################################################
# Don't forget to include output directory, otherwise
# the UI file won't be wrapped!
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/src/test
)

if(HAVE_OPENCL)
    include_directories(SYSTEM ${OpenCL_INCLUDE_DIRS})
endif()

#############################################################
# Tests:
set(TESTS
 testqgsgeometrysnapper.cpp
)
if(HAVE_GSL)
  set(TESTS ${TESTS} testqgsgcptransformer.cpp)
endif()

if (WITH_PDAL AND PDAL_2_5_OR_HIGHER)
  set(TESTS ${TESTS} testqgsprocessingpdalalgs.cpp)
endif()

foreach(TESTSRC ${TESTS})
    add_qgis_test(${TESTSRC} MODULE analysis LINKEDLIBRARIES qgis_analysis)
endforeach(TESTSRC)
add_qgis_test(testqgsprocessing.cpp MODULE analysis LINKEDLIBRARIES qgis_analysis LABELS "POSTGRES")
