all: bin/OoOTraceSimulator bin/Debug.OoOTraceSimulator bin/Prof.OoOTraceSimulator
debug: bin/Debug.OoOTraceSimulator

CPPFLAGS = -O3 -lm -ldramsim -DNDEBUG -DDRAMSIM -I/home/abhowmic/DRAMSim2/ -L/home/abhowmic/DRAMSim2/ -Wl,-rpath=/home/abhowmic/DRAMSim2/
DEBUGFLAGS = -lm -g -ldramsim -DDRAMSIM -I/home/abhowmic/DRAMSim2/ -L/home/abhowmic/DRAMSim2/ -Wl,-rpath=/home/abhowmic/DRAMSim2/
PROFFLAGS = -lm -pg -ldramsim -DDRAMSIM -I/home/abhowmic/DRAMSim2/ -L/home/abhowmic/DRAMSim2/ -Wl,-rpath=/home/abhowmic/DRAMSim2/
SRCS = ComponentList.cc
HEADERS = $(wildcard *.h)

bin/OoOTraceSimulator: OoOTraceSimulator.cc $(SRCS) $(HEADERS) Makefile
	g++ $(CPPFLAGS) $< $(SRCS) -lz -o $@ 

bin/Debug.OoOTraceSimulator: OoOTraceSimulator.cc $(SRCS) $(HEADERS) Makefile
	g++ $(DEBUGFLAGS) $< -lz $(SRCS) -lz -o $@ 

bin/Prof.OoOTraceSimulator: OoOTraceSimulator.cc $(SRCS) $(HEADERS) Makefile
	g++ $(PROFFLAGS) $< $(SRCS) -lz -o $@ 

clean:
	rm -f bin/Debug.OoOTraceSimulator bin/OoOTraceSimulator bin/Prof.OoOTraceSimulator
