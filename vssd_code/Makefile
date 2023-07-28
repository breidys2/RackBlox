#############################################################################
#
# Makefile 
#
#===========================================================================

# Excecutable file.
PROGRAM   = cnexcmd 

# Parameters .
CC      = gcc
CFLAGS  = -O3 -Wall -pthread 
#CFLAGS  = -g -Wall -pthread
RM      = rm -f
COMPILE = $(CC) $(CFLAGS) -c
OBJS    = $(addsuffix .o, $(basename $(wildcard *.c)))

# Make goes here.
all: $(PROGRAM)
	@$(RM) $(OBJS)

clean:
	$(RM) $(OBJS) $(PROGRAM)

show:
	@echo 'PROGRAM     :' $(PROGRAM)
	@echo 'SRCS        :' $(wildcard *.c)
	@echo 'OBJS        :' $(OBJS)
	@echo 'LINK.c      :' $(LINK.c)
	
# Object files.
objs:$(OBJS)
$(PROGRAM):$(OBJS)
	$(LINK.c)  $(OBJS)   -o $@
%.o:%.c
	@$(COMPILE) $< -o $@
