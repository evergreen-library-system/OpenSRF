#include <check.h>
#include "opensrf/osrf_stack.h"



//Set up the test fixture
void setup(void){
}

//Clean up the test fixture
void teardown(void){
}

// BEGIN TESTS

START_TEST(test_osrf_stack_process)
  int *mr = 0;
  fail_unless(osrf_stack_process(NULL, 10, mr) == -1,
      "osrf_stack_process should return -1 if client arg is NULL");
END_TEST

//END TESTS

Suite *osrf_stack_suite(void) {
  //Create test suite, test case, initialize fixture
  Suite *s = suite_create("osrf_stack");
  TCase *tc_core = tcase_create("Core");
  tcase_add_checked_fixture(tc_core, setup, teardown);

  //Add tests to test case
  tcase_add_test(tc_core, test_osrf_stack_process);

  //Add test case to test suite
  suite_add_tcase(s, tc_core);

  return s;
}

void run_tests(SRunner *sr) {
  srunner_add_suite(sr, osrf_stack_suite());
}
