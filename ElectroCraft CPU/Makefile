CC = g++
JDK_INCLUDE = /Library/Java/JavaVirtualMachines/1.7.0.jdk/Contents/Home/include
CPP_FILES = $(wildcard src/*.cpp) *.cpp
OBJ_FILES = $(patsubst src/%.cpp,obj/%.o,$(CPP_FILES))
LD_FLAGS = -std=c++11 -I$(JDK_INCLUDE) -D_GLIBCXX_USE_NANOSLEEP -shared -fPIC
CC_FLAGS = -std=c++11 -lpthread -D_GLIBCXX_USE_NANOSLEEP

all: libElectroCraftCPU.so

libElectroCraftCPU.so: $(OBJ_FILES)
	g++ $(LD_FLAGS) -o $@ $^

obj/%.o: JNI/%cpp %cpp
	g++ $(CC_FLAGS) -c -o $@ $<