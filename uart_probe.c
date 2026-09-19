/* Pi 5 BCM2712 Bluetooth UART diagnostics. No writes in default mode.
 * Register layout: Raspberry Pi Linux bcm2712-ds.dtsi, 8250_bcm7271.c.
 * This is not a PL011 UART. Never point this program at the debug console.
 */
#ifndef _QNX_SOURCE
#define _QNX_SOURCE 1
#endif
#include <sys/mman.h>
#include <hw/inout.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define BASE UINT64_C(0x107d50c000)
static uintptr_t regs;
static uint32_t rd(unsigned r) { return in32(regs + 4*r); }
static void wr(unsigned r, uint32_t v) { out32(regs + 4*r, v); }
static uint64_t ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec*1000 + t.tv_nsec/1000000;
}
static int exchange(const uint8_t *cmd, size_t n) {
    uint64_t deadline = ms()+2000;
    size_t sent=0, got=0, need=3;
    uint8_t event[258];
    while (ms()<deadline) {
        uint32_t status=rd(5);
        if (status & 0x1e) { fprintf(stderr,"UART line error: %02x\n",status); return -1; }
        /* BCM7271 FIFO holds 32 bytes. Send short HCI commands contiguously. */
        if (sent<n && (status&0x20)) {
            size_t burst=n-sent; if(burst>32) burst=32;
            while(burst--) wr(0,cmd[sent++]);
        }
        if (status&1) {
            if (got==sizeof(event)) return -1;
            event[got++]=(uint8_t)rd(0);
            if (got==1 && event[0]!=4) { fprintf(stderr,"Not H4 event: %02x\n",event[0]); return -1; }
            if (got==3) need=3+event[2];
            if (got==need) {
                for(size_t i=0;i<got;i++) printf("%02x ",event[i]);
                puts("");
                if(got<7 || event[1]!=0x0e || event[4]!=cmd[1] || event[5]!=cmd[2] || event[6]) return -1;
                return 0;
            }
        }
        if(!(status&1)) { struct timespec delay={0,100000}; nanosleep(&delay,NULL); }
    }
    fprintf(stderr,"HCI timeout (sent=%zu, received=%zu)\n",sent,got);
    return -1;
}
int main(int argc,char **argv) {
    int loopback=argc==2 && !strcmp(argv[1],"--loopback");
    int recover=argc==2 && !strcmp(argv[1],"--idle-after-abort");
    int active=recover || loopback || (argc==2 && !strcmp(argv[1],"--hci-read-info"));
    if(argc>1 && !active) { fprintf(stderr,"Usage: %s [--hci-read-info|--loopback]\n",argv[0]); return 2; }
    void *p=mmap_device_memory(NULL,0x20,PROT_READ|PROT_NOCACHE|(active?PROT_WRITE:0),0,BASE);
    if(p==MAP_FAILED) { perror("map Bluetooth UART"); return 1; }
    regs=(uintptr_t)p;
    uint32_t lcr=rd(3),mcr=rd(4);
    printf("BT UART 0x%llx LCR=%02x MCR=%02x\n",(unsigned long long)BASE,lcr,mcr);
    if(!active) { munmap_device_memory(p,0x20); return 0; }
    if(recover){
        /* Only after verifying no radio process exists and powering BT off.
         * Preserve the divisor: its pre-abort value is no longer available. */
        if(lcr != 3 || mcr != 0x22 || rd(1) != 0) return 1;
        wr(4,0); wr(3,0); wr(1,0);
        munmap_device_memory(p,0x20);
        return 0;
    }
    /* Requires separately verified pinmux, powered radio and exclusive ownership.
     * No IRQs, DMA, GPIO, clock/reset controller, or persistent writes.
     * Save the baud divisor and control registers, restore on normal exit.
     */
    if(lcr&0x80) { fputs("DLAB set: refusing possibly active UART\n",stderr); return 1; }
    uint32_t ier=rd(1);
    if(ier) { fputs("Interrupts enabled: UART may be owned; refusing\n",stderr); return 1; }
    if((rd(2)&0xc0)!=0xc0) { fputs("Expected enabled FIFO; refusing to alter unknown FIFO settings\n",stderr); return 1; }
    wr(3,lcr|0x80); uint32_t dll=rd(0),dlh=rd(1);
    wr(0,52); wr(1,0); /* 96 MHz / 16 / 52 = 115384 baud */
    wr(3,3); wr(4,2); /* 8N1, RTS asserted; preserve existing FIFO setup */
    printf("Configured LCR=%02x MCR=%02x LSR=%02x MSR=%02x; original divisor=%u\n",rd(3),rd(4),rd(5),rd(6),(dlh<<8)|dll);
    const uint8_t reset[]={1,3,12,0};
    const uint8_t version[]={1,1,16,0};
    const uint8_t name[]={1,20,12,0};
    int rc=0;
    if(loopback) {
        wr(4,0x12); /* internal loopback; no radio or GPIO access */
        wr(0,0xa5);
        uint64_t deadline=ms()+1000;
        while(!(rd(5)&1) && ms()<deadline) {
            struct timespec delay={0,1000000}; nanosleep(&delay,NULL);
        }
        if(!(rd(5)&1)) { fputs("Internal UART loopback timed out\n",stderr); rc=-1; }
        else { uint32_t value=rd(0); printf("Internal loopback received %02x\n",value); rc=value==0xa5?0:-1; }
    } else {
        if(system("gpio-bcm get 24-29")!=0) { fputs("Pin readback failed\n",stderr); rc=-1; }
        uint64_t deadline=ms()+2000;
        while(!(rd(6)&0x10) && ms()<deadline) {
            struct timespec delay={0,1000000}; nanosleep(&delay,NULL);
        }
        if(!(rd(6)&0x10)) {
            fputs("Radio did not assert CTS; no HCI bytes sent\n",stderr); rc=-1;
        }
        if(!rc) rc=exchange(reset,sizeof(reset));
        if(!rc) rc=exchange(version,sizeof(version));
        if(!rc) rc=exchange(name,sizeof(name));
    }
    wr(3,0x83); wr(0,dll); wr(1,dlh); wr(3,lcr); wr(4,mcr); wr(1,ier);
    munmap_device_memory(p,0x20);
    return rc?1:0;
}
