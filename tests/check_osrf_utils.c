#include <check.h>
#include "opensrf/utils.h"



//Set up the test fixture
void setup(void){
}

//Clean up the test fixture
void teardown(void){
}

// BEGIN TESTS

START_TEST(test_osrfXmlEscapingLength)
  const char* ordinary = "12345";
  fail_unless(osrfXmlEscapingLength(ordinary) == 0,
      "osrfXmlEscapingLength should return 0 if string has no special characters");
  const char* special = "<tag attr=\"attribute value\">a &amp; b</tag>";
  ck_assert_int_eq(osrfXmlEscapingLength(special), 38);
END_TEST

//END TESTS

Suite *osrf_utils_suite(void) {
  //Create test suite, test case, initialize fixture
  Suite *s = suite_create("osrf_utils");
  TCase *tc_core = tcase_create("Core");
  tcase_add_checked_fixture(tc_core, setup, teardown);

  //Add tests to test case
  tcase_add_test(tc_core, test_osrfXmlEscapingLength);

  //Add test case to test suite
  suite_add_tcase(s, tc_core);

  return s;
}

void run_tests(SRunner *sr) {
  srunner_add_suite(sr, osrf_utils_suite());
}
