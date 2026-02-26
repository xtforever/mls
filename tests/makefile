include ../rules.mk

TARGETS=test_autofree test_ssplit test_nested_lists
DEPS=mls.o${OBJ} m_tool.o${OBJ} 
EXE_TARGETS := $(patsubst %,%.exe${OBJ},$(TARGETS))

ALL: $(EXE_TARGETS) run

test_autofree.exe${OBJ}: test_autofree.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_ssplit.exe${OBJ}: test_ssplit.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_nested_lists.exe${OBJ}: test_nested_lists.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

run: $(EXE_TARGETS)
	@for exe in $(EXE_TARGETS); do \
		echo "Running $$exe..."; \
		./$$exe || exit 1; \
	done

check-leaks: $(EXE_TARGETS)
	@for exe in $(EXE_TARGETS); do \
		echo "Checking leaks in $$exe..."; \
		ASAN_OPTIONS=detect_leaks=1 ./$$exe || exit 1; \
	done

clean:
	-${RM} $(EXE_TARGETS) *.o *.od *~ *.lex.* *.tab.*

extra: clean ALL
