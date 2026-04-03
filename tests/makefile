include ../rules.mk

TARGETS=test_mls_core test_autofree test_ssplit test_nested_lists test_circular_marker test_slice test_table  test_table_advanced test_memory_advanced test_http test_http_improved http_server http_client test_hdf hdf_http_server test_flask test_flask_advanced test_extra test_se_string memcached_server test_string_robustness test_table_pointers
DEPS=../lib/mls.o${OBJ} ../lib/m_tool.o${OBJ} ../lib/m_table.o${OBJ} ../lib/m_http.o${OBJ} ../lib/m_hdf.o${OBJ} ../lib/m_http_server.o${OBJ} ../lib/m_flask.o${OBJ} ../lib/m_extra.o${OBJ}
EXE_TARGETS := $(patsubst %,%.exe${OBJ},$(TARGETS))

ALL: $(EXE_TARGETS) error-handler

test_table_pointers.exe${OBJ}: test_table_pointers.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

memcached_server.exed: memcached_server.od $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_string_robustness.exe${OBJ}: test_string_robustness.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_string.exe${OBJ}: test_m_string.o${OBJ} $(DEPS)

	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_extra.exe${OBJ}: test_extra.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_se_string.exe${OBJ}: test_se_string.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_mls_core.exe${OBJ}: test_mls_core.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_autofree.exe${OBJ}: test_autofree.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

error-handler: error-handler.c mls.c mls.h m_tool.c  m_tool.h m_table.h m_table.c
	gcc -g -o error-handler -DMLS_DEBUG error-handler.c ../lib/mls.c ../lib/m_tool.c ../lib/m_table.c -I ../lib

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

test_http_improved.exe${OBJ}: test_http_improved.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

http_server.exe${OBJ}: http_server.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

http_client.exe${OBJ}: http_client.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_hdf.exe${OBJ}: test_hdf.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

hdf_http_server.exe${OBJ}: hdf_http_server.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_flask.exe${OBJ}: test_flask.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_flask_advanced.exe${OBJ}: test_flask_advanced.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Compile lib sources to .od files
../lib/%.od: ../lib/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

RUN_TARGETS := $(filter-out http_server.exe${OBJ} hdf_http_server.exe${OBJ} test_flask.exe${OBJ}, $(EXE_TARGETS))

run: $(EXE_TARGETS)
	@for exe in $(RUN_TARGETS); do \
		echo "Running $$exe..."; \
		./$$exe || exit 1; \
	done

check-leaks: $(EXE_TARGETS)
	@for exe in $(RUN_TARGETS); do \
		echo "Checking leaks in $$exe..."; \
		ASAN_OPTIONS=detect_leaks=1 ./$$exe || exit 1; \
	done

clean:
	-${RM} $(EXE_TARGETS) *.o *.od *~ *.lex.* *.tab.* ../lib/*.od ../lib/*.o error-handler


extra: clean ALL
