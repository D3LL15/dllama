#
# LLAMA Examples
#

CC         := mpicxx
CFLAGS     := -Wall -fopenmp -std=gnu++11 -DLL_PERSISTENCE -I. -I../llama/include
LFLAGS     := 

ifeq ($(VIMRUNTIME),)
ifneq ($(wildcard /usr/bin/color-g++),)
CC         := color-g++
endif
endif

CFLAGS_D   := -ggdb -O0 -D_DEBUG ${CFLAGS}
CFLAGS     := -g -O3 -DNDEBUG ${CFLAGS}

DIR        := ../bin
SOURCES    := $(wildcard *.cc) 
TARGETS    := $(patsubst %.cc,$(DIR)/%,$(SOURCES)) 

.PHONY: all clean

all: $(TARGETS)  dllama.exe

clean:
	rm -f $(TARGETS)

$(DIR)/%: %.cc
	@echo CC $@ >&2
	$(CC) $(CFLAGS) -o $@ $< $(LFLAGS)
dllama.exe : main.cpp dllama_test.cpp dllama_test.h dllama.cpp dllama.h	snapshot_merger.cpp snapshot_merger.h shared_thread_state.h
	$(CC) $(CFLAGS) main.cpp dllama_test.cpp dllama.cpp snapshot_merger.cpp $(LFLAGS) -o dllama.exe
