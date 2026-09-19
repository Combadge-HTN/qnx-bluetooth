/* Read-only access to the firmware-populated Bluetooth address in the live FDT. */
#include <sys/mman.h>
#include <sys/asinfo.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <libfdt.h>
struct address_result { uint8_t *address; int found; };
static int read_address(struct asinfo_entry *as,char *name,void *arg) {
    (void)name;
    struct address_result *result=arg;
    uint64_t length=as->end-as->start+1;
    if(length<40 || length>1024*1024) return 1;
    void *tree=mmap64(NULL,(size_t)length,PROT_READ,MAP_SHARED|MAP_PHYS,NOFD,(off64_t)as->start);
    if(tree==MAP_FAILED) { perror("map live FDT"); return 1; }
    if(fdt_check_header(tree)==0 && fdt_totalsize(tree)<=length &&
       fdt_node_check_compatible(tree,0,"raspberrypi,5-model-b")==0) {
        int node=fdt_node_offset_by_compatible(tree,-1,"brcm,bcm43438-bt");
        int size=0;
        const uint8_t *addr=node<0?NULL:fdt_getprop(tree,node,"local-bd-address",&size);
        if(addr && size==6) {
            unsigned value=0;
            for(int i=0;i<6;i++) { result->address[i]=addr[5-i]; value|=addr[i]; }
            result->found=value!=0;
        }
    }
    munmap(tree,(size_t)length);
    return 0;
}
int board_bluetooth_address(uint8_t address[6]) {
    struct address_result result={address,0};
    if(walk_asinfo("fdt",read_address,&result)!=0 || !result.found) {
        fputs("Factory Bluetooth address not found in live device tree\n",stderr); return -1;
    }
    return 0;
}
