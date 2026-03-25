include ../rules.mk

TARGETS=test_autofree test_ssplit test_nested_lists test_circular_marker test_slice test_table test_slice_advanced test_table_advanced test_memory_advanced test_http
DEPS=mls.o${OBJ} m_tool.o${OBJ} m_table.o${OBJ} m_http.o${OBJ}
EXE_TARGETS := $(patsubst %,%.exe${OBJ},$(TARGETS))

ALL: $(EXE_TARGETS) run

test_autofree.exe${OBJ}: test_autofree.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_ssplit.exe${OBJ}: test_ssplit.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_nested_lists.exe${OBJ}: test_nested_lists.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_circular_marker.exe${OBJ}: test_circular_marker.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_slice.exe${OBJ}: test_slice.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_table.exe${OBJ}: test_table.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_slice_advanced.exe${OBJ}: test_slice_advanced.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_table_advanced.exe${OBJ}: test_table_advanced.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_memory_advanced.exe${OBJ}: test_memory_advanced.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_http.exe${OBJ}: test_http.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

m_http.o${OBJ}: ../lib/m_http.c ../lib/m_http.h
	$(CC) $(CFLAGS) -c -o $@ $<

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
