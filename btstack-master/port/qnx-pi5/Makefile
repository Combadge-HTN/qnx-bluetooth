BTSTACK_ROOT = ../..
.DEFAULT_GOAL := all
CORE = btstack_run_loop_posix.c btstack_uart_qnx_bcm.c btstack_stdin_posix.c \
       btstack_tlv_posix.c btstack_link_key_db_tlv.c le_device_db_tlv.c \
       hci_transport_h4.c hci_dump_posix_fs.c btstack_chipset_bcm.c \
       rijndael.c wav_util.c board_address.c fdt.c fdt_ro.c pcm_input.c main.c
include $(BTSTACK_ROOT)/example/Makefile.inc
CFLAGS += -O2 -g -Wall -Wextra -D_DEFAULT_SOURCE -DVOLUME_REDUCTION=5 -DQNX_PCM_INPUT -Ifdt-include \
          -I$(BTSTACK_ROOT)/platform/posix -I$(BTSTACK_ROOT)/platform/embedded \
          -I$(BTSTACK_ROOT)/chipset/bcm -I$(BTSTACK_ROOT)/3rd-party/rijndael \
          -I$(BTSTACK_ROOT)/3rd-party/tinydir
VPATH += fdt-include $(BTSTACK_ROOT)/platform/posix $(BTSTACK_ROOT)/platform/embedded \
         $(BTSTACK_ROOT)/chipset/bcm $(BTSTACK_ROOT)/3rd-party/rijndael
LDFLAGS += -lpthread
all: a2dp_source_demo
