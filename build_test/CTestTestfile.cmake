# CMake generated Testfile for 
# Source directory: C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp
# Build directory: C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp/build_test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(t9_tests "C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp/build_test/Debug/t9_tests.exe")
  set_tests_properties(t9_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp/CMakeLists.txt;87;add_test;C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(t9_tests "C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp/build_test/Release/t9_tests.exe")
  set_tests_properties(t9_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp/CMakeLists.txt;87;add_test;C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(t9_tests "C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp/build_test/MinSizeRel/t9_tests.exe")
  set_tests_properties(t9_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp/CMakeLists.txt;87;add_test;C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(t9_tests "C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp/build_test/RelWithDebInfo/t9_tests.exe")
  set_tests_properties(t9_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp/CMakeLists.txt;87;add_test;C:/Users/cools/Documents/GitHub/T9GamepadIME-cpp/CMakeLists.txt;0;")
else()
  add_test(t9_tests NOT_AVAILABLE)
endif()
