#include <check.h>
#include "opensrf/osrf_list.h"

osrfList *testOsrfList;
int globalItem1 = 7;
int globalItem3 = 15;

//Keep track of how many items have been freed using osrfCustomListFree
unsigned int freedItemsSize;

//Define a custom freeing function for list items
void osrfCustomListFree() {
  freedItemsSize++;
}

//Set up the test fixture
void setup(void) {
  freedItemsSize = 0;
  //Set up a list of size 10, define the freeing function, add some items to test with
  testOsrfList = osrfNewListSize(10);
  testOsrfList->freeItem = (void(*)(void*)) osrfCustomListFree;

  osrfListPush(testOsrfList, &globalItem1);
  osrfListPush(testOsrfList, NULL);
  osrfListPush(testOsrfList, &globalItem3);
}

//Clean up the test fixture
void teardown(void) {
  osrfListFree(testOsrfList);
}

// BEGIN TESTS

START_TEST(test_osrf_list_osrfNewList)
  osrfList *newList = osrfNewList();
  fail_if(newList == NULL, "osrfList object not successfully created");
  fail_unless(newList->arrsize == 48, "the osrfList is not the default size of 48");
END_TEST

START_TEST(test_osrf_list_osrfNewListSize)
  osrfList *smallList = osrfNewListSize(5);
  fail_if(smallList == NULL, "smallList not successfully created");
  fail_unless(smallList->arrsize == 5, "smallList wasn't created with the size 5");
  fail_unless(smallList->freeItem == NULL, "freeItem should be null by default");
  int i;
  for (i = 0 ; i < smallList->arrsize ; i++) {
    fail_if(smallList->arrlist[i] != NULL, "Every value in smallList->arrlist should be null");
  }

  //List created with size <= 0
  osrfList *sizelessList = osrfNewListSize(0);
  fail_unless(sizelessList->arrsize == 16,
      "osrfNewListSize called with a size of 0 or less should have an array size of 16");
END_TEST

START_TEST(test_osrf_list_osrfListPush)
  fail_unless(osrfListPush(NULL, NULL) == -1,
      "Passing a null list to osrfListPush should return -1");
  int listItem = 111;
  fail_unless(osrfListPush(testOsrfList, &listItem) == 0,
      "osrfListPush should return 0 if successful");
  fail_unless(testOsrfList->size == 4,
      "testOsrfList->size did not update correctly, should be 4");
  fail_unless(osrfListGetIndex(testOsrfList, 3) == &listItem,
      "listItem did not add to the end of testOsrfList");
END_TEST

START_TEST(test_osrf_list_osrfListPushFirst)
  fail_unless(osrfListPushFirst(NULL, NULL) == -1,
      "Passing a null list to osrfListPushFirst should return -1");
  int listItem = 123;
  fail_unless(osrfListPushFirst(testOsrfList, &listItem) == 3,
      "osrfListPushFirst should return a size of 3");
  fail_unless(osrfListGetIndex(testOsrfList, 1) == &listItem,
      "listItem should be in index 1 because it is the first that is null");
END_TEST

START_TEST(test_osrf_list_osrfListSet)
  //Null argument check
  fail_unless(osrfListSet(NULL, NULL, 1) == NULL,
      "Given a null list arg, osrfListSet should return null");

  //Adding an item to an existing, NULL position in the list
  int listItem = 456;
  fail_unless(osrfListSet(testOsrfList, &listItem, 4) == NULL,
      "Calling osrfListSet on an empty index should return NULL");
  fail_unless(osrfListGetIndex(testOsrfList, 4) == &listItem,
      "osrfListSet is not assigning item pointer to the correct position");
  fail_unless(testOsrfList->size == 5,
      "osrfListSet should update a lists size after adding an item to that list");

  //Adding an item to an exisiting, occupied position in the
  //list when there is a freeing function defined on the list
  int listItem2 = 789;
  fail_unless(osrfListSet(testOsrfList, &listItem2, 4) == NULL,
      "Calling osrfListSet on an index that held a value, \
       on a list that has a custom freeing function, should return NULL");
  fail_unless(osrfListGetIndex(testOsrfList, 4) == &listItem2,
      "When called on a position that already has a value, \
       osrfListSet should replace that value with the new item");
  fail_unless(testOsrfList->size == 5,
      "osrfListSet shouldn't update a lists size if the item is \
       not added beyond the current size");

  //Adding an item to an exisiting, occupied position in the list
  //when there is NOT a freeing function defined on the list
  testOsrfList->freeItem = NULL;
  int listItem3 = 111;
  fail_unless(osrfListSet(testOsrfList, &listItem3, 4) == &listItem2,
      "Calling osrfListSet on an index that held a value should \
       return the reference to that value");
  fail_unless(osrfListGetIndex(testOsrfList, 4) == &listItem3,
      "When called on a position that already has a value, \
       osrfListSet should replace that value with the new item");
  fail_unless(testOsrfList->size == 5,
      "osrfListSet shouldn't update a lists size if the item is \
       not added beyond the current size");

  //Adding an item to a position outside of the current array size
  int listItem4 = 444;
  fail_unless(osrfListSet(testOsrfList, &listItem4, 18) == NULL,
      "Calling osrfListSet on an empty index should return NULL, \
       even if the index does not exist yet");
  fail_unless(testOsrfList->arrsize == 266,
      "New arrsize should be 266 since it was 10 before, and grows \
       in increments of 256 when expanded");
  fail_unless(testOsrfList->size == 19,
      "List should have a size value of 19");
  fail_unless(osrfListGetIndex(testOsrfList, 18) == &listItem4,
      "Value not added to correct index of list");
END_TEST

START_TEST(test_osrf_list_osrfListGetIndex)
  fail_unless(osrfListGetIndex(NULL, 1) == NULL,
      "Calling osrfListGetIndex with a null list should return null");
  fail_unless(osrfListGetIndex(testOsrfList, 8) == NULL,
      "Calling osrfListGetIndex with a value outside the range of \
       occupied indexes should return NULL");
  fail_unless(osrfListGetIndex(testOsrfList, 2) == &globalItem3,
      "osrfListGetIndex should return the value of the list at the given index");
END_TEST

START_TEST(test_osrf_list_osrfListFree)
  //Set up a new list to be freed
  osrfList *myList = osrfNewList();
  myList->freeItem = (void(*)(void*)) osrfCustomListFree;
  int* myListItem1 = malloc(sizeof(int));
  *myListItem1 = 123;
  int* myListItem2 = malloc(sizeof(int));
  *myListItem2 = 456;
  osrfListSet(myList, myListItem1, 0);
  osrfListSet(myList, myListItem2, 1);
  osrfListFree(myList);
  fail_unless(freedItemsSize == 2,
      "osrfListFree should free each item in the list if there is a custom \
      freeing function defined");
END_TEST

START_TEST(test_osrf_list_osrfListClear)
  //Set up a new list with items to be freed
  osrfList *myList = osrfNewList();
  myList->freeItem = (void(*)(void*)) osrfCustomListFree;
  int* myListItem1 = malloc(sizeof(int));
  *myListItem1 = 123;
  int* myListItem2 = malloc(sizeof(int));
  *myListItem2 = 456;
  osrfListSet(myList, myListItem1, 0);
  osrfListSet(myList, myListItem2, 1);
  osrfListClear(myList);

  fail_unless(freedItemsSize == 2,
      "osrfListClear should free each item in the list if there is a custom \
       freeing function defined");
  fail_unless(myList->arrlist[0] == NULL && myList->arrlist[1] == NULL,
      "osrfListClear should make all previously used slots in the list NULL");
  fail_unless(myList->size == 0,
      "osrfListClear should set the list's size to 0");
END_TEST

START_TEST(test_osrf_list_osrfListSwap)
  //Prepare a second list to swap
  osrfList *secondOsrfList = osrfNewListSize(7);
  int* secondListItem2 = malloc(sizeof(int));
  *secondListItem2 = 8;
  int* secondListItem3 = malloc(sizeof(int));
  *secondListItem3 = 16;
  osrfListPush(secondOsrfList, NULL);
  osrfListPush(secondOsrfList, secondListItem2);
  osrfListPush(secondOsrfList, secondListItem3);

  osrfListSwap(testOsrfList, secondOsrfList);
  fail_unless(
    osrfListGetIndex(testOsrfList, 0) == NULL &&
    osrfListGetIndex(testOsrfList, 1) == secondListItem2 &&
    osrfListGetIndex(testOsrfList, 2) == secondListItem3,
    "After osrfListSwap, first list should now contain \
    the contents of the second list"
  );
  fail_unless(
    osrfListGetIndex(secondOsrfList, 0) == &globalItem1 &&
    osrfListGetIndex(secondOsrfList, 1) == NULL &&
    osrfListGetIndex(secondOsrfList, 2) == &globalItem3,
    "After osrfListSwap, second list should now contain \
    the contents of the first list"
  );
END_TEST

START_TEST(test_osrf_list_osrfListRemove)
  fail_unless(osrfListRemove(NULL, 2) == NULL,
      "osrfListRemove should return NULL when not given a list");
  fail_unless(osrfListRemove(testOsrfList, 1000) == NULL,
      "osrfListRemove should return NULL when given a position \
       exceeding the size of the list");
  fail_unless(osrfListRemove(testOsrfList, 2) == NULL,
      "osrfListRemove should return NULL if there is a custom freeing \
       function defined on the list");
  fail_unless(osrfListGetIndex(testOsrfList, 2) == NULL,
      "osrfListRemove should remove the value from the list");
  fail_unless(testOsrfList->size == 2,
      "osrfListRemove should adjust the size of the list if the last \
       element is removed");
  fail_unless(freedItemsSize == 1,
      "osrfListRemove should call a custom item freeing function if \
       defined");
  testOsrfList->freeItem = NULL;
  fail_unless(osrfListRemove(testOsrfList, 0) == &globalItem1,
      "osrfListRemove should return the value that it has removed from \
      the list if no custom freeing function is defined on the list");
  fail_unless(osrfListGetIndex(testOsrfList, 0) == NULL,
      "osrfListRemove should remove the value from the list and make \
       the position NULL");
  fail_unless(testOsrfList->size == 2, "osrfListRemove should not touch \
      the size of the list if it isn't removing the last element in the \
      list");
END_TEST

START_TEST(test_osrf_list_osrfListExtract)
  fail_unless(osrfListExtract(NULL, 2) == NULL,
      "osrfListExtract should return NULL when not given a list");
  fail_unless(osrfListExtract(testOsrfList, 1000) == NULL,
      "osrfListExtract should return NULL when given a position \
       exceeding the size of the list");
  fail_unless(osrfListExtract(testOsrfList, 2) == &globalItem3,
      "osrfListExtract should return the value that it has removed \
       from the list");
  fail_unless(osrfListGetIndex(testOsrfList, 2) == NULL,
      "osrfListExtract should remove the value from the list and \
       make the position NULL");
  fail_unless(testOsrfList->size == 2,
      "osrfListExtract should adjust the size of the list if the \
       last element is removed");
  fail_unless(osrfListExtract(testOsrfList, 0) == &globalItem1,
      "osrfListExtract should return the value that it has removed \
       from the list");
  fail_unless(osrfListGetIndex(testOsrfList, 0) == NULL,
      "osrfListExtract should remove the value from the list and \
       make the position NULL");
  fail_unless(testOsrfList->size == 2,
      "osrfListExtract should not touch the size of the list if it \
       isn't removing the last element in the list");
END_TEST

START_TEST(test_osrf_list_osrfListFind)
  int* notInList1 = malloc(sizeof(int));
  int* notInList2 = malloc(sizeof(int));
  fail_unless(osrfListFind(NULL, &notInList1) == -1,
      "osrfListFind should return -1 when not given a list");
  fail_unless(osrfListFind(testOsrfList, NULL) == -1,
      "osrfListFind should return -1 when not given an addr");
  fail_unless(osrfListFind(testOsrfList, &globalItem3) == 2,
      "osrfListFind should return the index where the first instance \
       of addr is located");
  fail_unless(osrfListFind(testOsrfList, &notInList2) == -1,
      "osrfListFind should return -1 when the addr does not exist in \
       the list");
END_TEST

START_TEST(test_osrf_list_osrfListGetCount)
  fail_unless(osrfListGetCount(NULL) == -1,
      "osrfListGetCount should return -1 when no list is given");
  fail_unless(osrfListGetCount(testOsrfList) == 3,
      "osrfListGetCount should return list->size when given a list");
END_TEST

START_TEST(test_osrf_list_osrfListPop)
  fail_unless(osrfListPop(NULL) == NULL,
      "osrfListPop should return NULL when no list is given");
  fail_unless(osrfListPop(testOsrfList) == NULL,
      "osrfListPop should return NULL if there is a custom freeing \
       function defined on the list");
  fail_unless(testOsrfList->arrlist[2] == NULL,
      "osrfListPop should remove the last item from the list");
  testOsrfList->freeItem = NULL;
  int* item = malloc(sizeof(int));
  *item = 10;
  osrfListPush(testOsrfList, item);
  fail_unless( osrfListPop(testOsrfList) == item,
      "osrfListPop should return the last item from the list");
  fail_unless(testOsrfList->arrlist[2] == NULL,
      "osrfListPop should remove the last item from the list");
END_TEST

START_TEST(test_osrf_list_osrfNewListIterator)
  fail_unless(osrfNewListIterator(NULL) == NULL,
      "osrfNewListIterator should return NULL when no list is given");
  osrfListIterator *testListItr = osrfNewListIterator(testOsrfList);
  fail_if(testListItr == NULL,
      "osrfNewListIterator should create a osrfListIterator object");
  fail_unless(testListItr->list == testOsrfList,
      "osrfNewListIterator should set the osrfListIterator->list \
       attribute to the given list");
  fail_unless(testListItr->current == 0,
      "osrfNewListIterator should set its current position to 0 by \
       default");
END_TEST

START_TEST(test_osrf_list_osrfListIteratorNext)
  fail_unless(osrfListIteratorNext(NULL) == NULL,
      "osrfListIteratorNext should return NULL when no list given");
  osrfListIterator *testListItr = osrfNewListIterator(testOsrfList);
  fail_unless(osrfListIteratorNext(testListItr) == &globalItem1,
      "osrfListIteratorNext should return the value stored at the current \
       index in the list, then increment");
  fail_unless(osrfListIteratorNext(testListItr) == NULL,
      "osrfListIteratorNext should return the value stored at the current \
       index in the list, then increment");
  fail_unless(osrfListIteratorNext(testListItr) == &globalItem3,
      "osrfListIteratorNext should return the value stored at the current \
       index in the list, then increment");
  fail_unless(osrfListIteratorNext(testListItr) == NULL,
      "osrfListIteratorNext should return NULL when it reaches the end of \
       the list");
  testListItr->list = NULL;
  fail_unless(osrfListIteratorNext(testListItr) == NULL,
      "osrfListIteratorNext should return NULL if osrfListIterator->list \
       is NULL");
END_TEST

START_TEST(test_osrf_list_osrfListIteratorFree)
END_TEST

START_TEST(test_osrf_list_osrfListIteratorReset)
  osrfListIterator *testListItr = osrfNewListIterator(testOsrfList);
  osrfListIteratorNext(testListItr);
  osrfListIteratorReset(testListItr);
  fail_unless(testListItr->current == 0,
      "osrfListIteratorReset should reset the iterator's current position to 0");
END_TEST

START_TEST(test_osrf_list_osrfListSetDefaultFree)
END_TEST

//END TESTS

Suite *osrf_list_suite(void) {
  //Create test suite, test case, initialize fixture
  Suite *s = suite_create("osrf_list");
  TCase *tc_core = tcase_create("Core");
  tcase_add_checked_fixture(tc_core, setup, teardown);

  //Add tests to test case
  tcase_add_test(tc_core, test_osrf_list_osrfNewList);
  tcase_add_test(tc_core, test_osrf_list_osrfNewListSize);
  tcase_add_test(tc_core, test_osrf_list_osrfListPush);
  tcase_add_test(tc_core, test_osrf_list_osrfListPushFirst);
  tcase_add_test(tc_core, test_osrf_list_osrfListSet);
  tcase_add_test(tc_core, test_osrf_list_osrfListGetIndex);
  tcase_add_test(tc_core, test_osrf_list_osrfListFree);
  tcase_add_test(tc_core, test_osrf_list_osrfListClear);
  tcase_add_test(tc_core, test_osrf_list_osrfListSwap);
  tcase_add_test(tc_core, test_osrf_list_osrfListRemove);
  tcase_add_test(tc_core, test_osrf_list_osrfListExtract);
  tcase_add_test(tc_core, test_osrf_list_osrfListFind);
  tcase_add_test(tc_core, test_osrf_list_osrfListGetCount);
  tcase_add_test(tc_core, test_osrf_list_osrfListPop);
  tcase_add_test(tc_core, test_osrf_list_osrfNewListIterator);
  tcase_add_test(tc_core, test_osrf_list_osrfListIteratorNext);
  tcase_add_test(tc_core, test_osrf_list_osrfListIteratorFree);
  tcase_add_test(tc_core, test_osrf_list_osrfListIteratorReset);
  tcase_add_test(tc_core, test_osrf_list_osrfListSetDefaultFree);

  //Add test case to test suite
  suite_add_tcase(s, tc_core);

  return s;
}

void run_tests(SRunner *sr) {
  srunner_add_suite(sr, osrf_list_suite());
}
