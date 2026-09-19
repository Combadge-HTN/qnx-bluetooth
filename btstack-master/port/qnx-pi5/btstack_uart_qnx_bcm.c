/* Experimental BCM2712/BCM7271 UART transport for QNX 8.
 * Fixed Bluetooth UART only. GPIO ownership/power is managed by run-radio.sh.
 * No interrupts, DMA, shared clocks, reset controllers or persistent writes.
 * Hardware RTS/CTS protects the 32-byte FIFO across run-loop scheduling gaps.
 */
#include <sys/mman.h>
#include <hw/inout.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "btstack_uart.h"
#include "btstack_run_loop.h"

static uintptr_t regs;
static uint32_t saved_lcr, saved_mcr, saved_dll, saved_dlh;
static int opened, error;
static btstack_timer_source_t timer;
static const btstack_uart_config_t *config;
static uint8_t *rx;
static const uint8_t *tx;
static unsigned rx_left, tx_left;
static uint64_t tx_deadline;
static void (*received)(void), (*sent)(void);
static uint32_t rd(unsigned r) { return in32(regs+4*r); }
static void wr(unsigned r,uint32_t v) { out32(regs+4*r,v); }
static uint64_t now_us(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000000 + t.tv_nsec/1000;
}
static int set_baud(uint32_t baud) {
    if(!opened || (baud!=115200 && baud!=3000000)) return -1;
    uint64_t deadline=now_us()+100000;
    while(!(rd(5)&0x40)) if(now_us()>deadline) return -1;
    uint32_t divisor=baud==115200?52:2;
    wr(3,0x83); wr(0,divisor); wr(1,0); wr(3,3);
    printf("Bluetooth UART baud: %u\n",baud);
    return 0;
}
static void fail(const char *reason) {
    fprintf(stderr,"Bluetooth UART: %s\n",reason);
    error=1; btstack_run_loop_trigger_exit();
}
static void poll_uart(btstack_timer_source_t *source) {
    if(!opened) return;
    uint64_t budget=now_us()+1000;
    do {
        uint32_t lsr=rd(5);
        if(lsr&0x1e) { fail("receive overrun/parity/framing error"); return; }
        if(rx_left && (lsr&1)) {
            *rx++=(uint8_t)rd(0);
            if(--rx_left==0 && received) received();
            if(!opened) return;
            continue;
        }
        if(tx_left && (lsr&0x20) && (rd(6)&0x10)) {
            unsigned count=tx_left>32?32:tx_left;
            tx_left-=count;
            while(count--) wr(0,*tx++);
            if(!tx_left && sent) sent();
            if(!opened) return;
            continue;
        }
        if(!tx_left) break;
        if(now_us()>tx_deadline) { fail("CTS/transmit timeout"); return; }
    } while(now_us()<budget);
    btstack_run_loop_set_timer(source,1);
    btstack_run_loop_add_timer(source);
}
static int init(const btstack_uart_config_t *c) { config=c; return 0; }
static int close_uart(void) {
    if(!opened) return 0;
    btstack_run_loop_remove_timer(&timer);
    /* Caller powers down radio after close; restore initial UART state. */
    wr(3,0x83); wr(0,saved_dll); wr(1,saved_dlh);
    wr(3,saved_lcr); wr(4,saved_mcr); wr(1,0);
    munmap_device_memory((void *)regs,0x20);
    opened=0; rx_left=tx_left=0;
    return 0;
}
static int open_uart(void) {
    if(opened || !config) return -1;
    void *p=mmap_device_memory(NULL,0x20,PROT_READ|PROT_WRITE|PROT_NOCACHE,0,UINT64_C(0x107d50c000));
    if(p==MAP_FAILED) { perror("Bluetooth UART mapping"); return -1; }
    regs=(uintptr_t)p;
    saved_lcr=rd(3); saved_mcr=rd(4);
    if(saved_lcr!=0 || saved_mcr!=0 || rd(1)!=0 || (rd(2)&0xc0)!=0xc0) {
        fputs("UART differs from verified idle state; refusing\n",stderr);
        munmap_device_memory(p,0x20); return -1;
    }
    wr(3,0x80); saved_dll=rd(0); saved_dlh=rd(1); wr(3,saved_lcr);
    opened=1; error=0;
    if(set_baud(config->baudrate)) { close_uart(); return -1; }
    /* Discard stale H4 bytes left by a previously interrupted session. */
    unsigned drained = 0;
    while((rd(5)&1) && drained < 256){ (void)rd(0); drained++; }
    if(rd(5)&1){ close_uart(); return -1; }
    wr(4,0x22); /* auto RTS/CTS and RTS asserted */
    btstack_run_loop_set_timer_handler(&timer,poll_uart);
    btstack_run_loop_set_timer(&timer,1); btstack_run_loop_add_timer(&timer);
    return 0;
}
static void set_received(void (*cb)(void)) { received=cb; }
static void set_sent(void (*cb)(void)) { sent=cb; }
static int parity(int value) { return value==BTSTACK_UART_PARITY_OFF?0:-1; }
static int flow(int value) { return value==BTSTACK_UART_FLOWCONTROL_ON?0:-1; }
static void receive_block(uint8_t *buffer,uint16_t len) {
    if(rx_left || !len) { fail("invalid overlapping receive"); return; }
    rx=buffer; rx_left=len;
}
static void send_block(const uint8_t *buffer,uint16_t len) {
    if(tx_left || !len) { fail("invalid overlapping transmit"); return; }
    tx=buffer; tx_left=len; tx_deadline=now_us()+2000000;
}
const btstack_uart_t *btstack_uart_qnx_bcm_instance(void) {
    static const btstack_uart_t driver={
        .init=init,.open=open_uart,.close=close_uart,
        .set_block_received=set_received,.set_block_sent=set_sent,
        .set_baudrate=set_baud,.set_parity=parity,.set_flowcontrol=flow,
        .receive_block=receive_block,.send_block=send_block
    };
    return &driver;
}
int btstack_uart_qnx_bcm_error(void) { return error; }
