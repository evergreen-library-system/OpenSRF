#include <check.h>
#include "opensrf/osrf_json.h"

jsonObject *jsonObj;
jsonObject *jsonHash;
jsonObject *jsonNumber;
jsonObject *jsonBool;
jsonObject *jsonArray;

//Set up the test fixture
void setup (void) {
  jsonObj = jsonNewObject("test");
  jsonHash = jsonNewObject(NULL);
  jsonNumber = jsonNewNumberObject(123.456);
  jsonBool = jsonNewBoolObject(0);
  jsonArray = jsonNewObjectType(JSON_ARRAY);
}

//Clean up the test fixture
void teardown (void) {
  jsonObjectFree(jsonObj);
  jsonObjectFree(jsonHash);
  jsonObjectFree(jsonNumber);
  jsonObjectFree(jsonBool);
  jsonObjectFree(jsonArray);
}

//Tests

START_TEST(test_osrf_json_object_jsonNewObject)
  fail_if(jsonObj == NULL, "jsonObject not created");
END_TEST

START_TEST(test_osrf_json_object_jsonNewObjectFmt)
  jsonObject *fmtObj;
  jsonObject *nullObj;
  fmtObj = jsonNewObjectFmt("string %d %d", 10, 20);
  nullObj = jsonNewObjectFmt(NULL);

  fail_if(fmtObj == NULL, "jsonObject not created");
  fail_unless(strcmp(fmtObj->value.s, "string 10 20") == 0,
      "jsonObject->value.s should contain the formatted string passed to jsonNewObjectFmt()");
  fail_unless(fmtObj->type == JSON_STRING,
      "jsonNewObjectFmt should set the jsonObject->type to JSON_STRING");
  fail_unless(nullObj->value.s == NULL,
      "jsonNewObjectFmt should set jsonObject->value.s to NULL if passed a NULL arg");
  fail_unless(nullObj->type == JSON_NULL,
      "jsonNewObjectFmt should set jsonObject->type to JSON_NULL if passed a NULL arg");
END_TEST

START_TEST(test_osrf_json_object_jsonNewNumberObject)
  jsonObject *numObj;
  numObj = jsonNewNumberObject(123);

  fail_if(numObj == NULL, "jsonObject not created");
  fail_unless(strcmp(numObj->value.s, "123") == 0,
      "jsonNewNumberObject should set jsonObject->value.s to the string value of the num arg");
  fail_unless(numObj->type == JSON_NUMBER,
      "jsonNewNumberObject should set jsonObject->type to JSON_NUMBER");
END_TEST

START_TEST(test_osrf_json_object_jsonNewNumberStringObject)
  jsonObject *nullobj = jsonNewNumberStringObject(NULL);
  fail_unless(strcmp(nullobj->value.s, "0") == 0,
      "jsonNewNumberStringObject should return a jsonObject with a value of 0 if passed a NULL numstr arg");
  fail_unless(nullobj->type == JSON_NUMBER,
      "jsonNewNumberStringObject should return a jsonObject with type JSON_NUMBER");
  jsonObject *notnumobj = jsonNewNumberStringObject("not a number");
  fail_unless(notnumobj == NULL,
      "jsonNewNumberStringObject should return NULL if passed an arg that is not a number string");
  jsonObject *numstrobj = jsonNewNumberStringObject("123");
  fail_unless(strcmp(numstrobj->value.s, "123") == 0,
      "jsonNewNumberStringObject should return a jsonObject with value.s = the value of the numstr arg");
  fail_unless(numstrobj->type == JSON_NUMBER,
      "jsonNewNumberStringObject should return a jsonObject of type JSON_NUMBER");
END_TEST

START_TEST(test_osrf_json_object_jsonNewBoolObject)
  fail_unless(jsonBool->type == JSON_BOOL,
      "jsonNewBoolObject should return a jsonObject of type JSON_BOOL");
  fail_unless(jsonBool->value.b == 0,
      "jsonNewBoolObject should return an object with a value of the val arg");
END_TEST

START_TEST(test_osrf_json_object_jsonSetBool)
  jsonSetBool(jsonBool, -1);
  fail_unless(jsonBool->value.b == -1,
      "jsonSetBool should set jsonObject->value.b to the value of the val arg");
END_TEST

START_TEST(test_osrf_json_object_jsonObjectSetKey)
  fail_unless(jsonObjectSetKey(NULL, "key1", NULL) == -1);
  fail_unless(jsonObjectSetKey(jsonHash, "key1", NULL) == 1);
  fail_unless(jsonObjectSetKey(jsonHash, "key2", jsonNewObject("test2")) == 2);
  fail_unless(jsonObjectGetKey(jsonHash, "key1")->value.s == NULL);
  fail_unless(strcmp(jsonObjectGetKey(jsonHash, "key2")->value.s, "test2") == 0);
END_TEST

START_TEST(test_osrf_json_object_jsonObjectRemoveKey)
  jsonObjectSetKey(jsonHash, "key1", jsonNewObject("value"));
  fail_unless(jsonObjectRemoveKey(jsonHash, NULL) == -1);
  fail_unless(jsonObjectRemoveKey(jsonHash, "key1") == 1);
END_TEST

START_TEST(test_osrf_json_object_jsonObjectGetKey)
  jsonObjectSetKey(jsonHash, "key1", jsonNewObject("value"));
  fail_unless(strcmp(jsonObjectGetKey(jsonHash, "key1")->value.s, "value") == 0);
END_TEST

START_TEST(test_osrf_json_object_jsonObjectSetClass)
  jsonObjectSetClass(jsonObj, NULL);
  fail_unless(jsonObj->classname == NULL);
  jsonObjectSetClass(jsonObj, "aClass");
  fail_unless(strcmp(jsonObj->classname, "aClass") == 0);
END_TEST

START_TEST(test_osrf_json_object_jsonObjectGetClass)
  fail_unless(jsonObjectGetClass(NULL) == NULL);
  jsonObjectSetClass(jsonObj, "aClass");
  fail_unless(strcmp(jsonObjectGetClass(jsonObj), "aClass") == 0);
END_TEST

START_TEST(test_osrf_json_object_jsonObjectSetIndex)
  jsonObject *jsonArrayValue = jsonNewObject("value");
  fail_unless(jsonObjectSetIndex(NULL, 0, jsonArrayValue) == -1,
      "jsonObjectSetIndex should return -1 if dest arg is NULL");
  fail_unless(jsonObjectSetIndex(jsonArray, 0, NULL) == 1,
      "jsonObjectSetIndex should return the size of the json array after setting the new index");
  fail_unless(jsonObjectSetIndex(jsonArray, 1, jsonArrayValue) == 2,
      "jsonObjectSetIndex should return the size of the json array after setting the new index");
  jsonObject *jsonArrayResult = jsonObjectGetIndex(jsonArray, 1);
  fail_unless(strcmp(jsonArrayResult->value.s, "value") == 0,
      "the value inserted into the jsonArray should be the value of the newObj arg");
  fail_unless(jsonArrayResult->parent == jsonArray,
      "the parent of the element inserted should be equal to the newObj arg");
END_TEST

START_TEST(test_osrf_json_object_jsonObjectGetIndex)
  jsonObject *jsonArrayValue = jsonNewObject("value");
  jsonObjectSetIndex(jsonArray, 0, jsonArrayValue);
  fail_unless(jsonObjectGetIndex(NULL, 0) == NULL,
      "if no obj arg is passed to jsonObjectGetIndex, it should return NULL");
  fail_unless(jsonObjectGetIndex(jsonArray, 2) == NULL,
      "if the index in the jsonArray is NULL, jsonObjectGetIndex should return NULL");
  fail_unless(jsonObjectGetIndex(jsonNumber, 0) == NULL,
      "if the obj arg isn't of type JSON_ARRAY, return NULL");
  jsonObject *getIndexValue = jsonObjectGetIndex(jsonArray, 0);
  fail_unless(strcmp(getIndexValue->value.s, "value") == 0,
      "jsonObjectGetIndex should return the jsonObject at the index given");
END_TEST

START_TEST(test_osrf_json_object_jsonObjectToJSONRaw)
  fail_unless(jsonObjectToJSONRaw(NULL) == NULL,
      "when passed NULL, jsonObjectToJSONRaw should return NULL");

  jsonObject *val1 = jsonNewObject("value1");
  jsonObject *val2 = jsonNewObject("value2");
  jsonObjectSetClass(val1, "class1");
  jsonObjectSetClass(val2, "class2");
  jsonObjectSetKey(jsonHash, "key1", val1);
  jsonObjectSetKey(jsonHash, "key2", val2);

  fail_unless(strcmp(jsonObjectToJSONRaw(jsonHash),
      "{\"key1\":\"value1\",\"key2\":\"value2\"}") == 0,
      "jsonObjectToJSONRaw should return a string of raw JSON, without expanding\
      class names, built from the obj arg");
END_TEST

START_TEST(test_osrf_json_object_jsonObjectToJSON)
  fail_unless(jsonObjectToJSON(NULL) == NULL,
      "jsonObjectToJSON should return NULL if passed a NULL obj arg");
  jsonObject *val1 = jsonNewObject("value1");
  jsonObject *val2 = jsonNewObject("value2");
  jsonObjectSetClass(val1, "class1");
  jsonObjectSetClass(val2, "class2");

  jsonObjectSetKey(jsonHash, "key1", val1);
  jsonObjectSetKey(jsonHash, "key2", val2);
  fail_unless(strcmp(jsonObjectToJSON(jsonHash),
      "{\"key1\":{\"__c\":\"class1\",\"__p\":\"value1\"},\"key2\":{\"__c\":\"class2\",\"__p\":\"value2\"}}") == 0,
      "jsonObjectToJSON should return the obj arg as raw json, expanding class names");
END_TEST

START_TEST(test_osrf_json_object_doubleToString)
  fail_unless(strcmp(doubleToString(123.456),
      "123.456000000000003069544618484") == 0,
      "doubleToString should return a string version of the given double, with a precision of 30 digits");
END_TEST

START_TEST(test_osrf_json_object_jsonObjectGetString)
  fail_unless(strcmp(jsonObjectGetString(jsonObj), "test") == 0,
      "jsonObjectGetString should return the value of the given object, if it is of type JSON_STRING");
  fail_unless(strcmp(jsonObjectGetString(jsonNumber),
      "123.456000000000003069544618484") == 0,
      "jsonObjectGetString should return the value of the given JSON_NUMBER object if it is not NULL");
  jsonObject *jsonNullNumber = jsonNewNumberObject(0);
  jsonObjectSetNumberString(jsonNullNumber, "NaN"); //set jsonNullNumber->value to NULL
  fail_unless(strcmp(jsonObjectGetString(jsonNullNumber), "0") == 0,
      "jsonObjectGetString should return 0 if value of the given JSON_NUMBER object is NULL");
  fail_unless(jsonObjectGetString(jsonHash) == NULL,
      "jsonObjectGetString should return NULL if the given arg is not of type JSON_NUMBER or JSON_STRING");
  fail_unless(jsonObjectGetString(NULL) == NULL,
      "jsonObjectGetString should return NULL if the given arg is NULL");
END_TEST

START_TEST(test_osrf_json_object_jsonObjectGetNumber)
  fail_unless(jsonObjectGetNumber(NULL) == 0,
      "jsonObjectGetNumber should return 0 if given arg is NULL");
  fail_unless(jsonObjectGetNumber(jsonHash) == 0,
      "jsonObjectGetNumber should return 0 if given arg is not of type JSON_NUMBER");
  jsonObject *jsonNullNumber = jsonNewNumberObject(0);
  jsonObjectSetNumberString(jsonNullNumber, "NaN");
  fail_unless(jsonObjectGetNumber(jsonNullNumber) == 0,
      "jsonObjectGetNumber should return 0 if given args value is NULL");
  fail_unless(jsonObjectGetNumber(jsonNumber) == 123.456000000000003069544618484,
      "jsonObjectGetNumber should return the value of the given obj in double form");
END_TEST

START_TEST(test_osrf_json_object_jsonObjectSetString)
  jsonObjectSetString(jsonObj, NULL);
  fail_unless(strcmp(jsonObj->value.s, "test") == 0,
      "jsonObjectSetString should not change the value of the dest arg if passed a NULL string arg");
  jsonObjectSetString(jsonObj, "changed");
  fail_unless(strcmp(jsonObj->value.s, "changed") == 0,
      "jsonObjectSetString should change the value of the dest arg to the value of the string arg");
END_TEST

START_TEST(test_osrf_json_object_jsonObjectSetNumberString)
  fail_unless(jsonObjectSetNumberString(NULL, "asdf") == -1,
      "jsonObjectSetNumberString should return -1 when dest arg is NULL");
  fail_unless(jsonObjectSetNumberString(jsonNumber, NULL) == -1,
      "jsonObjectSetNumberString should return -1 when string arg is NULL");
  fail_unless(jsonObjectSetNumberString(jsonNumber, "111.111") == 0,
      "jsonObjectSetNumberString should return 0 upon success");
  fail_unless(strcmp(jsonNumber->value.s, "111.111") == 0,
      "jsonObjectSetNumberString should set the value of the dest arg to the value of the string arg");
  fail_unless(jsonObjectSetNumberString(jsonNumber, "not a number") == -1,
      "jsonObjectSetNumber should return -1 if the string arg is not numeric");
  fail_unless(jsonNumber->value.s == NULL,
      "when the string arg is not numeric, dest->value.s should be set to NULL");
END_TEST

START_TEST(test_osrf_json_object_jsonObjectSetNumber)
  jsonObjectSetNumber(jsonNumber, 999.999);
  fail_unless(strcmp(jsonNumber->value.s, "999.999000000000023646862246096") == 0,
      "jsonObjectSetNumber should set dest->value.s to the stringified version of the num arg");
END_TEST

START_TEST(test_osrf_json_object_jsonObjectClone)
  jsonObject *nullClone = jsonObjectClone(NULL);
  fail_unless(nullClone->type == JSON_NULL && nullClone->value.s == NULL,
      "when passed a NULL arg, jsonObjectClone should return a jsonObject of type JSON_NULL with a value of NULL ");

  jsonObject *anotherNullClone = jsonObjectClone(nullClone);
  fail_unless(anotherNullClone->type == JSON_NULL && anotherNullClone->value.s == NULL,
      "jsonObjectClone should return a clone of an object with type JSON_NULL");

  jsonObject *stringClone = jsonObjectClone(jsonObj);
  fail_unless(stringClone->type == JSON_STRING && strcmp(stringClone->value.s, "test") == 0,
      "jsonObjectClone should return a clone of an object with type JSON_STRING");

  jsonObject *numberClone = jsonObjectClone(jsonNumber);
  fail_unless(numberClone->type == JSON_NUMBER,
      "jsonObjectClone should return a clone of a JSON_NUMBER object");
  fail_unless(strcmp(numberClone->value.s, "123.456000000000003069544618484") == 0,
      "jsonObjectClone should return a clone of a JSON_NUMBER object");

  jsonObject *boolClone = jsonObjectClone(jsonBool);
  fail_unless(boolClone->type == JSON_BOOL && boolClone->value.b == 0,
      "jsonObjectClone should return a clone of a JSON_BOOL object");

  //Array
  jsonObject *arrayVal1 = jsonNewObject("arrayval1");
  jsonObject *arrayVal2 = jsonNewObject("arrayval2");
  jsonObjectSetIndex(jsonArray, 0, arrayVal1);
  jsonObjectSetIndex(jsonArray, 0, arrayVal2);
  jsonObject *arrayClone = jsonObjectClone(jsonArray);
  fail_unless(strcmp(jsonObjectToJSON(arrayClone), jsonObjectToJSON(jsonArray)) == 0,
      "jsonObjectClone should return a clone of a JSON_ARRAY object");

  //Hash
  jsonObject *val1 = jsonNewObject("value1");
  jsonObject *val2 = jsonNewObject("value2");
  jsonObjectSetClass(val1, "class1");
  jsonObjectSetClass(val2, "class2");
  jsonObjectSetKey(jsonHash, "key1", val1);
  jsonObjectSetKey(jsonHash, "key2", val2);
  jsonObject *hashClone = jsonObjectClone(jsonHash);
  fail_unless(strcmp(jsonObjectToJSON(hashClone), jsonObjectToJSON(jsonHash)) == 0,
      "jsonObjectClone should return a clone of a JSON_HASH object");
END_TEST

START_TEST(test_osrf_json_object_jsonBoolIsTrue)
  fail_unless(jsonBoolIsTrue(NULL) == 0,
      "jsonBoolIsTrue should return 0 if a NULL arg is passed");
  fail_unless(jsonBoolIsTrue(jsonObj) == 0,
      "jsonBoolIsTrue should return 0 if a non JSON_BOOL arg is passed");
  fail_unless(jsonBoolIsTrue(jsonBool) == 0,
      "jsonBoolIsTrue should return 0 if the value of boolObj is 0");
  jsonObject *newBool = jsonNewBoolObject(123);
  fail_unless(jsonBoolIsTrue(newBool) == 1,
      "jsonBoolIsTrue should return 1 if the value of boolObj is not 0");
END_TEST

//END Tests


Suite *osrf_json_object_suite (void) {
  //Create test suite, test case, initialize fixture
  Suite *s = suite_create("osrf_json_object");
  TCase *tc_core = tcase_create("Core");
  tcase_add_checked_fixture(tc_core, setup, teardown);

  //Add tests to test case
  tcase_add_test(tc_core, test_osrf_json_object_jsonNewObject);
  tcase_add_test(tc_core, test_osrf_json_object_jsonNewObjectFmt);
  tcase_add_test(tc_core, test_osrf_json_object_jsonNewBoolObject);
  tcase_add_test(tc_core, test_osrf_json_object_jsonSetBool);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectToJSONRaw);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectToJSON);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectSetKey);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectGetKey);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectSetClass);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectGetClass);
  tcase_add_test(tc_core, test_osrf_json_object_jsonNewNumberObject);
  tcase_add_test(tc_core, test_osrf_json_object_jsonNewNumberStringObject);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectRemoveKey);
  tcase_add_test(tc_core, test_osrf_json_object_doubleToString);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectGetString);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectGetNumber);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectSetString);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectSetNumberString);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectSetNumber);
  tcase_add_test(tc_core, test_osrf_json_object_jsonBoolIsTrue);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectSetIndex);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectGetIndex);
  tcase_add_test(tc_core, test_osrf_json_object_jsonObjectClone);

  //Add test case to test suite
  suite_add_tcase(s, tc_core);

  return s;
}

void run_tests (SRunner *sr) {
  srunner_add_suite (sr, osrf_json_object_suite());
}
