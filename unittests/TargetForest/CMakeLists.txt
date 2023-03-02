add_klee_unit_test(TargetForestTest
  TargetForestTest.cpp)
target_link_libraries(TargetForestTest PRIVATE kleeCore)
target_include_directories(TargetForestTest BEFORE PUBLIC "../../lib")
