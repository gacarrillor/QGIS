#####################################################
# Don't forget to include output directory, otherwise
# the UI file won't be wrapped!
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/external/kdbush/include
  ${CMAKE_SOURCE_DIR}/src/test
)

if(HAVE_OPENCL)
    include_directories(SYSTEM ${OpenCL_INCLUDE_DIRS})
endif()

#############################################################
# Tests:

set(TESTS
 testqgsrange.cpp
)

if(WITH_QTWEBKIT)
  set(TESTS ${TESTS}
    testqgslayouthtml.cpp
  )
endif()


#foreach(TESTSRC ${TESTS})
#    add_qgis_test( ${TESTSRC} MODULE core LINKEDLIBRARIES qgis_core)
#endforeach(TESTSRC)

add_qgis_test(testqgscoordinatereferencesystem.cpp MODULE core LINKEDLIBRARIES qgis_core DEPENDENCIES synccrsdb )

#if (ENABLE_PGTEST)
#  SET_TESTS_PROPERTIES(test_core_vectorlayerjoinbuffer PROPERTIES LABELS "POSTGRES")
#endif()

#add_subdirectory(geometry)
#add_subdirectory(vector)
