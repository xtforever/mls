thread_safe=1
production=0

include ../../rules.mk
VPATH=../../lib:.:gather:out

CFLAGS+=-I../../lib -I.

DEPS=mls.od m_tool.od m_table.od m_hdf.od m_subproc.od m_extra.od \
     cfg.od gather/sec_system.od gather/sec_lvm.od gather/sec_zfs.od \
     gather/sec_ports.od gather/sec_proc.od gather/sec_cron.od \
     gather/sec_firewall.od gather/sec_stack.od out/out_term.od
TARGET=rootinfo.exed

$(TARGET): $(DEPS)
ALL: $(TARGET)

check: ALL
	./$(TARGET) > /dev/null

clean:
	-${RM} -f $(TARGET) *.od
