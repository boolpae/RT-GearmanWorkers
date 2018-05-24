###############################################################################
PROJECT_NAME	:= itf
PROJECT_ROOT	:= $(shell pwd | sed 's/\ /\\ /g')
SUB_PROJECTS	:= vr inotify
SUB_LIBRARIES	:= common worker
TEST_PROJECTS	:= sample

# Make variables (CC, etc...)
CC		:= gcc
CPP		:= g++
STDLIB	:= -std=c++11
MAKE	:= make
MKDIR	:= mkdir -p
RM		:= rm
CP		:= cp
LINK	:= ln -sf
ECHO	:= echo
AR		:= ar
RANLIB	:= ranlib
ANT		:= /usr/local/bin/ant
###############################################################################

SRC_NAME	:= src
INC_NAME	:= include
OBJ_NAME	:= obj
LIB_NAME	:= lib
BIN_NAME	:= bin
DEPEND_FILE	:= .depend

# Platform
OS_NAME	:= $(shell uname -s)
OS_VER	:= $(shell uname -r)
OS_HW	:= $(shell uname -m)

ifneq "$(findstring release, $(MAKECMDGOALS))" ""
DIST	:= Release
CFLAGS 	:= -Wall -O3 $(STDLIB)
OPTION	:= RELEASE=1
else
DIST	:= Debug
CFLAGS 	:= -g -O0 $(STDLIB)
endif

ifeq ($(RELEASE), 1)
DIST	:= Release
CFLAGS 	:= -Wall -O3 $(STDLIB)
endif

ifneq "$(findstring lib, $(MAKECMDGOALS))" ""
BUILD.LIB	:= 1
endif
ifneq "$(findstring bin, $(MAKECMDGOALS))" ""
BUILD.BIN	:= 1
endif
ifneq "$(findstring test, $(MAKECMDGOALS))" ""
BUILD.TEST	:= 1
endif

INCLUDE		:= $(INC_NAME:%=-I"$(PROJECT_ROOT)/%")
SRCS_PATH	:= $(SRC_NAME:%=$(PROJECT_ROOT)/%)
OBJS_PATH	:= $(OBJ_NAME:%=$(PROJECT_ROOT)/%/$(DIST))
LIBS_PATH	:= $(LIB_NAME:%=$(PROJECT_ROOT)/%/$(OS_NAME)_$(OS_HW))
BINS_PATH	:= $(BIN_NAME:%=$(PROJECT_ROOT)/%/$(DIST)/$(OS_NAME)_$(OS_HW))

.PHONY: release clean lib bin all

ifeq ($(BUILD.LIB),1)
lib: $(SUB_LIBRARIES)
release: ; @true
else ifeq ($(BUILD.BIN),1)
bin: $(SUB_PROJECTS)
release: ; @true
else ifeq ($(BUILD.TEST),1)
test: $(TEST_PROJECTS)
release: ; @true
else
release: all
endif

all: $(SUB_LIBRARIES) $(SUB_PROJECTS)

$(SUB_LIBRARIES):
	@`[ -d "$(OBJS_PATH)" ] || $(MKDIR) "$(OBJS_PATH)"`
	@`[ -d "$(LIBS_PATH)" ] || $(MKDIR) "$(LIBS_PATH)"`
	$(MAKE) -C $(SRCS_PATH)/$@ $@_all PROJECT_ROOT=$(PROJECT_ROOT) BUILD=$@ $(OPTION)

$(SUB_PROJECTS) $(TEST_PROJECTS): $(SUB_LIBRARIES)
	@`[ -d "$(OBJS_PATH)" ] || $(MKDIR) "$(OBJS_PATH)"`
	@`[ -d "$(BINS_PATH)" ] || $(MKDIR) "$(BINS_PATH)"`
	$(MAKE) -C $(SRCS_PATH)/$@ $@_all PROJECT_ROOT=$(PROJECT_ROOT) BUILD=$@ $(OPTION)

depend dep:
	@for DIR in $(SUB_LIBRARIE/S) $(SUB_PROJECTS); do \
		$(MAKE) -C $(SRCS_PATH)/$$DIR PROJECT_ROOT=$(PROJECT_ROOT) BUILD=$$DIR $${DIR}_depend; \
		if [ $$? != 0 ]; then exit 1; fi; \
	done

distclean: mrproper clean
	$(RM) -rf "$(PROJECT_ROOT)/$(BIN_NAME)" "$(PROJECT_ROOT)/$(OBJ_NAME)"

mrproper:
	@for DIR in $(SUB_LIBRARIES) $(SUB_PROJECTS); do \
		$(MAKE) -C $(SRCS_PATH)/$$DIR PROJECT_ROOT=$(PROJECT_ROOT) BUILD=$$DIR $${DIR}_mrproper; \
		if [ $$? != 0 ]; then exit 1; fi; \
	done

clean: 
	@for DIR in $(SUB_LIBRARIES) $(SUB_PROJECTS) $(TEST_PROJECTS); do \
		$(MAKE) -C $(SRCS_PATH)/$$DIR PROJECT_ROOT=$(PROJECT_ROOT) BUILD=$$DIR $${DIR}_clean; \
		if [ $$? != 0 ]; then exit 1; fi; \
	done
	@for DIR in $(SUB_LIBRARIES) $(SUB_PROJECTS) $(TEST_PROJECTS); do \
		$(MAKE) -C $(SRCS_PATH)/$$DIR PROJECT_ROOT=$(PROJECT_ROOT) BUILD=$$DIR $${DIR}_clean RELEASE=1; \
		if [ $$? != 0 ]; then exit 1; fi; \
	done
	@`[ ! -d "$(BIN_NAME)" ] || $(RM) -r "$(BIN_NAME)"`
