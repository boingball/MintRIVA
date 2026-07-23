# Ittiam libavc decoder, built with its architecture-neutral integer C paths.
# Encoder, MVC/SVC and all CPU-specific assembly are intentionally excluded.
LIBAVC_ROOT ?= vendor/libavc
LIBAVC_PORT ?= vendor/libavc_port

LIBAVC_COMMON = $(filter-out $(LIBAVC_ROOT)/common/ithread.c \
                  $(LIBAVC_ROOT)/common/ih264_resi_trans_quant.c \
                  $(LIBAVC_ROOT)/common/ih264_trans_data.c, \
                  $(wildcard $(LIBAVC_ROOT)/common/*.c))
LIBAVC_DECODER = $(wildcard $(LIBAVC_ROOT)/decoder/*.c)
LIBAVC_PORTSRC = $(LIBAVC_PORT)/ih264d_function_selector_port.c \
                 $(LIBAVC_PORT)/ithread_port.c $(LIBAVC_PORT)/compat.c
LIBAVC_SRC = $(LIBAVC_COMMON) $(LIBAVC_DECODER) $(LIBAVC_PORTSRC)
LIBAVC_FLAGS = -I$(LIBAVC_PORT) -I$(LIBAVC_ROOT)/common \
               -I$(LIBAVC_ROOT)/decoder -include $(LIBAVC_PORT)/compat.h
LIBAVC_GCC_FLAGS = -fno-strict-aliasing -fwrapv
