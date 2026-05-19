thread_safe=1
production=0

include ../../rules.mk
VPATH=../../lib
CFLAGS+=-I../../lib

DEPS=mls.od m_tool.od m_table.od
TARGETS=test_thread_safe_fuzzy.exed test_thread_safe_strings.exed

ALL: $(TARGETS)

test_thread_safe_fuzzy.exed: test_thread_safe_fuzzy.od $(DEPS)
test_thread_safe_strings.exed: test_thread_safe_strings.od $(DEPS)

clean:
	-${RM} -f *.exed *.od
