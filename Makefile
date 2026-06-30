.SUFFIXES : .c .o

#include tools.mk

TARGETDEV := 192.168.0.100

LOCAL_DIR := $(shell pwd)

Q_		   := @
PKG_CONFIG ?= pkg-config


CFLAGS    += -fPIC -Wno-unused-result -Wno-sign-compare
CXXFLAGS  += -fPIC -Wno-unused-result -Wno-sign-compare -std=c++17
LDFLAGS   += -lpthread -ldl

OUT_DIR   := out
TARGET    := Test

INCDIRS   :=
SRCDIRS   :=
SRCS      :=

INCDIRS   += $(LOCAL_DIR)
SRCDIRS   += $(LOCAL_DIR)
SRCS      += main.cpp

SRCS      += Log.cpp
SRCS      += WorkerThread.cpp

SRCS      += SocketCAN.cpp

###############################################################################
# DO NOT MODIFY .......
###############################################################################
APP           := $(OUT_DIR)/$(TARGET)
APP_OBJS      := $(SRCS:%=$(OUT_DIR)/%.o)
APP_DEPS      := $(APP_OBJS:.o=.d)
APP_CFLAGS    := $(CFLAGS) $(DEFINES) -MMD -MP
APP_CXXFLAGS  := $(CXXFLAGS) $(DEFINES) -MMD -MP
APP_CFLAGS    += $(addprefix -I, $(INCDIRS))
APP_CXXFLAGS  += $(addprefix -I, $(INCDIRS))
APP_LDFLAGS   := $(addprefix -L, $(LIBDIRS))
APP_LDFLAGS   += $(LDFLAGS)

vpath %.cpp $(SRCDIRS)
vpath %.c $(SRCDIRS)

.PHONY: all clean

all: $(OUT_DIR) app

app: $(APP_OBJS)
	@echo "[Linking... $(notdir $(APP))]"
	$(Q_)$(CXX) -o $(APP) $(APP_OBJS) $(APP_LDFLAGS)

clean:
	@echo "[Clean... all objs]"
	$(Q_)rm -rf $(OUT_DIR)

$(OUT_DIR):
	$(Q_)mkdir -p $(OUT_DIR)

$(OUT_DIR)/%.c.o: %.c
	@echo "[Compile... $(notdir $<)]"
	$(Q_)$(CC) $(APP_CFLAGS) -c $< -o $@

$(OUT_DIR)/%.cpp.o: %.cpp
	@echo "[Compile... $(notdir $<)]"
	$(Q_)$(CXX) $(APP_CXXFLAGS) -c $< -o $@

-include $(APP_DEPS)

install:
	@echo "[Install .... $(notdir $(APP))]"
	scp $(APP) root@$(TARGETDEV):/home/root
