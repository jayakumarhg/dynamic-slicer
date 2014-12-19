##############################################################
#
# Here are some things you might want to configure
#
##############################################################

#TARGET_COMPILER?=ms
TARGET_COMPILER?=gnu

##############################################################
#
# include *.config files
#
##############################################################

ifeq ($(TARGET_COMPILER),gnu)
    include ./makefile.gnu.config
    LINKER?=${CXX}
    CXXFLAGS ?= -I$(PIN_HOME)/InstLib -fomit-frame-pointer -Wall -Wno-unknown-pragmas $(DBG) $(OPT) -MMD -g
endif

ifeq ($(TARGET_COMPILER),ms)
    include ./makefile.ms.config
    DBG?=
endif

##############################################################
#
# Tools sets
#
##############################################################

TOOL_ROOTS = tracer tracer.naive 

SANITY_TOOLS = opcodemix$(PINTOOL_SUFFIX)
# leave out fence, see comment at top of fence.cpp

TEST_TOOLS_ROOTS = tracer

TEST_TOOLS = $(TEST_TOOLS_ROOTS:%=%$(PINTOOL_SUFFIX))

TOOLS = $(TOOL_ROOTS:%=%$(PINTOOL_SUFFIX))

all: tools
tools: $(TOOLS)
test: $(TEST_TOOLS:%=%.test)
tests-sanity: $(SANITY_TOOLS:%=%.test)

## build rules

%.o : %.cxx
	${CXX} ${COPT} $(CXXFLAGS) ${PIN_CXXFLAGS} ${OUTOPT}$@ $< 

$(TOOLS): $(PIN_LIBNAMES)
$(TOOLS): %$(PINTOOL_SUFFIX) : %.o
	${LINKER} ${PIN_LDFLAGS} $(LINK_DEBUG) ${LINK_OUT}$@ $< ${PIN_LPATHS} ${PIN_LIBS} $(DBG)

## cleaning
clean:
	-rm -f *.o $(TOOLS) *.out *.tested *.failed *.d *makefile.copy *.exp *.lib

-include *.d

