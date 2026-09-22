thread_safe ?= 1


include ../../rules.mk
VPATH=../../lib:.:gather:out

CFLAGS+=-I../../lib -I.

DEPS=mls_base.o mls_ext.o m_tool.o table.o list.o m_hdf.o m_subproc.o m_extra.o \
     cfg.o gather/sec_system.o gather/sec_lvm.o gather/sec_zfs.o \
     gather/sec_ports.o gather/sec_proc.o gather/sec_cron.o \
     gather/sec_firewall.o gather/sec_stack.o gather/sec_docker.o \
     gather/sec_health.o gather/sec_logs.o out/out_term.o

TARGET=rootinfo.exe
production  = 0
ifeq ($(production), 0)
DEPS := $(DEPS:.o=.od)
TARGET := $(TARGET:.exe=.exed)
endif


$(TARGET): $(DEPS)
ALL: $(TARGET)

check: ALL
	./$(TARGET) > /dev/null

clean:
	-${RM} -f $(TARGET) *.o *.od gather/*.o gather/*.od out/*.o out/*.od

