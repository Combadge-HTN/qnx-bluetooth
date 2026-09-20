# Python to Bluetooth PCM playback

All files are in `~/projects/qnx-bluetooth` on the Pi. No extra packages,
system audio configuration or reboot are needed. The existing Bluetooth
launcher requires root; the Python sender runs as qnxuser.

## Start playback

In terminal 1 on the Pi:

```sh
cd ~/projects/qnx-bluetooth
# Already created during setup; only needed if it has been removed:
test -p audio.pcm || mkfifo -m 600 audio.pcm
sudo sh ./run-radio.sh
```

Press `a` to discover/connect TWS Mini Speaker. Wait for `Stream established`,
then press `f`. Wait for `PCM FIFO ready` and `Stream started`.
When connecting over SSH use `ssh -tt` from an interactive terminal.

In terminal 2 on the Pi:

```sh
cd ~/projects/qnx-bluetooth
python3 play_pcm.py --tone
# Or play a compatible WAV file:
python3 play_pcm.py recording.wav
```

The tone lasts three seconds. All PCM output is attenuated by 32 in the
receiver, including WAV files. Speaker-generated chimes are independent.

For application integration, the driver accepts `QNX_PCM_VOLUME_SHIFT=0`
through `8` in its environment when `f` is pressed. The divisor is 2 raised
to that value: 0 gives full amplitude, 2 gives 1/4; the default 5 gives 1/32. This changes
PCM-file/stream volume only, not the built-in tone. Combadge's Bluetooth
supervisor selects 0 and preserves speech samples without a clipping boost.

Press `p` in terminal 1 to close the FIFO and pause. Press `f` to accept a new
writer again. Ctrl-C ends the session and restores Bluetooth pins/UART.
An existing writer may get BrokenPipeError when playback is stopped.

## Use from an application

With this project directory on Python's import path:

```python
from play_pcm import send_pcm

# Your generator yields bytes containing complete stereo frames.
send_pcm(my_pcm_chunks())
```

Format: **44100 Hz, two interleaved channels, signed 16-bit little-endian**.
Each frame is four bytes: left sample then right sample. Suggested chunks
are 441 frames (1764 bytes). The WAV reader accepts only this format and does
not resample or convert other files. Only one producer may write at a time.

The kernel FIFO provides bounded buffering and backpressure. The sender waits
up to ten seconds for a reader or for stalled writes. A generator that itself
blocks is outside that timeout. The receiver never blocks the Bluetooth event
loop and substitutes silence on underruns. Partial frames are retained between
reads and discarded at observed writer EOF. Senders should finish on complete
frames. At EOF, buffered data plays out and the Bluetooth session continues
streaming silence until paused; submitting bytes is not a speaker-drain ack.

This is application playback, not a SALSA device or system-wide default output.
No automatic reconnection or mixing is implemented. After disconnection,
reconnect and press `f` before restarting the producer.

## Validation and limitations

- Native build on QNX succeeded after a clean rebuild.
- Silent C tests passed for split frames, signed little-endian decoding,
  underrun silence, truncated writer EOF and a subsequent writer.
- Python generated and submitted a three-second tone. The receiver reported
  exactly 132300 input frames and subsequently silence; pause succeeded.
- On final shutdown, GPIO 24–27 and 29 were input/pull-down, UART LCR/MCR
  were zero, and the radio lock was removed. Wi-Fi was not changed.
- The user confirmed the repeated Python tone was audible and steady.
  Long-duration reliability is not established; this is an experimental driver.

An initial test terminal closure left temporary radio settings active. These
were recovered without a reboot. SIGHUP handling was added to main, and UART
startup now drains a bounded amount of stale receive data. Two startup attempts
asserted before a clean rebuild succeeded; the precise cause is unconfirmed.
The UART divisor from before the initial aborted session was unavailable;
recovery preserved its current value and restored the idle control registers.
Subsequent sessions explicitly set the required baud rate at startup.

`uart_probe --idle-after-abort` is a narrowly guarded manual recovery aid,
not a normal startup step. It must only be used after confirming no Bluetooth
process remains and powering off BT via GPIO29. Never use it on an active
session. Keep the launcher's refusal guards; do not bypass a stale lock blindly.
