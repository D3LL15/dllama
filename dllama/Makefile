#
# LLAMA Examples
#

CC         := mpicxx
CFLAGS     := -cxx=g++ -Wall -fpermissive -fopenmp -std=gnu++11 -DLL_PERSISTENCE -I. -I../llama/include
LFLAGS     := 

ifeq ($(VIMRUNTIME),)
ifneq ($(wildcard /usr/bin/color-g++),)
CC         := color-g++
endif
endif

CFLAGS_D   := -ggdb -O0 -D_DNDEBUG -DDEBUG_ENABLED ${CFLAGS}
CFLAGS     := -g -O3 -DNDEBUG ${CFLAGS}

DIR        := ../bin
SOURCES    := $(wildcard *.cc) 
TARGETS    := $(patsubst %.cc,$(DIR)/%,$(SOURCES)) 

SHARED_CLASSES := main.cpp dllama_test.cpp dllama_test.h graph_database.h snapshot_merger.cpp snapshot_merger.h shared_thread_state.cpp shared_thread_state.h snapshot_manager.cpp snapshot_manager.h

.PHONY: all clean

all: $(TARGETS)  dllama.exe simple_dllama.exe llama.exe
	#dllamatest.exe

clean:
	rm -f $(TARGETS)
	rm -f *.exe

$(DIR)/%: %.cc
	@echo CC $@ >&2
	$(CC) $(CFLAGS) -o $@ $< $(LFLAGS)
dllama.exe : main.cpp dllama_test.cpp dllama_test.h dllama.cpp dllama.h graph_database.cpp graph_database.h snapshot_merger.cpp snapshot_merger.h shared_thread_state.cpp shared_thread_state.h snapshot_manager.cpp snapshot_manager.h
	$(CC) $(CFLAGS) main.cpp dllama_test.cpp dllama.cpp graph_database.cpp snapshot_merger.cpp shared_thread_state.cpp snapshot_manager.cpp $(LFLAGS) -o dllama.exe

simple_dllama.exe : main.cpp dllama_test.cpp dllama_test.h simple_dllama.cpp simple_dllama.h graph_database.cpp graph_database.h snapshot_merger.cpp snapshot_merger.h shared_thread_state.cpp shared_thread_state.h snapshot_manager.cpp snapshot_manager.h 
	$(CC) $(CFLAGS) -DSIMPLE_DLLAMA main.cpp dllama_test.cpp simple_dllama.cpp graph_database.cpp snapshot_merger.cpp shared_thread_state.cpp snapshot_manager.cpp $(LFLAGS) -o simple_dllama.exe

dllamatest.exe : dllamatest.cpp dllama_test.cpp dllama_test.h dllama.cpp dllama.h graph_database.cpp graph_database.h snapshot_merger.cpp snapshot_merger.h shared_thread_state.cpp shared_thread_state.h snapshot_manager.cpp snapshot_manager.h
	$(CC) $(CFLAGS) -Igoogletestmaster/googletest/include dllamatest.cpp graph_database.cpp dllama_test.cpp dllama.cpp snapshot_merger.cpp shared_thread_state.cpp snapshot_manager.cpp -Lgoogletestmaster/googlemock/gtest/ -lgtest $(LFLAGS) -o dllamatest.exe

llama.exe : main.cpp dllama_test.cpp dllama_test.h llama_for_benchmark.cpp dllama.h graph_database.cpp graph_database.h snapshot_merger.cpp snapshot_merger.h shared_thread_state.cpp shared_thread_state.h snapshot_manager.cpp snapshot_manager.h 
	$(CC) $(CFLAGS) -DLLAMA_BENCHMARK main.cpp dllama_test.cpp llama_for_benchmark.cpp graph_database.cpp snapshot_merger.cpp shared_thread_state.cpp snapshot_manager.cpp $(LFLAGS) -o llama.exe
