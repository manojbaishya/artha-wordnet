/* tests/unit/test_wni.c
 * GLib GTest unit tests for the WordNet Interface (wni.c / wni.h).
 *
 * Pure, side-effect-free logic only — no WordNet database, no GUI, no D-Bus.
 * wni.c is compiled in directly (with `static` stripped) so its static
 * helpers are visible to the tests; nothing here calls wni_request_nyms(),
 * so no WordNet data files are needed at runtime (only libwordnet for the
 * link, because wni.c's other functions reference wn symbols).
 *
 * Build: part of the `artha-unit-test` xmake target.
 * Run:   xmake run artha-unit-test
 */

#define static          /* expose wni.c's static helpers for unit testing */
#include "core/wni.c"        /* brings wn.h, wni.h and all of wni.c */

#include <glib.h>
#include <glib/gtestutils.h>

/* ------------------------------------------------------------------ */
/* wni_strcmp0: NULL-safe, case-insensitive comparison                */
/* ------------------------------------------------------------------ */

static void test_wni_strcmp0_both_null(void)
{
	g_assert_cmpint(wni_strcmp0(NULL, NULL), ==, 0);
}

static void test_wni_strcmp0_first_null(void)
{
	g_assert_cmpint(wni_strcmp0(NULL, "artha"), ==, -1);
}

static void test_wni_strcmp0_second_null(void)
{
	g_assert_cmpint(wni_strcmp0("artha", NULL), ==, 1);
}

static void test_wni_strcmp0_equal(void)
{
	g_assert_cmpint(wni_strcmp0("artha", "artha"), ==, 0);
}

static void test_wni_strcmp0_same_pointer(void)
{
	const char *s = "happy";
	g_assert_cmpint(wni_strcmp0(s, s), ==, 0);
}

static void test_wni_strcmp0_case_insensitive(void)
{
	g_assert_cmpint(wni_strcmp0("Happy", "happy"), ==, 0);
	g_assert_cmpint(wni_strcmp0("WORDNET", "wordnet"), ==, 0);
}

static void test_wni_strcmp0_ordering(void)
{
	g_assert_cmpint(wni_strcmp0("abc", "abd"), <, 0);
	g_assert_cmpint(wni_strcmp0("abd", "abc"), >, 0);
	g_assert_cmpint(wni_strcmp0("a", "ab"), <, 0);
	g_assert_cmpint(wni_strcmp0("ab", "a"), >, 0);
}

/* ------------------------------------------------------------------ */
/* strip_adj_marker: strip "(a)", "(p)", "(ip)" from adjectives       */
/* Modifies the string in place and returns the same pointer.         */
/* ------------------------------------------------------------------ */

static void test_strip_adj_marker_adj_marker(void)
{
	gchar *word = g_strdup("beautiful(a)");
	gchar *result = strip_adj_marker(word);
	g_assert_cmpstr(result, ==, "beautiful");
	g_assert_cmpstr(word, ==, "beautiful");   /* modified in place */
	g_assert(result == word);                 /* same pointer returned */
	g_free(word);
}

static void test_strip_adj_marker_predicative(void)
{
	gchar *word = g_strdup("tall(p)");
	g_assert_cmpstr(strip_adj_marker(word), ==, "tall");
	g_free(word);
}

static void test_strip_adj_marker_postnominal(void)
{
	gchar *word = g_strdup("sick(ip)");
	g_assert_cmpstr(strip_adj_marker(word), ==, "sick");
	g_free(word);
}

static void test_strip_adj_marker_no_marker(void)
{
	gchar *word = g_strdup("happy");
	g_assert_cmpstr(strip_adj_marker(word), ==, "happy");
	g_free(word);
}

static void test_strip_adj_marker_empty(void)
{
	gchar *word = g_strdup("");
	g_assert_cmpstr(strip_adj_marker(word), ==, "");
	g_free(word);
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	/* wni_strcmp0 */
	g_test_add_func("/artha/unit/wni_strcmp0/both_null", test_wni_strcmp0_both_null);
	g_test_add_func("/artha/unit/wni_strcmp0/first_null", test_wni_strcmp0_first_null);
	g_test_add_func("/artha/unit/wni_strcmp0/second_null", test_wni_strcmp0_second_null);
	g_test_add_func("/artha/unit/wni_strcmp0/equal", test_wni_strcmp0_equal);
	g_test_add_func("/artha/unit/wni_strcmp0/same_pointer", test_wni_strcmp0_same_pointer);
	g_test_add_func("/artha/unit/wni_strcmp0/case_insensitive", test_wni_strcmp0_case_insensitive);
	g_test_add_func("/artha/unit/wni_strcmp0/ordering", test_wni_strcmp0_ordering);

	/* strip_adj_marker */
	g_test_add_func("/artha/unit/strip_adj_marker/adj_marker", test_strip_adj_marker_adj_marker);
	g_test_add_func("/artha/unit/strip_adj_marker/predicative", test_strip_adj_marker_predicative);
	g_test_add_func("/artha/unit/strip_adj_marker/postnominal", test_strip_adj_marker_postnominal);
	g_test_add_func("/artha/unit/strip_adj_marker/no_marker", test_strip_adj_marker_no_marker);
	g_test_add_func("/artha/unit/strip_adj_marker/empty", test_strip_adj_marker_empty);

	return g_test_run();
}
