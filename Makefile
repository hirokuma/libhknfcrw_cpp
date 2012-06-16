#!/bin/sh

TARGET = libhknfcrw.a

SRCDIR = ./src
OBJDIR = ./obj
INCDIR = ./inc

SRCS = \
	misc.cpp \
	devaccess_uart.cpp \
	HkNfcA.cpp \
	HkNfcB.cpp \
	HkNfcF.cpp \
	HkNfcDep.cpp \
	NfcPcd.cpp \
	HkNfcRw.cpp

OBJS = $(addprefix $(OBJDIR)/, $(SRCS:.cpp=.o))
SRCP = $(addprefix $(SRCDIR)/, $(SRCS))
vpath %.cpp $(SRCDIR)


CFLAGS = -I$(INCDIR) -Wextra -O3
LDOPT  = 

# rules

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) r $(TARGET) $(OBJS)

$(OBJDIR)/%.o : %.cpp
	$(CXX) -o $@ -c $(CFLAGS) $<

clean:
	$(RM) $(OBJDIR)/*.o $(TARGET) .Depend

.Depend:
	-$(CXX) $(CFLAGS) -MM $(SRCP) > .tmp
	@if [ ! -d $(OBJDIR) ]; then \
		echo ";; mkdir $(OBJDIR)"; mkdir $(OBJDIR); \
	fi
	sed -e '/^[^ ]/s,^,$(OBJDIR)/,' .tmp> .Depend
	rm .tmp

include .Depend
