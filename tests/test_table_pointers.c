#include "mls.h"
#include "m_table.h"
#include "m_tool.h"
#undef ASSERT
#include "greatest.h"

SUITE (m_table_pointers_suite);

TEST test_table_uint64_values (void)
{
	int t = m_table_create();
	
	/* Test large integers */
	int64_t large_val = 0x123456789ABCDEF0LL;
	mt_seti(t, "large", large_val);
	ASSERT_EQ(large_val, (int64_t)mt_get(t, "large"));
	
	/* Test handles (still work) */
	int s = s_dup("TableValue");
	mt_seth(t, "handle", s, MLS_TABLE_TYPE_STRING);
	ASSERT_EQ(s, (int)mt_get(t, "handle"));
	
	m_table_free(t);
	PASS();
}

TEST test_table_ptr_functions (void)
{
	int t = m_table_create();
	
	char *my_ptr = malloc(100);
	strcpy(my_ptr, "PointerData");
	
	mt_setp(t, "ptr", my_ptr);
	void *ret_ptr = mt_getp(t, "ptr");
	
	ASSERT_EQ(my_ptr, ret_ptr);
	ASSERT_STR_EQ("PointerData", (char *)ret_ptr);
	
	/* Introspection */
	ASSERT_EQ(MLS_TABLE_TYPE_PTR, m_table_get_type_cstr(t, "ptr"));
	
	/* Free table - should NOT free our raw pointer */
	m_table_free(t);
	
	/* Access pointer after table free */
	ASSERT_STR_EQ("PointerData", my_ptr);
	free(my_ptr);
	
	PASS();
}

TEST test_table_mixed_types (void)
{
	int t = m_table_create();
	
	mt_seti(t, "int", 42);
	mt_sets(t, "str", "dynamic");
	
	char dummy;
	mt_setp(t, "ptr", &dummy);
	
	ASSERT_EQ(42, (int)mt_get(t, "int"));
	ASSERT_STR_EQ("dynamic", m_str((int)mt_get(t, "str")));
	ASSERT_EQ(&dummy, mt_getp(t, "ptr"));
	
	m_table_free(t);
	PASS();
}

GREATEST_SUITE (m_table_pointers_suite)
{
	RUN_TEST (test_table_uint64_values);
	RUN_TEST (test_table_ptr_functions);
	RUN_TEST (test_table_mixed_types);
}

GREATEST_MAIN_DEFS ();

int main (int argc, char **argv)
{
	GREATEST_MAIN_BEGIN ();
	m_init ();
	RUN_SUITE (m_table_pointers_suite);
	GREATEST_MAIN_END ();
}
