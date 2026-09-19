#!/usr/bin/env python3
"""Send 44.1 kHz stereo S16_LE PCM to the project Bluetooth FIFO.

The receiver attenuates by 32. Exactly one writer may use the FIFO at a time.
"""
import argparse
import errno
import math
import os
from pathlib import Path
import select
import stat
import struct
import time
import wave

FIFO = Path(__file__).resolve().parent / 'audio.pcm'

def send_pcm(chunks, timeout=10):
    """Send iterable byte chunks, with bounded waits and receiver backpressure."""
    deadline = time.monotonic() + timeout
    while True:
        try:
            fd = os.open(FIFO, os.O_WRONLY | os.O_NONBLOCK | os.O_NOFOLLOW)
            break
        except OSError as exc:
            if exc.errno != errno.ENXIO or time.monotonic() >= deadline:
                raise
            time.sleep(0.05)
    try:
        if not stat.S_ISFIFO(os.fstat(fd).st_mode):
            raise ValueError('audio.pcm must be a named pipe')
        for chunk in chunks:
            if len(chunk) % 4:
                raise ValueError('Each chunk must contain complete stereo PCM frames')
            pending = memoryview(chunk)
            deadline = time.monotonic() + timeout
            while pending:
                try:
                    n = os.write(fd, pending)
                    pending = pending[n:]
                    deadline = time.monotonic() + timeout
                except BlockingIOError:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise TimeoutError('Bluetooth PCM receiver stopped consuming audio')
                    select.select([], [fd], [], min(remaining, 0.25))
    finally:
        os.close(fd)

def tone():
    for start in range(0, 44100 * 3, 441):
        data = bytearray()
        for i in range(start, start + 441):
            fade = min(1, i / 441, (44100 * 3 - 1 - i) / 441)
            value = round(16000 * fade * math.sin(2 * math.pi * 441 * i / 44100))
            data.extend(struct.pack('<hh', value, value))
        yield data

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('wav', nargs='?', help='PCM WAV: stereo, 44100 Hz, 16-bit')
    parser.add_argument('--tone', action='store_true', help='quiet three-second tone')
    args = parser.parse_args()
    if bool(args.wav) == args.tone:
        parser.error('choose a WAV file or --tone')
    if args.tone:
        send_pcm(tone())
    else:
        with wave.open(args.wav, 'rb') as source:
            if (source.getnchannels(), source.getsampwidth(), source.getframerate(), source.getcomptype()) != (2, 2, 44100, 'NONE'):
                parser.error('WAV must be uncompressed stereo 44100 Hz 16-bit PCM')
            send_pcm(iter(lambda: source.readframes(441), b''))
    print('PCM submitted; the receiver drains the pipe, then sends silence.')

if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, TimeoutError) as exc:
        raise SystemExit(str(exc))
