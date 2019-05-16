#include <check.h>
#include <stdio.h>
#include <unistd.h>

#include "stream.h"
#include "rtr/pdu.c"

/*
 * Just a wrapper for `buffer2fd()`. Boilerplate one-liner.
 */
#define BUFFER2FD(buffer, cb, obj) {					\
	struct pdu_header header;					\
	int fd, err;							\
									\
	fd = buffer2fd(buffer, sizeof(buffer));				\
	ck_assert_int_ge(fd, 0);					\
	init_pdu_header(&header);					\
	err = cb(&header, fd, obj);					\
	close(fd);							\
	ck_assert_int_eq(err, 0);					\
	assert_pdu_header(&(obj)->header);				\
}

static void
init_pdu_header(struct pdu_header *header)
{
	header->protocol_version = 0;
	header->pdu_type = 22;
	header->m.reserved = 12345;
	header->length = 0xFFAA9955;
}

static void
assert_pdu_header(struct pdu_header *header)
{
	ck_assert_uint_eq(header->protocol_version, 0);
	ck_assert_uint_eq(header->pdu_type, 22);
	ck_assert_uint_eq(header->m.reserved, 12345);
	ck_assert_uint_eq(header->length, 0xFFAA9955);
}

START_TEST(test_pdu_header_from_stream)
{
	unsigned char input[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	struct pdu_header header;
	int fd;
	int err;

	fd = buffer2fd(input, sizeof(input));
	ck_assert_int_ge(fd, 0);
	err = pdu_header_from_reader(fd, &header);
	close(fd);
	ck_assert_int_eq(err, 0);

	ck_assert_uint_eq(header.protocol_version, 0);
	ck_assert_uint_eq(header.pdu_type, 1);
	ck_assert_uint_eq(header.m.reserved, 0x0203);
	ck_assert_uint_eq(header.length, 0x04050607);
}
END_TEST

START_TEST(test_serial_notify_from_stream)
{
	unsigned char input[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
	struct serial_notify_pdu pdu;

	BUFFER2FD(input, serial_notify_from_stream, &pdu);
	ck_assert_uint_eq(pdu.serial_number, 0x010203);
}
END_TEST

START_TEST(test_serial_query_from_stream)
{
	unsigned char input[] = { 13, 14, 15, 16, 17 };
	struct serial_query_pdu pdu;

	BUFFER2FD(input, serial_query_from_stream, &pdu);
	ck_assert_uint_eq(pdu.serial_number, 0x0d0e0f10);
}
END_TEST

START_TEST(test_reset_query_from_stream)
{
	unsigned char input[] = { 18, 19 };
	struct reset_query_pdu pdu;

	BUFFER2FD(input, reset_query_from_stream, &pdu);
}
END_TEST

START_TEST(test_cache_response_from_stream)
{
	unsigned char input[] = { 18, 19 };
	struct cache_response_pdu pdu;

	BUFFER2FD(input, cache_response_from_stream, &pdu);
}
END_TEST

START_TEST(test_ipv4_prefix_from_stream)
{
	unsigned char input[] = { 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
			29, 30, 31, 32 };
	struct ipv4_prefix_pdu pdu;

	BUFFER2FD(input, ipv4_prefix_from_stream, &pdu);
	ck_assert_uint_eq(pdu.flags, 18);
	ck_assert_uint_eq(pdu.prefix_length, 19);
	ck_assert_uint_eq(pdu.max_length, 20);
	ck_assert_uint_eq(pdu.zero, 21);
	ck_assert_uint_eq(pdu.ipv4_prefix.s_addr, 0x16171819);
	ck_assert_uint_eq(pdu.asn, 0x1a1b1c1d);
}
END_TEST

START_TEST(test_ipv6_prefix_from_stream)
{
	unsigned char input[] = { 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
			44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
			58, 59, 60 };
	struct ipv6_prefix_pdu pdu;

	BUFFER2FD(input, ipv6_prefix_from_stream, &pdu);
	ck_assert_uint_eq(pdu.flags, 33);
	ck_assert_uint_eq(pdu.prefix_length, 34);
	ck_assert_uint_eq(pdu.max_length, 35);
	ck_assert_uint_eq(pdu.zero, 36);
	ck_assert_uint_eq(pdu.ipv6_prefix.s6_addr32[0], 0x25262728);
	ck_assert_uint_eq(pdu.ipv6_prefix.s6_addr32[1], 0x292a2b2c);
	ck_assert_uint_eq(pdu.ipv6_prefix.s6_addr32[2], 0x2d2e2f30);
	ck_assert_uint_eq(pdu.ipv6_prefix.s6_addr32[3], 0x31323334);
	ck_assert_uint_eq(pdu.asn, 0x35363738);
}
END_TEST

START_TEST(test_end_of_data_from_stream)
{
	unsigned char input[] = { 61, 62, 63, 64 };
	struct end_of_data_pdu pdu;

	BUFFER2FD(input, end_of_data_from_stream, &pdu);
	ck_assert_uint_eq(pdu.serial_number, 0x3d3e3f40);
}
END_TEST

START_TEST(test_cache_reset_from_stream)
{
	unsigned char input[] = { 65, 66, 67 };
	struct cache_reset_pdu pdu;

	BUFFER2FD(input, cache_reset_from_stream, &pdu);
}
END_TEST

START_TEST(test_error_report_from_stream)
{
	unsigned char input[] = {
			/* Sub-pdu length */
			0, 0, 0, 12,
			/* Sub-pdu */
			1, 0, 2, 3, 0, 0, 0, 12, 1, 2, 3, 4,
			/* Error msg length */
			0, 0, 0, 5,
			/* Error msg */
			'h', 'e', 'l', 'l', 'o',
			/* Garbage */
			1, 2, 3, 4,
	};
	struct error_report_pdu *pdu;
	struct serial_notify_pdu *sub_pdu;

	pdu = malloc(sizeof(struct error_report_pdu));
	if (!pdu)
		ck_abort_msg("PDU allocation failure");

	BUFFER2FD(input, error_report_from_stream, pdu);

	sub_pdu = pdu->erroneous_pdu;
	ck_assert_uint_eq(sub_pdu->header.protocol_version, 1);
	ck_assert_uint_eq(sub_pdu->header.pdu_type, 0);
	ck_assert_uint_eq(sub_pdu->header.m.reserved, 0x0203);
	ck_assert_uint_eq(sub_pdu->header.length, 12);
	ck_assert_uint_eq(sub_pdu->serial_number, 0x01020304);
	ck_assert_str_eq(pdu->error_message, "hello");

	/*
	 * Yes, this test memory leaks on failure.
	 * Not sure how to fix it without making a huge mess.
	 */
	error_report_destroy(pdu);
}
END_TEST

START_TEST(test_interrupted)
{
	unsigned char input[] = { 0, 1 };
	struct serial_notify_pdu pdu;
	struct pdu_header header;
	int fd, err;

	fd = buffer2fd(input, sizeof(input));
	ck_assert_int_ge(fd, 0);
	init_pdu_header(&header);
	err = serial_notify_from_stream(&header, fd, &pdu);
	close(fd);
	ck_assert_int_eq(err, -EPIPE);
}
END_TEST

Suite *pdu_suite(void)
{
	Suite *suite;
	TCase *core, *errors;

	core = tcase_create("Core");
	tcase_add_test(core, test_pdu_header_from_stream);
	tcase_add_test(core, test_serial_notify_from_stream);
	tcase_add_test(core, test_serial_notify_from_stream);
	tcase_add_test(core, test_serial_query_from_stream);
	tcase_add_test(core, test_reset_query_from_stream);
	tcase_add_test(core, test_cache_response_from_stream);
	tcase_add_test(core, test_ipv4_prefix_from_stream);
	tcase_add_test(core, test_ipv6_prefix_from_stream);
	tcase_add_test(core, test_end_of_data_from_stream);
	tcase_add_test(core, test_cache_reset_from_stream);
	tcase_add_test(core, test_error_report_from_stream);

	errors = tcase_create("Errors");
	tcase_add_test(errors, test_interrupted);

	suite = suite_create("PDU");
	suite_add_tcase(suite, core);
	suite_add_tcase(suite, errors);
	return suite;
}

int main(void)
{
	Suite *suite;
	SRunner *runner;
	int tests_failed;

	suite = pdu_suite();

	runner = srunner_create(suite);
	srunner_run_all(runner, CK_NORMAL);
	tests_failed = srunner_ntests_failed(runner);
	srunner_free(runner);

	return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}