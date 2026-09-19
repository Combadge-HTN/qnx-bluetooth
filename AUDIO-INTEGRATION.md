# Native audio integration investigation

System-wide routing remains blocked on the QNX 8 io-snd driver development
interface. No routing configuration was changed during this investigation.

## Evidence from the target

- io-snd loads sndrv-ctrl-usb.so and exposes the USB card as controlC0 and
  pcmC0D0p. Its configuration supports driver DLLs and software mixing.
- The USB DLL exports card_dll_init, card_dll_destroy, and dll_version, and
  imports ado_pcm_create, ado_subchn_buf_alloc, ado_subchn_buf_map,
  ado_pcm_get_hw_params, and dma_interrupt. Correct declarations, callback
  structures and version requirements are needed to implement another DLL.
- qnx-alsa-dev 8.0.5 supplies application headers. asound.h has format and
  parameter definitions but does not supply the QNX PCM message protocol or
  driver callback declarations needed for a compatible replacement service.
- libasound lacks snd_pcm_ioplug_create. A Linux ALSA external PCM plugin
  cannot be assumed to work on this runtime.
- io-snd exports ado_link_tap_stream, but the available headers contain no
  tap API. This symbol alone does not establish a usable application tap.
- The public QNX Raspberry Pi 4 BSP tree contains no audio driver source.
  Installed package searches did not identify an audio driver development kit.

## Required material

## Follow-up: SALSA inspection on the installed image

Direct disassembly of /usr/lib/libasound.so confirms that snd_config_top,
snd_config_load, snd_config_search, snd_config_make and snd_pcm_open_lconf
immediately return -6 (ENXIO). Their exported names do not indicate working
configuration or plugin support. No snd_pcm_ioplug_create or extplug interface
is exported, and no plugin-development headers are in qnx-alsa-dev.

snd_pcm_open resolves a device using snd_dev_get_device, opens it, and uses
ioctl calls and shared-memory handles to communicate with the audio service.
It does not load an external PCM plugin in this implementation.

asound_qnx.h does contain a PCM routing matrix API. Its matrix selects bits
for existing sink devices; it supplies no callback or registration interface
to implement a new sink. AFM declarations control existing service objects,
not a custom PCM consumer callback. No public playback-tap interface was found
in the supplied headers. The currently exposed devices are USB playback and
capture plus preferred aliases; no separate loopback or monitor is exposed.

Package searches, including the local qnx-core/qnx-extra directories, found
qnx-alsa runtime/debug/development packages, PortAudio, python3-sounddevice,
libsndfile and the multimedia sound output package. They did not identify a
custom SALSA sink/plugin SDK or PulseAudio/PipeWire package in the configured
package indexes. No packages were installed or indexes refreshed.

This matches upstream SALSA's documented omission of external PCM plugins
and configuration support:
https://github.com/tiwai/salsa-lib#readme
https://www.alsa-project.org/wiki/SALSA-Library

Conclusion: the installed SALSA development library cannot by itself register
our Bluetooth program as a system-wide output. System-wide integration is
not inherently impossible: a compatible native audio-service output driver,
a supported service-side playback tap/virtual device, or substantial changes
to the audio stack could enable it. The first two still need QNX interfaces
not supplied by the inspected package. A direct PCM feed remains the smaller
application-specific option, but has not been implemented by this inspection.

All inspection was read-only on the service. No sound was played, no routing
was changed, and no reboot or service restart occurred.

## Obtaining the missing native interface

Obtain QNX 8 io-snd driver headers, accompanying API documentation, and a
compatible audio controller sample through QNX Software Center. QNX confirms
that Raspberry Pi PCM and I2S audio samples are distributed there:
https://qnx.software/en/blog/2026/building-a-qnx-powered-cd-player

An old io-audio DDK is not a verified substitute. Exact SDK compatibility with
the target runtime must be checked before loading any new DLL.

## Implementation once the interface is available

1. Implement a virtual playback controller using io-snd's software mixer.
2. Transfer bounded PCM buffers to the existing Bluetooth process over local
   IPC, with defined underrun, disconnect, backpressure and shutdown handling.
3. Feed PCM into the existing SBC/A2DP encoder; negotiate the actual sample
   rate and retain a quiet test volume.
4. Test a separate native audio device with an ordinary SALSA application,
   including simultaneous application playback and recovery after disconnect.
5. Switch the preferred output only after successful validation, preserving
   the original route and providing a rollback command.

No reboot is planned. Any required service restart must first have a concrete
recovery procedure. Merely wrapping selected applications in LD_PRELOAD would
not fulfill system-wide routing and is not presented as a solution.
