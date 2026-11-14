/* tests/integration/test_wni_integration.c
 * GLib GTest integration tests against the real WordNet database.
 *
 * Exercises wni_request_nyms() / wni_free() through the public API in
 * src/wni.h, with WordNet's data files (/usr/share/wordnet on this host)
 * and real memory allocation. No GUI, no D-Bus.
 *
 * Build: part of the `artha-integration-test` xmake target (links wni +
 * glib + wordnet).
 * Run:   xmake run artha-integration-test
 */

#include <glib.h>
#include <glib/gtestutils.h>
#include "core/wni.h"

/* ------------------------------------------------------------------ */
/* helpers                                                            */
/* ------------------------------------------------------------------ */

/* Walk the GSList of WNINym and return the WNIOverview for OVERVIEW id,
 * or NULL if not present / no definitions were produced. */
static WNIOverview *overview_of(GSList *response)
{
	GSList *t = response;
	while(t)
	{
		WNINym *nym = (WNINym *) t->data;
		if(nym && nym->id == WORDNET_INTERFACE_OVERVIEW && nym->data)
			return (WNIOverview *) nym->data;
		t = g_slist_next(t);
	}
	return NULL;
}

static gboolean overview_has_synonyms(WNIOverview *ov)
{
	if(!ov || !ov->synonyms_list)
		return FALSE;
	return ov->synonyms_list->data != NULL;
}

/* ------------------------------------------------------------------ */
/* wni_request_nyms against real WordNet                              */
/* ------------------------------------------------------------------ */

static void test_known_word_returns_true(void)
{
	GSList *response = NULL;
	gboolean ok = wni_request_nyms("happy", &response, WORDNET_INTERFACE_OVERVIEW, FALSE);
	g_assert_true(ok);
	g_assert_nonnull(response);
	WNIOverview *ov = overview_of(response);
	g_assert_nonnull(ov);
	g_assert_true(ov->definitions_list != NULL);
	g_assert_true(overview_has_synonyms(ov));
	wni_free(&response);
	g_assert_null(response); /* wni_free NULLs the caller's list pointer */
}

static void test_unknown_word_returns_false(void)
{
	GSList *response = NULL;
	gboolean ok = wni_request_nyms("zzzqwertynonexistentxyz", &response, WORDNET_INTERFACE_OVERVIEW, FALSE);
	g_assert_false(ok);
}

static void test_null_search_returns_false(void)
{
	GSList *response = NULL;
	gboolean ok = wni_request_nyms(NULL, &response, WORDNET_INTERFACE_OVERVIEW, FALSE);
	g_assert_false(ok);
	g_assert_null(response);
}

static void test_futile_search_returns_false(void)
{
	GSList *response = NULL;
	gboolean ok = wni_request_nyms(".", &response, WORDNET_INTERFACE_OVERVIEW, FALSE);
	g_assert_false(ok);
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/artha/integration/wni_request_nyms/known_word",
	                test_known_word_returns_true);
	g_test_add_func("/artha/integration/wni_request_nyms/unknown_word",
	                test_unknown_word_returns_false);
	g_test_add_func("/artha/integration/wni_request_nyms/null_search",
	                test_null_search_returns_false);
	g_test_add_func("/artha/integration/wni_request_nyms/futile_search",
	                test_futile_search_returns_false);

	return g_test_run();
}
