ifeq (${MODE},)
	MODE := release
else
	ifneq (${MODE},release)
		ifneq (${MODE},debug)
			$(error Invalid MODE value "${MODE}")
		endif
	endif
endif

ifeq (${CXX},)
	CXX := c++
endif

BINDIR := bin
OBJDIR := obj
BINSUBDIR := ${BINDIR}/${MODE}
OBJSUBDIR := ${OBJDIR}/${MODE}
SRCDIR := src
SRCSUBDIRS := queues

EXAMPLE_SRCDIR := ${SRCDIR}/examples
EXAMPLE_SRCS := $(wildcard ${EXAMPLE_SRCDIR}/*.cpp)
EXAMPLE_ARTIFACTS := $(subst .cpp,,$(subst ${SRCDIR}/,${BINSUBDIR}/,${EXAMPLE_SRCS}))

SRCS := ${EXAMPLE_SRCS}
OBJS := $(subst ${SRCDIR}/,${OBJSUBDIR}/,$(subst .cpp,.o,${SRCS}))
DEPS := $(subst .o,.d,${OBJS})

INCLUDE_FLAGS := $(addprefix -I,${SRCDIR})

CXXFLAGS_BASE := -std=c++11 -Wall -Wextra ${INCLUDE_FLAGS}
CXXFLAGS_DEBUG := ${CXXFLAGS_BASE} -O0 -g2
CXXFLAGS_RELEASE := ${CXXFLAGS_BASE} -O2 -g -DNDEBUG

ifeq (${MODE},release)
	CXXFLAGS := ${CXXFLAGS_RELEASE}
else
	CXXFLAGS := ${CXXFLAGS_DEBUG}
endif

LDFLAGS := -lstdc++ -pthread

all: ${EXAMPLE_ARTIFACTS}

help:
	@echo "This makefile builds the example code in directory ${EXAMPLE_SRCDIR}. Run \"make MODE=debug\" for a debug build."

#${OBJDIR} ${BINDIR}:
#	@if [ ! -d $@ ]; then mkdir -p $@; fi

${OBJSUBDIR}/%.o: ${SRCDIR}/%.cpp
	@if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi
	${CXX} $< ${CXXFLAGS} -c -MMD -o $@

-include ${DEPS}

${BINSUBDIR}/%: ${OBJSUBDIR}/%.o
	@if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi
	${CXX} -o $@ $^ ${LDFLAGS}

clean:
	rm -rf ${OBJDIR} ${BINDIR}

.PHONY: all help clean
