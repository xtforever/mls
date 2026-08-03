thread_safe=1
production=0

include ../../rules.mk
VPATH=../../lib
CFLAGS+=-I../../lib

DEPS=mls.od m_tool.od m_table.od
TARGET=rootinfo.exed

$(TARGET): $(DEPS)
ALL: $(TARGET)

check: ALL
	./$(TARGET) > /dev/null

clean:
	-${RM} -f $(TARGET) $(DEPS)
