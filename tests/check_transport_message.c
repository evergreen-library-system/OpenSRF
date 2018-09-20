#include <check.h>
#include "opensrf/transport_message.h"

transport_message *a_message; 

//Set up the test fixture
void setup(void) {
  a_message = message_init("body", "subject", "thread", "recipient", "sender");
}

//Clean up the test fixture
void teardown(void) {
  message_free(a_message);
}

//BEGIN TESTS

START_TEST(test_transport_message_init_empty)
  transport_message* empty_message = message_init(NULL, NULL, NULL, NULL, NULL);
  fail_if(empty_message == NULL, "transport_message wasn't created");
  fail_unless(strcmp(empty_message->body, "") == 0,
      "When calling message_init, an NULL body arg should yield an empty string");
  fail_unless(strcmp(empty_message->thread, "") == 0,
      "When calling message_init, an NULL thread arg should yield an empty string");
  fail_unless(strcmp(empty_message->subject, "") == 0,
      "When calling message_init, an NULL subject arg should yield an empty string");
  fail_unless(strcmp(empty_message->recipient, "") == 0,
      "When calling message_init, an NULL recipient arg should yield an empty string");
  fail_unless(strcmp(empty_message->sender, "") == 0,
      "When calling message_init, an NULL sender arg should yield an empty string");
  fail_unless(empty_message->router_from == NULL,
      "message_init should set the router_from field to NULL");
  fail_unless(empty_message->router_to == NULL,
      "message_init should set the router_to field to NULL");
  fail_unless(empty_message->router_class == NULL,
      "message_init should set the router_class field to NULL");
  fail_unless(empty_message->router_command == NULL,
      "message_init should set the router_command field to NULL");
  fail_unless(empty_message->osrf_xid == NULL,
      "message_init should set the osrf_xid field to NULL");
  fail_unless(empty_message->is_error == 0,
      "message_init should set the is_error field to 0");
  fail_unless(empty_message->error_type == NULL,
      "message_init should set the error_type field to NULL");
  fail_unless(empty_message->error_code == 0,
      "message_init should set the error_code field to 0");
  fail_unless(empty_message->broadcast == 0,
      "message_init should set the broadcast field to 0");
  fail_unless(empty_message->msg_xml == NULL,
      "message_init should set the msg_xml field to NULL");
  fail_unless(empty_message->next == NULL,
      "message_init should set the next field to NULL");
END_TEST

START_TEST(test_transport_message_init_populated)
  fail_if(a_message == NULL, "transport_message wasn't created");
  fail_unless(strcmp(a_message->body, "body") == 0,
      "When calling message_init, an body arg should be stored in the body field");
  fail_unless(strcmp(a_message->thread, "thread") == 0,
      "When calling message_init, an thread arg should be stored in the thread field");
  fail_unless(strcmp(a_message->subject, "subject") == 0,
      "When calling message_init, a subject arg should be stored in the subject field");
  fail_unless(strcmp(a_message->recipient, "recipient") == 0,
      "When calling message_init, a recipient arg should be stored in the recipient field");
  fail_unless(strcmp(a_message->sender, "sender") == 0,
      "When calling message_init, a sender arg should be stored in the sender field");
END_TEST

START_TEST(test_transport_message_new_message_from_xml_empty)
  fail_unless(new_message_from_xml(NULL) == NULL,
      "Passing NULL to new_message_from_xml should return NULL");
  fail_unless(new_message_from_xml("\0") == NULL,
      "Passing a NULL string to new_message_from_xml should return NULL");

  const char* empty_msg = "<message/>";
  transport_message* t_msg = new_message_from_xml(empty_msg);
  fail_if(t_msg == NULL,
      "new_message_from_xml should create a new transport_message");
  fail_unless(strcmp(t_msg->thread, "") == 0,
      "When passed no thread, msg->thread should be set to an empty string");
  fail_unless(strcmp(t_msg->subject, "") == 0,
      "When passed no subject, msg->subject should be set to an empty string");
  fail_unless(strcmp(t_msg->body, "") == 0,
      "When passed no body, msg->body should be set to an empty string");
  fail_unless(t_msg->recipient == NULL,
      "When passed no recipient, msg->recipient should be NULL");
  fail_unless(t_msg->sender == NULL,
      "When passed no sender, msg->sender should be NULL");
  fail_unless(t_msg->router_from == NULL,
      "When passed no router_from, msg->router_from should be NULL");
  fail_unless(t_msg->router_to == NULL,
      "When passed no router_to, msg->router_to should be NULL");
  fail_unless(t_msg->router_class == NULL,
      "When passed no router_class, msg->router_class should be NULL");
  fail_unless(t_msg->router_command == NULL,
      "router_command should never be passed, and therefore should be NULL");
  fail_unless(t_msg->osrf_xid == NULL,
      "When passed no osrf_xid, msg->osrf_xid should be NULL");
  fail_unless(t_msg->is_error == 0,
      "is_error should never be passed, and msg->is_error should be set to 0");
  fail_unless(t_msg->error_type == NULL,
      "error_type should never be passed, and should be NULL");
  fail_unless(t_msg->error_code == 0,
      "error_code should never be passed, and msg->error_code should be set to 0");
  fail_unless(t_msg->broadcast == 0,
      "When passed no broadcast, msg->broadcast should be set to 0");
  fail_unless(strcmp(t_msg->msg_xml, "<message/>") == 0,
      "msg->msg_xml should contain the contents of the original xml message");
  fail_unless(t_msg->next == NULL, "msg->next should be set to NULL");
END_TEST

START_TEST(test_transport_message_new_message_from_xml_populated)
  const char* xml_jabber_msg =
    "<message from=\"sender\" to=\"receiver\"><opensrf router_from=\"routerfrom\" router_to=\"routerto\" router_class=\"class\" broadcast=\"1\" osrf_xid=\"xid\"/><thread>thread_value</thread><subject>subject_value</subject><body>body_value</body></message>";

  transport_message *my_msg = new_message_from_xml(xml_jabber_msg);
  fail_if(my_msg == NULL, "new_message_from_xml failed to create a transport_message");
  fail_unless(strcmp(my_msg->sender, "routerfrom") == 0,
      "new_message_from_xml should populate the sender field");
  fail_unless(strcmp(my_msg->recipient, "receiver") == 0,
      "new_message_from_xml should populate the receiver field");
  fail_unless(strcmp(my_msg->osrf_xid, "xid") == 0,
      "new_message_from_xml should populate the osrf_xid field");
  fail_unless(strcmp(my_msg->router_from, "routerfrom") == 0,
      "new_message_from_xml should populate the router_from field");
  fail_unless(strcmp(my_msg->subject, "subject_value") == 0,
      "new_message_from_xml should populate the subject field");
  fail_unless(strcmp(my_msg->thread, "thread_value") == 0,
      "new_message_from_xml should populate the thread field");
  fail_unless(strcmp(my_msg->router_to, "routerto") == 0,
      "new_message_from_xml should populate the router_to field");
  fail_unless(strcmp(my_msg->router_class, "class") == 0,
      "new_message_from_xml should populate the router_class field");
  fail_unless(my_msg->broadcast == 1,
      "new_message_from_xml should populate the broadcast field");
  fail_unless(strcmp(my_msg->msg_xml, xml_jabber_msg) == 0,
      "new_message_from_xml should store the original xml msg in msg_xml");
END_TEST

START_TEST(test_transport_message_set_osrf_xid)
  message_set_osrf_xid(a_message, "abcd");
  fail_unless(strcmp(a_message->osrf_xid, "abcd") == 0,
      "message_set_osrf_xid should set msg->osrf_xid to the value of the osrf_xid arg");
  message_set_osrf_xid(a_message, NULL);
  fail_unless(strcmp(a_message->osrf_xid, "") == 0,
      "message_set_osrf_xid should set msg->osrf_xid to an empty string if osrf_xid arg is NULL");
END_TEST

START_TEST(test_transport_message_set_router_info_empty)
  message_set_router_info(a_message, NULL, NULL, NULL, NULL, 0);
  fail_unless(strcmp(a_message->router_from, "") == 0,
      "message_set_router_info should set msg->router_from to empty string if NULL router_from arg is passed");
  fail_unless(strcmp(a_message->router_to, "") == 0,
      "message_set_router_info should set msg->router_to to empty string if NULL router_to arg is passed");
  fail_unless(strcmp(a_message->router_class, "") == 0,
      "message_set_router_info should set msg->router_class to empty string if NULL router_class arg is passed");
  fail_unless(strcmp(a_message->router_command, "") == 0,
      "message_set_router_info should set msg->router_command to empty string if NULL router_command arg is passed");
  fail_unless(a_message->broadcast == 0,
      "message_set_router_info should set msg->broadcast to the content of the broadcast_enabled arg");
END_TEST

START_TEST(test_transport_message_set_router_info_populated)
  message_set_router_info(a_message, "routerfrom", "routerto", "routerclass", "routercmd", 1);
  fail_unless(strcmp(a_message->router_from, "routerfrom") == 0,
      "message_set_router_info should set msg->router_from to the value of the router_from arg");
  fail_unless(strcmp(a_message->router_to, "routerto") == 0,
      "message_set_router_info should set msg->router_to to the value of the router_to arg");
  fail_unless(strcmp(a_message->router_class, "routerclass") == 0,
      "message_set_router_info should set msg->router_class to the value of the router_class arg");
  fail_unless(strcmp(a_message->router_command, "routercmd") == 0,
      "message_set_router_info should set msg->router_command to the value of the router_command arg");
  fail_unless(a_message->broadcast == 1,
      "message_set_router_info should set msg->broadcast to the value of the broadcast_enabled arg");
END_TEST

START_TEST(test_transport_message_free)
  fail_unless(message_free(NULL) == 0,
      "message_free should return 0 if passed a NULL msg arg");
  transport_message *msg = message_init("one", "two", "three", "four", "five");
  fail_unless(message_free(msg) == 1,
      "message_free should return 1 if successful");
END_TEST

START_TEST(test_transport_message_prepare_xml)
  fail_unless(message_prepare_xml(NULL) == 0,
      "Passing a NULL msg arg to message_prepare_xml should return 0");

  transport_message *msg = message_init(NULL,NULL,NULL,NULL,NULL);
  msg->msg_xml = "somevalue";
  fail_unless(message_prepare_xml(msg) == 1,
      "If msg->msg_xml is already populated, message_prepare_xml should return 1");

  message_set_router_info(a_message, "routerfrom", "routerto", "routerclass", "routercommand", 1);
  message_set_osrf_xid(a_message, "osrfxid");
  set_msg_error(a_message, "errortype", 123);

  fail_unless(message_prepare_xml(a_message) == 1,
      "message_prepare_xml should return 1 upon success");
  fail_if(a_message->msg_xml == NULL,
      "message_prepare_xml should store the returned xml in msg->msg_xml");

  fail_unless(strcmp(a_message->msg_xml, "<message to=\"recipient\" from=\"sender\"><error type=\"errortype\" code=\"123\"/><opensrf router_from=\"routerfrom\" router_to=\"routerto\" router_class=\"routerclass\" router_command=\"routercommand\" osrf_xid=\"osrfxid\" broadcast=\"1\"/><thread>thread</thread><subject>subject</subject><body>body</body></message>") == 0,
      "message_prepare_xml should store the correct xml in msg->msg_xml");
END_TEST

START_TEST(test_transport_message_jid_get_username)
  int buf_size = 15;
  char buffer[buf_size];
  jid_get_username("testuser@domain.com/stuff", buffer, buf_size);
  fail_unless(strcmp(buffer, "testuser") == 0,
      "jid_get_username should set the buffer to the username extracted from the jid arg");
END_TEST

START_TEST(test_transport_message_jid_get_resource)
  char buf_size = 15;
  char buffer[buf_size];
  jid_get_resource("testuser@domain.com/stuff", buffer, buf_size);
  fail_unless(strcmp(buffer, "stuff") == 0,
      "jid_get_resource should set the buffer to the resource extracted from the jid arg");
  jid_get_resource("testuser@domain.com", buffer, buf_size);
  fail_unless(strcmp(buffer, "") == 0,
      "jid_get_resource should set the buffer to an empty string if there is no resource");
END_TEST

START_TEST(test_transport_message_jid_get_domain)
  char buf_size = 15;
  char buffer[buf_size];
  jid_get_domain("testuser@domain.com/stuff", buffer, buf_size);
  fail_unless(strcmp(buffer, "domain.com") == 0,
      "jid_get_domain should set the buffer to the domain extracted from the jid arg");
  jid_get_domain("ksdljflksd",  buffer, buf_size);
  fail_unless(strcmp(buffer, "") == 0,
      "jid_get_domain should set the buffer to an empty string if the jid is malformed");
END_TEST

START_TEST(test_transport_message_set_msg_error)
  set_msg_error(a_message, NULL, 111);
  fail_unless(a_message->is_error == 1,
      "set_msg_error called with a NULL error type should only set msg->is_error to 1");
  set_msg_error(a_message, "fatal", 123);
  fail_unless(a_message->is_error == 1,
      "set_msg_error should set msg->is_error to 1");
  fail_unless(strcmp(a_message->error_type, "fatal") == 0,
      "set_msg_error should set msg->error_type if error_type arg is passed");
  fail_unless(a_message->error_code == 123,
      "set_msg_error should set msg->error_code to the value of the err_code arg");
END_TEST
//END TESTS

Suite *transport_message_suite(void) {
  //Create test suite, test case, initialize fixture
  Suite *s = suite_create("transport_message");
  TCase *tc_core = tcase_create("Core");
  tcase_add_checked_fixture(tc_core, setup, teardown);

  //Add tests to test case
  tcase_add_test(tc_core, test_transport_message_init_empty);
  tcase_add_test(tc_core, test_transport_message_init_populated);
  tcase_add_test(tc_core, test_transport_message_new_message_from_xml_empty);
  tcase_add_test(tc_core, test_transport_message_new_message_from_xml_populated);
  tcase_add_test(tc_core, test_transport_message_set_osrf_xid);
  tcase_add_test(tc_core, test_transport_message_set_router_info_empty);
  tcase_add_test(tc_core, test_transport_message_set_router_info_populated);
  tcase_add_test(tc_core, test_transport_message_free);
  tcase_add_test(tc_core, test_transport_message_prepare_xml);
  tcase_add_test(tc_core, test_transport_message_jid_get_username);
  tcase_add_test(tc_core, test_transport_message_jid_get_resource);
  tcase_add_test(tc_core, test_transport_message_jid_get_domain);
  tcase_add_test(tc_core, test_transport_message_set_msg_error);

  //Add test case to test suite
  suite_add_tcase(s, tc_core);

  return s;
}

void run_tests(SRunner *sr) {
  srunner_add_suite(sr, transport_message_suite());
}
