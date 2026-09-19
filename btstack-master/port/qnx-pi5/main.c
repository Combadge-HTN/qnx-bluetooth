/* QNX user-space Bluetooth audio bring-up. Upstream BTstack license applies
 * to the linked stack; see ../../LICENSE. No automatic GPIO or boot changes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include "btstack.h"
#include "btstack_run_loop_posix.h"
#include "btstack_uart.h"
#include "hci_transport_h4.h"
#include "btstack_chipset_bcm.h"
#include "btstack_tlv_posix.h"
#include "hci_dump_posix_fs.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "ble/le_device_db_tlv.h"
#include "pcm_input.h"

static btstack_packet_callback_registration_t callback;
static btstack_tlv_posix_t database;
static volatile sig_atomic_t stopping;
static btstack_timer_source_t watchdog;
static unsigned startup_ticks;
static int working, failed;
int btstack_main(int argc, const char **argv);
const btstack_uart_t *btstack_uart_qnx_bcm_instance(void);
int btstack_uart_qnx_bcm_error(void);
int board_bluetooth_address(uint8_t address[6]);
static void on_signal(int sig) { (void)sig; stopping=1; }
static void tick(btstack_timer_source_t *timer) {
    if(stopping || (!working && ++startup_ticks>300)) {
        if(!stopping) { fputs("Controller startup timed out\n",stderr); failed=1; }
        btstack_run_loop_trigger_exit(); return;
    }
    btstack_run_loop_set_timer(timer,100);
    btstack_run_loop_add_timer(timer);
}
static void event(uint8_t type,uint16_t channel,uint8_t *packet,uint16_t size) {
    (void)channel; (void)size;
    if(type!=HCI_EVENT_PACKET) return;
    if(hci_event_packet_get_type(packet)==BTSTACK_EVENT_POWERON_FAILED) {
        failed=1; btstack_run_loop_trigger_exit();
    }
    if(hci_event_packet_get_type(packet)==BTSTACK_EVENT_STATE &&
       btstack_event_state_get_state(packet)==HCI_STATE_WORKING) {
        working=1; puts("Controller ready. Use the demo menu to scan/connect.");
    }
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--check-board")) {
        bd_addr_t address;
        if(board_bluetooth_address(address)) return 1;
        printf("Pi 5 factory Bluetooth address: %s\n",bd_addr_to_str(address));
        return 0;
    }
    if(argc!=3 || !strcmp(argv[1],"--help")) {
        printf("Usage: %s --onboard controller-firmware.hcd\n"
               "       --check-board: read-only board/address validation\n"
               "Requires an exclusively owned, configured Bluetooth UART.\n"
               "This is an A2DP test application, not system-wide audio routing.\n",argv[0]);
        return argc==2 && !strcmp(argv[1],"--help")?0:2;
    }
    if(strcmp(argv[1],"--onboard")) {
        fputs("Refusing non-Bluetooth UART (including debug console).\n",stderr); return 2;
    }
    struct stat st;
    if(stat(argv[2],&st) || !S_ISREG(st.st_mode)) { perror("Firmware unavailable"); return 1; }
    bd_addr_t factory_address;
    if(board_bluetooth_address(factory_address)) return 1;
    printf("Factory Bluetooth address: %s\n",bd_addr_to_str(factory_address));
    umask(077);
    if(hci_dump_posix_fs_open("hci.pklg",HCI_DUMP_PACKETLOGGER)!=0) return 1;
    hci_dump_init(hci_dump_posix_fs_get_instance());
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    const btstack_tlv_t *tlv=btstack_tlv_posix_init_instance(&database,"pairings.tlv");
    btstack_tlv_set_instance(tlv,&database);
    hci_transport_config_uart_t config={
        .type=HCI_TRANSPORT_CONFIG_UART, .baudrate_init=115200,
        .baudrate_main=3000000, .flowcontrol=1, .device_name=argv[1], .parity=0
    };
    const btstack_uart_t *uart=btstack_uart_qnx_bcm_instance();
    hci_init(hci_transport_h4_instance_for_uart(uart),&config);
    hci_set_link_key_db(btstack_link_key_db_tlv_get_instance(tlv,&database));
    le_device_db_tlv_configure(tlv,&database);
    btstack_chipset_bcm_set_hcd_file_path(argv[2]);
    hci_set_chipset(btstack_chipset_bcm_instance());
    hci_set_bd_addr(factory_address);
    callback.callback=event; hci_add_event_handler(&callback);
    signal(SIGINT,on_signal); signal(SIGTERM,on_signal); signal(SIGHUP,on_signal);
    btstack_run_loop_set_timer_handler(&watchdog,tick);
    btstack_run_loop_set_timer(&watchdog,100); btstack_run_loop_add_timer(&watchdog);
    btstack_main(argc,(const char **)argv);
    btstack_run_loop_execute();
    pcm_input_stop();
    hci_power_control(HCI_POWER_OFF);
    uart->close();
    btstack_stdin_reset();
    btstack_tlv_posix_deinit(&database);
    hci_dump_posix_fs_close();
    return failed || btstack_uart_qnx_bcm_error()?1:0;
}
