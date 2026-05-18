thread_safe=1
production=0

include ../../rules.mk
VPATH=../../lib
CFLAGS+=-I../../lib

DEPS=mls.od m_tool.od m_table.od
TARGET=dirwalker.exed


$(TARGET): $(DEPS)
ALL: $(TARGET)

clean:
	-${RM} -f $(TARGET) $(DEPS)
