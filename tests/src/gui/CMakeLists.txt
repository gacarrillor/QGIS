# Standard includes and utils to compile into all tests.
set (util_SRCS)

#####################################################
# Don't forget to include output directory, otherwise
# the UI file won't be wrapped!
include_directories(
  ${CMAKE_SOURCE_DIR}/tests/src/core #for render checker class
  ${CMAKE_SOURCE_DIR}/src/test

  ${CMAKE_CURRENT_BINARY_DIR}
)

if(ENABLE_MODELTEST)
  include_directories(${CMAKE_SOURCE_DIR}/tests/qt_modeltest)
endif()

# a simple app for testing GUI of renderers
# These tests are old and are never run so removed for now.
#set(rendererv2gui_SRCS testrenderergui.cpp)
#set(rendererv2gui_HDRS testrenderergui.h)
#add_executable(qgis_rendererv2gui ${rendererv2gui_SRCS} ${rendererv2gui_MOC_SRCS})

#ADD_QGIS_TEST(histogramtest testqgsrasterhistogram.cpp)

set(TESTS
  testqgsdockwidget.cpp
)

foreach(TESTSRC ${TESTS})
  add_qgis_test( ${TESTSRC} MODULE gui LINKEDLIBRARIES qgis_gui qgis_analysis)
endforeach(TESTSRC)

add_qgis_test(testqgsrasterlayersaveasdialog.cpp MODULE gui LINKEDLIBRARIES qgis_gui qgis_analysis)
add_qgis_test(testqgslistwidget.cpp MODULE gui LINKEDLIBRARIES qgis_gui qgis_analysis)

