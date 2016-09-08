CUR_DIR :=$(shell pwd)


UNAME := $(shell uname -sm)

# HOST_ARCH
ifneq (,$(findstring arm,$(UNAME)))
	HOST_ARCH := arm
endif

ifneq (,$(findstring 86,$(UNAME)))
	HOST_ARCH := x86
endif

ifeq ($(HOST_ARCH),)
$(error Unable to determine HOST_ARCH from uname -sm: $(UNAME)!)
endif


RM = rm -r -f

ifeq ($(HOST_ARCH),x86)
	export PATH:=/opt/gcc-linaro-arm-linux-gnueabihf-4.7-2013.04-20130415_linux/bin:$(PATH)
	COMPILERPREFIX=arm-linux-gnueabihf-
	CXX = $(COMPILERPREFIX)g++
	OMXCORE_INC :=../../omx-include
	FFMPEG_INC := ../ffmpeg
	FFMPG_LIB := ../lib
	CFLAGS += -fPIC
	CFLAGS += -g -O2  -I$(FFMPEG_INC) -I$(OMXCORE_INC)
	LDFLAGS := -L$(FFMPG_LIB) -ldl -lpthread  -lrt -lavformat -lavcodec -lavutil  -lz -lbz2 -lswresample -lvorbisenc -lvorbis -logg 
endif

ifeq ($(HOST_ARCH),arm)
	COMPILERPREFIX=arm-linux-gnueabihf-
	CXX = $(COMPILERPREFIX)g++
	LIBS_1 :=/usr/lib/arm-linux-gnueabihf
	LIBS_2 :=/lib/arm-linux-gnueabihf
	OMXCORE_INC :=/usr/include/omx-include
	COMMONLIBDIR :=
	CFLAGS := -fPIC
	CFLAGS += -g -O2   -I$(OMXCORE_INC)
	LDFLAGS := -ldl -lpthread  -lrt -lavformat -lavcodec -lavutil
endif


PROG := owlplayer

SOURCES_C := $(wildcard *.cpp)
OBJS_C := $(patsubst %.cpp, %.o,$(SOURCES_C))

all: clean $(PROG)

$(PROG): $(OBJS_C)
	$(CXX) -o $@  $(OBJS_C) $(LDFLAGS)
		
$(OBJS_C) : %.o:%.cpp
	$(CXX) -c $(CFLAGS) $< -o $@	
	
clean:
	echo $(UNAME)
	$(RM) $(PROG) *.o

.PHONY: clean
