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

all: $(TARGETS)  dllama.exe simple_dllama.exe dllamatest.exe

clean:
	rm -f $(TARGETS)

$(DIR)/%: %.cc
	@echo CC $@ >&2
	$(CC) $(CFLAGS) -o $@ $< $(LFLAGS)
dllama.exe : main.cpp dllama_test.cpp dllama_test.h dllama.cpp dllama.h snapshot_merger.cpp snapshot_merger.h shared_thread_state.h snapshot_manager.cpp snapshot_manager.h
	$(CC) $(CFLAGS) main.cpp dllama_test.cpp dllama.cpp snapshot_merger.cpp snapshot_manager.cpp $(LFLAGS) -o dllama.exe
simple_dllama.exe : main.cpp dllama_test.cpp dllama_test.h simple_dllama.cpp dllama.h snapshot_merger.cpp snapshot_merger.h shared_thread_state.h snapshot_manager.cpp snapshot_manager.h
	$(CC) $(CFLAGS) main.cpp dllama_test.cpp simple_dllama.cpp snapshot_merger.cpp snapshot_manager.cpp $(LFLAGS) -o simple_dllama.exe
dllamatest.exe : dllamatest.cpp dllama_test.cpp dllama_test.h dllama.cpp dllama.h snapshot_merger.cpp snapshot_merger.h shared_thread_state.h snapshot_manager.cpp snapshot_manager.h
	$(CC) $(CFLAGS) -DDEBUG_ENABLED -Igoogletestmaster/googletest/include dllamatest.cpp dllama_test.cpp dllama.cpp snapshot_merger.cpp snapshot_manager.cpp -Lgoogletestmaster/googlemock/gtest/ -lgtest $(LFLAGS) -o dllamatest.exe
