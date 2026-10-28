include ../rules.mk

TARGETS=test_mls_core test_autofree test_ssplit test_nested_lists test_circular_marker test_slice test_table test_memory_advanced test_http test_http_improved test_http_hostile http_server http_client test_hdf hdf_http_server test_flask test_flask_advanced test_extra test_m_var test_sql memcached_server test_string_robustness test_splice test_m_subproc test_read_fields test_implode test_arena test_carray test_darray test_strbuf test_htable test_hashmap test_token test_bptree test_pager test_m_strpool test_m_forth test_m_isam test_m_dbwords test_m_fproto test_m_engine test_m_fapi test_m_ftransport test_m_fserver test_m_fclient
ifeq ($(thread_safe),1)
TARGETS+=test_thread_safe_fuzzy
TARGETS+=test_thread_safety
TARGETS+=test_tsan_races
endif
DEPS=../lib/mls_base.o${OBJ} ../lib/mls_ext.o${OBJ} ../lib/m_tool.o${OBJ} ../lib/m_var.o${OBJ} ../lib/m_sql.o${OBJ} ../lib/list.o${OBJ} ../lib/table.o${OBJ} ../lib/m_http.o${OBJ} ../lib/m_hdf.o${OBJ} ../lib/m_http_server.o${OBJ} ../lib/m_flask.o${OBJ} ../lib/m_extra.o${OBJ}

EXE_TARGETS := $(patsubst %,%.exe${OBJ},$(TARGETS))

ALL: $(EXE_TARGETS) error-handler

memcached_server.exed: memcached_server.od $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_arena.exe${OBJ}: test_arena.o${OBJ} ../lib/arena.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_carray.exe${OBJ}: test_carray.o${OBJ} ../lib/carray.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_darray.exe${OBJ}: test_darray.o${OBJ} ../lib/darray.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_strbuf.exe${OBJ}: test_strbuf.o${OBJ} ../lib/strbuf.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_htable.exe${OBJ}: test_htable.o${OBJ} ../lib/htable.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_hashmap.exe${OBJ}: test_hashmap.o${OBJ} ../lib/hashmap.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_token.exe${OBJ}: test_token.o${OBJ} ../lib/token.o${OBJ} ../lib/strbuf.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_bptree.exe${OBJ}: test_bptree.o${OBJ} ../lib/bptree.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_pager.exe${OBJ}: test_pager.o${OBJ} ../lib/pager.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_strpool.exe${OBJ}: test_m_strpool.o${OBJ} ../lib/m_strpool.o${OBJ} ../lib/hashmap.o${OBJ} ../lib/darray.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_forth.exe${OBJ}: test_m_forth.o${OBJ} ../lib/m_forth.o${OBJ} \
	../lib/arena.o${OBJ} ../lib/carray.o${OBJ} ../lib/darray.o${OBJ} \
	../lib/strbuf.o${OBJ} ../lib/htable.o${OBJ} ../lib/hashmap.o${OBJ} \
	../lib/token.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_isam.exe${OBJ}: test_m_isam.o${OBJ} ../lib/m_isam.o${OBJ} \
	../lib/bptree.o${OBJ} ../lib/darray.o${OBJ} ../lib/hashmap.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_dbwords.exe${OBJ}: test_m_dbwords.o${OBJ} ../lib/m_dbwords.o${OBJ} \
	../lib/m_forth.o${OBJ} ../lib/m_isam.o${OBJ} ../lib/bptree.o${OBJ} \
	../lib/arena.o${OBJ} ../lib/carray.o${OBJ} ../lib/darray.o${OBJ} \
	../lib/strbuf.o${OBJ} ../lib/htable.o${OBJ} ../lib/hashmap.o${OBJ} \
	../lib/token.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_fproto.exe${OBJ}: test_m_fproto.o${OBJ} ../lib/m_fproto.o${OBJ} \
	../lib/strbuf.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_engine.exe${OBJ}: test_m_engine.o${OBJ} ../lib/m_engine.o${OBJ} \
	../lib/m_dbwords.o${OBJ} ../lib/m_forth.o${OBJ} ../lib/m_isam.o${OBJ} \
	../lib/m_fproto.o${OBJ} ../lib/bptree.o${OBJ} ../lib/arena.o${OBJ} \
	../lib/carray.o${OBJ} ../lib/darray.o${OBJ} ../lib/strbuf.o${OBJ} \
	../lib/htable.o${OBJ} ../lib/hashmap.o${OBJ} ../lib/token.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_fapi.exe${OBJ}: test_m_fapi.o${OBJ} ../lib/m_fapi.o${OBJ} \
	../lib/m_fclient.o${OBJ} ../lib/m_ftransport.o${OBJ} \
	../lib/m_engine.o${OBJ} ../lib/m_dbwords.o${OBJ} ../lib/m_forth.o${OBJ} \
	../lib/m_isam.o${OBJ} ../lib/m_fproto.o${OBJ} ../lib/bptree.o${OBJ} \
	../lib/arena.o${OBJ} ../lib/carray.o${OBJ} ../lib/darray.o${OBJ} \
	../lib/strbuf.o${OBJ} ../lib/htable.o${OBJ} ../lib/hashmap.o${OBJ} \
	../lib/token.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_ftransport.exe${OBJ}: test_m_ftransport.o${OBJ} \
	../lib/m_ftransport.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_fserver.exe${OBJ}: test_m_fserver.o${OBJ} ../lib/m_fserver.o${OBJ} \
	../lib/m_engine.o${OBJ} ../lib/m_dbwords.o${OBJ} ../lib/m_forth.o${OBJ} \
	../lib/m_isam.o${OBJ} ../lib/m_fproto.o${OBJ} ../lib/m_ftransport.o${OBJ} \
	../lib/bptree.o${OBJ} ../lib/arena.o${OBJ} ../lib/carray.o${OBJ} \
	../lib/darray.o${OBJ} ../lib/strbuf.o${OBJ} ../lib/htable.o${OBJ} \
	../lib/hashmap.o${OBJ} ../lib/token.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_fclient.exe${OBJ}: test_m_fclient.o${OBJ} ../lib/m_fclient.o${OBJ} \
	../lib/m_fserver.o${OBJ} ../lib/m_engine.o${OBJ} ../lib/m_dbwords.o${OBJ} \
	../lib/m_forth.o${OBJ} ../lib/m_isam.o${OBJ} ../lib/m_fproto.o${OBJ} \
	../lib/m_ftransport.o${OBJ} ../lib/bptree.o${OBJ} ../lib/arena.o${OBJ} \
	../lib/carray.o${OBJ} ../lib/darray.o${OBJ} ../lib/strbuf.o${OBJ} \
	../lib/htable.o${OBJ} ../lib/hashmap.o${OBJ} ../lib/token.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_string_robustness.exe${OBJ}: test_string_robustness.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_string.exe${OBJ}: test_m_string.o${OBJ} $(DEPS)

	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_extra.exe${OBJ}: test_extra.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_var.exe${OBJ}: test_m_var.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_sql.exe${OBJ}: test_sql.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_mls_core.exe${OBJ}: test_mls_core.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_thread_safe_fuzzy.exe${OBJ}: test_thread_safe_fuzzy.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_thread_safety.exe${OBJ}: test_thread_safety.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_tsan_races.exe${OBJ}: test_tsan_races.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_m_subproc.exe${OBJ}: test_m_subproc.o${OBJ} $(DEPS) ../lib/m_subproc.o${OBJ}
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_read_fields.exe${OBJ}: test_read_fields.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_implode.exe${OBJ}: test_implode.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_autofree.exe${OBJ}: test_autofree.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

error-handler: error-handler.c mls_base.c mls_base.h mls_ext.c mls_ext.h m_tool.c m_tool.h list.h list.c table.h table.c
	gcc -g -o error-handler -DMLS_DEBUG -DMLS_THREAD_SAFE=0 error-handler.c ../lib/mls_base.c ../lib/mls_ext.c ../lib/m_tool.c ../lib/list.c ../lib/table.c -I ../lib

test_ssplit.exe${OBJ}: test_ssplit.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_nested_lists.exe${OBJ}: test_nested_lists.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_circular_marker.exe${OBJ}: test_circular_marker.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_slice.exe${OBJ}: test_slice.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_splice.exe${OBJ}: test_splice.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_table.exe${OBJ}: test_table.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_slice_advanced.exe${OBJ}: test_slice_advanced.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_memory_advanced.exe${OBJ}: test_memory_advanced.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_http.exe${OBJ}: test_http.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_http_improved.exe${OBJ}: test_http_improved.o${OBJ} $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_http_hostile.exe${OBJ}: test_http_hostile.o${OBJ} $(DEPS)
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

RUN_TARGETS := $(filter-out http_server.exe${OBJ} hdf_http_server.exe${OBJ} test_flask.exe${OBJ} memcached_server.exe${OBJ}, $(EXE_TARGETS))

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

# Single-header amalgam (mls_core.h): 4 build configs x selectable trace_level
check-amalgam:
	bash test_mls_core_amalgam.sh

# ThreadSanitizer build — catches data races
check-tsan:
	$(MAKE) thread_safe=1 CFLAGS="-g -fsanitize=thread -DMLS_THREAD_SAFE -D_GNU_SOURCE -I../lib" \
		LDLIBS="-lpthread -fsanitize=thread -ldl -lm" \
		clean ALL run

# Stress test — high-iteration fuzzy test (override ITERS via FUZZ_ITERS)
check-stress:
	@echo "Running fuzzy stress test with FUZZ_ITERS=500000 ..."
	FUZZ_ITERS=500000 ./test_thread_safe_fuzzy.exe${OBJ}
	@echo "Running thread safety stress test (high iterations) ..."
	@echo "(increase via 'make check-stress STRESS_ITERS=100000')"
	@./test_thread_safety.exe${OBJ}

clean:
	-${RM} $(EXE_TARGETS) *.o *.od *~ *.lex.* *.tab.* ../lib/*.od ../lib/*.o error-handler


extra: clean ALL
