/* Bounded kernel FIFO buffering; never block the Bluetooth run loop. */
#include "pcm_input.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int input = -1;
static unsigned char partial[4];
static unsigned used;
static unsigned long received, silence;
void pcm_input_stop(void){
    if(input >= 0){
        close(input);
        printf("PCM stopped: %lu input frames, %lu silence frames\n", received, silence);
    }
    input = -1;
    used = 0;
}
int pcm_input_start(void){
    struct stat st;
    pcm_input_stop();
    input = open("audio.pcm", O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    if(input < 0){ perror("Open audio.pcm FIFO"); return -1; }
    if(fstat(input, &st) || !S_ISFIFO(st.st_mode)){
        fputs("audio.pcm must be a FIFO\n", stderr);
        close(input); input = -1; return -1;
    }
    received = silence = 0;
    return 0;
}
void pcm_input_read(int16_t *samples, int frames){
    unsigned char bytes[2048];
    int done = 0;
    memset(samples, 0, (size_t)frames * 4);
    while(input >= 0 && done < frames){
        size_t want = (size_t)(frames - done) * 4 - used;
        if(want > sizeof(bytes) - used) want = sizeof(bytes) - used;
        memcpy(bytes, partial, used);
        ssize_t n = read(input, bytes + used, want);
        if(n <= 0){
            if(n == 0) used = 0; /* discard incomplete frame at writer EOF */
            else if(errno != EAGAIN && errno != EINTR){ perror("PCM read"); pcm_input_stop(); }
            break;
        }
        size_t total = used + (size_t)n;
        size_t complete = total / 4;
        for(size_t j = 0; j < complete * 2; j++){
            unsigned value = bytes[j*2] | ((unsigned)bytes[j*2+1] << 8);
            samples[done*2+j] = (int16_t)value;
        }
        done += (int)complete;
        used = total % 4;
        memcpy(partial, bytes + complete*4, used);
    }
    received += done;
    silence += frames - done;
}
