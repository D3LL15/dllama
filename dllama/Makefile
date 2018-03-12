
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

SHARED_CLASSES := dllama_test.cpp dllama_test.h graph_database.cpp graph_database.h dllama.cpp dllama.h simple_dllama.cpp simple_dllama.h llama_for_benchmark.cpp llama_for_benchmark.h snapshot_merger.cpp snapshot_merger.h shared_thread_state.cpp shared_thread_state.h snapshot_manager.cpp snapshot_manager.h
SHARED_BODIES := dllama_test.cpp graph_database.cpp dllama.cpp simple_dllama.cpp llama_for_benchmark.cpp snapshot_merger.cpp shared_thread_state.cpp snapshot_manager.cpp

.PHONY: all clean

all: dllama.exe simple_dllama.exe llama.exe dllamatest.exe

clean:
	rm -f *.exe

dllama.exe : main.cpp $(SHARED_CLASSES)
	$(CC) $(CFLAGS) -DBENCHMARKING_FLAG main.cpp $(SHARED_BODIES) $(LFLAGS) -o dllama.exe

simple_dllama.exe : main.cpp $(SHARED_CLASSES)
	$(CC) $(CFLAGS) -DSIMPLE_DLLAMA -DBENCHMARKING_FLAG main.cpp $(SHARED_BODIES) $(LFLAGS) -o simple_dllama.exe

dllamatest.exe : dllamatest.cpp $(SHARED_CLASSES)
	$(CC) $(CFLAGS) -Igoogletestmaster/googletest/include dllamatest.cpp $(SHARED_BODIES) -Lgoogletestmaster/googlemock/gtest/ -lgtest $(LFLAGS) -o dllamatest.exe

llama.exe : main.cpp $(SHARED_CLASSES)
	$(CC) $(CFLAGS) -DLLAMA_BENCHMARK -DBENCHMARKING_FLAG main.cpp $(SHARED_BODIES) $(LFLAGS) -o llama.exe
