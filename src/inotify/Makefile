PRJ_HOME	:= $(shell echo $(PROJECT_ROOT) | sed 's/\ /\\ /g')
-include $(PRJ_HOME)/Makefile
PWD	:= $(shell pwd | sed 's/\ /\\ /g')
ifeq ($(BUILD), )
BUILD	:= $(PWD:$(shell dirname $(PWD))/%=%)
endif

###############################################################################
SOURCE			:= inotify.cc
INCLUDE_PATH	:= 
LIBRARIES		:= ${DIST}/itf_common
FLAGS			:= 
SHARED_LIBS		:= -lboost_program_options -lboost_filesystem -lboost_system
SHARED_LIBS		+= -llog4cpp -lpthread -lcurl
###############################################################################

ifeq ($(MAKECMDGOALS), $(BUILD)_all)
-include $(DEPEND_FILE)
endif

OBJ_DIR		:= $(shell echo $(OBJS_PATH)/$(BUILD) | sed 's/\ /\\ /g')
LIB_DIR		:= $(shell echo $(LIBS_PATH) | sed 's/\ /\\ /g')
BUILD_DIR	:= $(shell echo $(BINS_PATH) | sed 's/\ /\\ /g')

$(BUILD)_OBJS	:= $(SOURCE:%.cc=$(OBJ_DIR)/%.o)
$(BUILD)_LIBS	:= $(LIBRARIES:%=$(LIB_DIR)/%.a)
BUILD_NAME		:= $(BUILD_DIR)/$(PROJECT_NAME)_$(BUILD)

$(BUILD)_all: $($(BUILD)_OBJS)
	$(CPP) -o "$(BUILD_NAME)" $($(BUILD)_OBJS) $($(BUILD)_LIBS) $(SHARED_LIBS)

.SECONDEXPANSION:
$(OBJ_DIR)/%.o: %.cc
	@`[ -d "$(OBJ_DIR)" ] || $(MKDIR) "$(OBJ_DIR)"`
	@`[ -d "$(OBJ_DIR)/$(shell dirname $<)" ] || $(MKDIR) "$(OBJ_DIR)/$(shell dirname $<)"`
	$(CPP) $(CFLAGS) $(FLAGS) $(INCLUDE) $(INCLUDE_PATH:%=-I"%") -c $< -o "$@"

$(BUILD)_depend:
	@$(ECHO) "# $(OBJ_DIR)" > $(DEPEND_FILE)
	@for FILE in $(SOURCE:%.cc=%); do \
		$(CPP) -MM -MT "$(OBJ_DIR)/$$FILE.o" $$FILE.c $(CFLAGS) $(FLAGS) $(INCLUDE) >> $(DEPEND_FILE); \
	done

$(BUILD)_clean:
	$(RM) -rf "$(OBJ_DIR)"
	$(RM) -f "$(BUILD_NAME)"

$(BUILD)_mrproper:
	@$(RM) -f $(DEPEND_FILE)