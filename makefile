############################################################
#
#  Project:        quTAG User Library
#
#  Filename:       Makefile
#
#  Purpose:        GCC makefile for examples
#
#  Author:         N-Hands GmbH & Co KG
#
############################################################
# $Id: makefile.lx,v 1.1 2017/12/14 12:50:37 trurl Exp $

LIBDIR=../lib
INCDIR=../inc

#TARGET=example0 example5 fqnet_example

#all : $(TARGET)

#define outputrule
#$(1): $(1:%=%.c)
#	$(CXX) -o $$@ -g -Wall -O0 -I$(INCDIR) -L$(LIBDIR) $$< -ltdcbase
#endef
#$(foreach src,$(TARGET),$(eval $(call outputrule,$(src))))

CXX = gcc
INC = $(shell pwd)
LD = c++
CPPFLAGS := -pthread -std=c++11 -m64 -I$(INC)/../inc
LDFLAGS := -L../lib -ltdcbase -lftd3xx 

CPPFLAGS += -g 

TARGET3 = FQNETDAQ

SRC3 = FQNETDAQ.c


OBJ3 = $(SRC3:.c=.o)

all : $(TARGET3) 


$(TARGET3) : $(OBJ3)
	$(LD) $(CPPFLAGS) -o $(TARGET3) $(OBJ3) $(LDFLAGS)
	@echo $@
	@echo $<
	@echo $^

%.o : %.c
	$(CXX) $(CPPFLAGS) -o $@ -c $<
	@echo $@
	@echo $<


clean : 
	rm -f *.o $(TARGET) $(TARGET1) $(TARGET2) $(TARGET3)

