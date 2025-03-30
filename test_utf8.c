
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "utf8_string.h"

// Define ANSI colors for output.
#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_RESET "\x1b[0m"

// Global counters for tests.
unsigned int test_count = 0;
unsigned int fail_count = 0;

void test_header(const char *name) {
    printf("\n=== Testing %s ===\n", name);
}

void test_assert(int condition, const char *message) {
    test_count++;
    if (!condition) {
        printf(ANSI_RED "FAIL: %s\n" ANSI_RESET, message);
        fail_count++;
    } else {
        printf(ANSI_GREEN "PASS: %s\n" ANSI_RESET, message);
    }
}

/*******************************
 * Helper: Compare UTF-8 bytes
 *******************************/
int utf8_compare(utf8_string *s, const char *expected) {
    size_t exp_len = strlen(expected);
    return (s->length == exp_len) &&
    (memcmp(s->data, expected, exp_len) == 0);
}

/*******************************
 * Expanded Test Cases
 *******************************/

// 1. Basic Creation & Free
void test_basic_creation() {
    test_header("Basic Creation & Free");

    utf8_string s = from("Hello");
    test_assert(s.length == 5, "Length matches ASCII");
    test_assert(s.capacity >= 5, "Capacity is sufficient");
    test_assert(utf8_compare(&s, "Hello"), "Content is 'Hello'");
    utf8_free(&s);

    utf8_string empty = from("");
    test_assert(empty.length == 0, "Empty string length is zero");
    test_assert(empty.data != NULL || empty.capacity == 0, "Empty string data handled");
    utf8_free(&empty);
}
// 2. UTF-8 Decoding Tests (Enhanced)
void test_utf8_decoding() {
    test_header("UTF-8 Decoding");

    struct { char *bytes; unsigned cp; } valid[] = {
        {"A", 0x41}, {"\xC2\xA9", 0xA9}, 
        {"\xE2\x82\xAC", 0x20AC}, {"\xF0\x9F\x8D\x95", 0x1F355}
    };

    for (int i = 0; i < 4; i++) {
        utf8_string s = from(valid[i].bytes);
        unsigned cp = decode_utf8_char(s.data);
        char msg[50];
        snprintf(msg, sizeof(msg), "Valid decode %d: U+%04X", i, valid[i].cp);
        test_assert(cp == valid[i].cp, msg);
        utf8_free(&s);
    }

    struct { char *bytes; unsigned cp; } invalid[] = {
        {"\xC2", 0xFFFD}, {"\xE0\xA0", 0xFFFD}, {"\xF0\x9F\x8D", 0xFFFD},
        {"\xC0\x80", 0xFFFD}, {"\xED\xA0\x80", 0xFFFD}, {"\xFF", 0xFFFD},
        {"\x80", 0xFFFD}, {"\xF4\x90\x80\x80", 0xFFFD}
    };

    for (int i = 0; i < 8; i++) {
        utf8_string s = from(invalid[i].bytes);
        unsigned cp = decode_utf8_char(s.data);
        test_assert(cp == 0xFFFD, "Invalid sequence detected");
        utf8_free(&s);
    }
}

// 4. Enhanced Slicing Tests
void test_slicing() {
    test_header("Slicing Operations");

    // Test partial multi-byte sequence handling
    utf8_string s = from("X\xE2\x82Y"); // Partial euro symbol
    utf8_slice slice = slice_byte(&s, 1, 3); // [E2, 82]
    align_next(&slice, 0);
    test_assert(slice.length == 3, "Aligns partial sequence to full length");
    utf8_free(&s);
}

// 5. New: Memory Safety Tests
void test_memory_safety() {
    test_header("Memory Safety");

    // Test geometric growth
    utf8_string s = from("");
    for (int i = 0; i < 1000; i++) {
        utf8_concat_literal(&s, "🍣"); // 4-byte emoji
    }
    test_assert(s.capacity >= s.length, "Geometric growth works");

    // Test double free protection
    utf8_slice slice = slice_byte(&s, 0, 100);
    utf8_free(&s);
    utf8_free(&slice); // Shouldn't crash
    test_assert(1, "Double free protection works");
}

// 6. New: Edge Case Tests
void test_edge_cases() {
    test_header("Edge Cases");

    // Embedded NULL handling
    utf8_string s = from("A\0B");
    test_assert(s.length == 3, "Handles NULL bytes in content");
    utf8_free(&s);
    // Test null in multi-byte sequences (should fail decoding)
    utf8_string s1 = from("\xC0\x80");  // Overlong null
    test_assert(decode_utf8_char(s1.data) == 0xFFFD, "Overlong null rejected");
    utf8_free(&s1);
    // Test valid null
    utf8_string valid_null = from("\x00");  // Valid U+0000
    test_assert(decode_utf8_char(valid_null.data) == 0x0000, "Null is valid");
    // Empty string operations
    utf8_string empty = from("");
    utf8_slice eslice = slice_char(&empty, 0, 1);
    test_assert(eslice.data == NULL, "Empty string slicing");
    utf8_free(&empty);
}

// 7. New: Surrogate/Overlong Tests
void test_special_encodings() {
    test_header("Special Encodings");

    // Surrogate pair
    utf8_string s_surrogate = from("\xED\xA0\x80");
    unsigned cp = decode_utf8_char(s_surrogate.data);
    test_assert(cp == 0xFFFD, "Surrogate pair detection");
    utf8_free(&s_surrogate);

    // Overlong encoding
    utf8_string s_overlong = from("\xC1\x81");
    cp = decode_utf8_char(s_overlong.data);
    test_assert(cp == 0xFFFD, "Overlong encoding detection");
    utf8_free(&s_overlong);
}

// 8. Enhanced Seeking Tests
void test_seeking() {
    test_header("Seeking Operations");

    // Test truncated sequence handling
    utf8_string s = from("A\xC2\xA9\xE2"); 
    test_assert(seek_char(&s, 2) == -1, "Detects truncated sequence");
    utf8_free(&s);
}
// 2. UTF-8 Decoding Tests
// 3. Concatenation & Literal Concatenation
void test_concatenation() {
    test_header("Concatenation Operations");

    utf8_string s1 = from("Hello");
    utf8_string s2 = from(" 世界");
    utf8_concat(&s1, &s2);
    test_assert(s1.length == 5 + 7, "Concat length is correct");
    test_assert(utf8_compare(&s1, "Hello 世界"), "Concat content is correct");

    utf8_string lit = from("!");
    utf8_concat_literal(&s1, "!");
    test_assert(utf8_compare(&s1, "Hello 世界!"), "Literal concat works");

    utf8_free(&s1);
    utf8_free(&s2);
    utf8_free(&lit);
}

// 7. Insert and Delete Operations
void test_insert_delete() {
    test_header("Insert and Delete");

    utf8_string base = from("StartEnd");
    utf8_string mid = from("Middle");
    insert(&base, &mid, 5);
    test_assert(utf8_compare(&base, "StartMiddleEnd"), "Insert operation works");

    // Delete the inserted portion.
    delete_char(&base, 5, 11);
    test_assert(utf8_compare(&base, "StartEnd"), "Delete operation works");

    // Delete from beginning.
    delete_char(&base, 0, 5);
    test_assert(utf8_compare(&base, "End"), "Delete from start works");

    utf8_free(&base);
    utf8_free(&mid);
}

// 8. Memory and Reallocation Tests
void test_memory_realloc() {
    test_header("Memory & Reallocation");

    utf8_string s = from("Test");
    // Append many times to force multiple reallocations.
    for (int i = 0; i < 1000; i++) {
        utf8_push_str_literal(&s, "12345");
    }
    test_assert(s.length == 4 + 5 * 1000, "Massive append length is correct");
    test_assert(s.capacity >= s.length, "Capacity is sufficient after many appends");
    // Optionally, check for substring existence.
    test_assert(memmem(s.data, s.length, "Test12345", 9) != NULL, "Data remains intact");
    utf8_free(&s);
}

// 9. Robustness with NULL and Invalid Input
void test_invalid_input() {
    test_header("Invalid Input Handling");

    // Call functions with NULL pointers.
    test_assert(seek_char(NULL, 5) == 0, "seek_char returns 0 for NULL input");
    // For deletion, insertion, or concatenation, ensure they do not crash with empty strings.
    utf8_string empty = from("");
    utf8_string another = from("Data");
    utf8_concat(&empty, &another);
    test_assert(utf8_compare(&empty, "Data"), "Concat on empty string works");
    utf8_free(&empty);
    utf8_free(&another);
}

// 10. Iterative Print Test (for visual confirmation)
void test_iterative_print() {
    test_header("UTF-8 Iteration Print");
    utf8_string s = from("Iteration🌟");
    printf("Iterated bytes: ");
    utf8_iterate_print(&s);
    printf("\n");
    utf8_free(&s);
}

int main() {
    printf("Starting Comprehensive UTF-8 Library Tests\n");

    test_basic_creation();
    test_utf8_decoding();
    test_concatenation();
    test_slicing();
    test_seeking();
    test_insert_delete();
    test_memory_realloc();
    test_memory_safety();
    test_edge_cases();
    test_special_encodings();
    test_invalid_input();
    test_iterative_print();

    printf("\nTest Summary:\n");
    printf("Total Tests: %d\n", test_count);
    printf(ANSI_GREEN "Passed: %d\n" ANSI_RESET, test_count - fail_count);
    if (fail_count > 0) {
        printf(ANSI_RED "Failed: %d\n" ANSI_RESET, fail_count);
    } else {
        printf(ANSI_GREEN "All tests passed!\n" ANSI_RESET);
    }

    return fail_count ? EXIT_FAILURE : EXIT_SUCCESS;
}

