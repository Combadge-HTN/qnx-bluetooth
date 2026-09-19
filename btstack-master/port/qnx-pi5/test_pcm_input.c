/* Run in a dedicated empty test directory, never the live project's FIFO. */
#include "pcm_input.h"
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void){
    assert(mkfifo("audio.pcm", 0600) == 0);
    assert(pcm_input_start() == 0);
    int writer = open("audio.pcm", O_WRONLY | O_NONBLOCK);
    assert(writer >= 0);
    int16_t out[4];
    unsigned char data[] = {0x34, 0x12, 0xfe, 0xff};
    assert(write(writer, data, 1) == 1);
    pcm_input_read(out, 1);
    assert(out[0] == 0 && out[1] == 0);
    assert(write(writer, data+1, 3) == 3);
    pcm_input_read(out, 2);
    assert(out[0] == 0x1234 && out[1] == -2 && out[2] == 0 && out[3] == 0);
    assert(write(writer, data, 1) == 1);
    close(writer);
    pcm_input_read(out, 1);
    assert(out[0] == 0 && out[1] == 0);
    writer = open("audio.pcm", O_WRONLY | O_NONBLOCK);
    assert(writer >= 0);
    assert(write(writer, data, 4) == 4);
    pcm_input_read(out, 1);
    assert(out[0] == 0x1234 && out[1] == -2);
    close(writer);
    pcm_input_stop();
    assert(unlink("audio.pcm") == 0);
    return 0;
}
