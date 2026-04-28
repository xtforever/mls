production=0
TAG="\"DEBUG_$(shell date)\""
CFLAGS+=-g -Wall -DMLS_DEBUG  -fstack-protector-all -fsanitize=address
DEPS=mls.od
all: mlscoretest.exed

%.exed: %.od $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.od: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-${RM} *.exed *.od



