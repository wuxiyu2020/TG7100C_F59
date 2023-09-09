
NAME := mars_httpc

$(NAME)_TYPE := framework

$(NAME)_SOURCES := mars_httpc.c

ifeq ($(CONFIG_BOARD_NAME), TG7100CEVB)
$(NAME)_CFLAGS      += -Wno-error=incompatible-pointer-types
else
#default gcc
ifeq ($(COMPILER),)
$(NAME)_CFLAGS      += -Wall -Werror
else ifeq ($(COMPILER),gcc)
$(NAME)_CFLAGS      += -Wall -Werror
endif
endif

# $(NAME)_COMPONENTS += yloop kernel.hal

GLOBAL_INCLUDES := include

GLOBAL_DEFINES += MARS_HTTPC