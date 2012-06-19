#!/bin/sh

TARGET = tst

SRCDIR = .
OBJDIR = ./obj
INCDIR = ../inc


SRCS = \
	deptest.cpp \
	main.cpp

OBJS = $(addprefix $(OBJDIR)/, $(SRCS:.cpp=.o))
SRCP = $(addprefix $(SRCDIR)/, $(SRCS))
vpath %.cpp $(SRCDIR)


CFLAGS = -I$(INCDIR) -Wextra -O3
LDOPT  = -L.. -lhknfcrw

# rules

all: $(TARGET)

$(TARGET): $(OBJS) ../libhknfcrw.a
	$(CXX) $(OBJS) -o $(TARGET) $(LDOPT)

$(OBJDIR)/%.o : %.cpp
	$(CXX) -o $@ -c $(CFLAGS) $<

clean:
	$(RM) $(OBJDIR)/*.o $(TARGET) $(TARGET).exe

