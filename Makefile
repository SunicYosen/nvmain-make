# Make file for nvmain
# SunicYosen

default:bin

SHELL ?= /bin/bash
BASE_DIR = $(abspath .)
BUILD_ROOT = $(abspath $(BASE_DIR)/build)
NVMAIN_BUILD = $(abspath $(BASE_DIR)/trace)

OBJDIR = $(BUILD_ROOT)/obj

CPPPATH = $(BASE_DIR)
SRCDIR = $(BASE_DIR)

GCC ?= gcc-4.8

GXX ?= g++-4.8

#fast/debug/prof
TYPECONFIG ?= fast

CXXFLAGS :=  -Werror -Wall -fPIC -DTRACE\
	           -std=c++11 -Wextra -DNDEBUG \
			       -Woverloaded-virtual  \

ifeq ($(TYPECONFIG),fast)
	CXXFLAGS += -O3
	OBJSUFFIX := fo
	build_type := "fast"

else 
    ifeq ($(TYPECONFIG),debug)
	    CXXFLAGS += -O0 -ggdb3
	    OBJSUFFIX = do
	    build_type := "debug"
		
    else 
	    ifeq ($(TYPECONFIG),prof)
    	    CXXFLAGS += -O0 -ggdb3 -pg        
    	    LINKFLAGS := -pg
    	    OBJSUFFIX = po
    	    build_type := "prof"
		else
			build_type := "error"
		endif
	endif
endif

program_name = nvmain
program_name_oneStep = nvmain_onstep

BIN := $(program_name).$(TYPECONFIG)
BIN_ONESTEP := $(program_name_oneStep).$(TYPECONFIG)

# ALLSUBDIR := $(shell ls \.\./nvmain -R | grep '^\./.*:$$' | awk '{gsub(":","");print}')
ALLSUBDIR := $(shell find . -maxdepth 4 -type d)
SRCS := $(foreach n,$(ALLSUBDIR), $(wildcard $(n)/*.cpp))
HEADERS :=  $(foreach n,$(ALLSUBDIR) , $(wildcard $(n)/*.h))
OBJS_S := $(patsubst %.cpp, %.$(OBJSUFFIX), $(SRCS))

$(BUILD_ROOT)/%.$(OBJSUFFIX): %.cpp
	mkdir -p $(@D) && \
	$(GXX) $(CXXFLAGS) -o $@ -c $<

$(BIN) : $(addprefix $(BUILD_ROOT)/,$(OBJS_S))
	$(GXX) $^ -o $(BUILD_ROOT)/$@

$(BIN_ONESTEP): $(SRCS) $(HEADERS)
	mkdir -p $(BUILD_ROOT) && \
	$(GXX) $(SRCS) $(CXXFLAGS) -o $(BUILD_ROOT)/$(@F)

.PHONY: bin
.PHONY: onestep_bin
bin: $(BIN)
onestep_bin: $(BIN_ONESTEP)

.PHONY: clean
clean:
	rm -f $(addprefix $(BUILD_ROOT)/,$(OBJS_S)) $(BUILD_ROOT)/$(BIN) $(BUILD_ROOT)/$(BIN_ONESTEP)
	rm -rf $(addprefix $(BUILD_ROOT)/,$(ALLSUBDIR))
	rm -rf $(BUILD_ROOT)