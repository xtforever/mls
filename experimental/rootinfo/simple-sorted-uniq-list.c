
// keep a list of mls-strings  sorted, keys are uniq
int add_uniq (int list, const char *s )
{
        if (!s || !*s)
                return -1;
        // mscmpc - compare c-string with mls-string
        int p = m_binsert(list, s, mscmpc, 0);
        if (p < 0) { // found
                return (-p) - 1;
        }
	// not found, insert at returned position
        INT(list,p) = s_dup(s);
}
