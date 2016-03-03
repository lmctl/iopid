PROGRAM :=	iopid
OBJS :=		$(patsubst %.c,%.o,$(wildcard *.c))
CFLAGS :=	-Wextra -Werror

all:		$(PROGRAM)

$(PROGRAM):	$(OBJS)
		$(CC) -o $@ $(OBJS) $(LDLAGS)

.PHONY: clean
clean:		
		rm -f $(OBJS) $(PROGRAM)

.PHONY: clobber
clobber:	clean
		rm -f *~

