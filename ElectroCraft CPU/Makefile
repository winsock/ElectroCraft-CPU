CC = g++
JDK_INCLUDE = /Library/Java/JavaVirtualMachines/1.7.0.jdk/Contents/Home/include
CPP_FILES = $(wildcard JNI/*.cpp) *.cpp
OBJ_FILES = $(patsubst src/%.cpp,obj/%.o,$(CPP_FILES))
LD_FLAGS = -std=c++11 -D_GLIBCXX_USE_NANOSLEEP -lpthread -shared -fPIC -I$(JDK_INCLUDE)
CC_FLAGS = --export-dynamic

all: libElectroCraftCPU.so

libElectroCraftCPU.so: $(OBJ_FILES)
	$(CC) $(LD_FLAGS) -o $@ $^

obj/%.o: JNI/%.cpp %.cpp
	$(CC) $(CC_FLAGS) -c -o $@ $<
	
clean:
	rm libElectroCraftCPU.so