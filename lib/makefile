production=0
TAG="\"DEBUG_$(shell date)\""
CFLAGS_DEB=-g -Wall -DMLS_DEBUG  -fstack-protector-all -fsanitize=address
DEPS=mls.od
all: mlscoretest.exed

%.exed: %.od $(DEPS)
	$(CC) $(CFLAGS_DEB) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.od: %.c
	$(CC) $(CFLAGS_DEB) -c -o $@ $<

clean:
	-${RM} *.exed *.od



