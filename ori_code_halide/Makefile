program_NAME := halide_tutorial_1
program_C_SRCS := $(wildcard *.c) $(wildcard ./deps/*.c)
program_CXX_SRCS := $(wildcard *.cpp) $(wildcard ./deps/*.cpp)
program_C_OBJS := ${program_C_SRCS:.c=.o}
program_CXX_OBJS := ${program_CXX_SRCS:.cpp=.o}
program_OBJS := $(program_C_OBJS) $(program_CXX_OBJS)
program_INCLUDE_DIRS := /home/sander/Halide/include /home/sander/Halide/tools
program_LIBRARY_DIRS := /home/sander/Halide/bin
program_LIBRARIES := Halide pthread dl z

CPPFLAGS += $(foreach includedir,$(program_INCLUDE_DIRS),-I$(includedir)) -g -std=c++11
LDFLAGS += $(foreach librarydir,$(program_LIBRARY_DIRS),-L$(librarydir)) -g
LDFLAGS += $(foreach library,$(program_LIBRARIES),-l$(library))

.PHONY: all clean distclean

all: $(program_NAME)

$(program_NAME): $(program_OBJS)
	g++ $(program_OBJS) $(LDFLAGS) -o $(program_NAME)

clean:
	@- $(RM) $(program_NAME)
	@- $(RM) $(program_OBJS)

distclean: clean

run: 
	LD_LIBRARY_PATH=~/Halide/bin ./$(program_NAME)
