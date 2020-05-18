# ShockAudio <sub>(Will probably change name later)</sub>

This firmware is used to make a network/bluetooth speaker using an ESP32. Plan is to make a custom PCB as well.

### Todo
- [ ] Test changes made in this new branch
- [ ] Fix bugs found during testing


- [ ] Sources should write directly to the context buffer, not first write to
  an intermediate buffer. This will safe quite some memory.
- [ ] Split mixer, so that it only mixes the data, not send it to the renderer.
  Or at least return the data somehow.
    - This is required to stream the data to other sinks as well.
- [ ] Think about how the sources are interacting with their context
    - Directly or through helper functions? 
    - Now it's a mix of both and it's a mess.
- [ ] Add some better status management (Playing, paused, stopped, etc.)
    - Pausing a source should preferably pause the task itself.
    - Maybe add a 'Waiting' status instead of 'stopped'.
    - When setting status to 'stopped' the task should stop. To start it again,
      the init function has to be called again.

Bluetooth:
- SBC is pretty good, see [2]
- [ ] Add some proper volume handling
- [ ] Add some proper metadata handling (with posibility to export data for e.g. display)
    - Now receiving metadata, but not saving yet
- [ ] Add ability for media control from sink
    - Can now play and pause (not tested yet)
- [ ] Take another look at the A2D codec config
- [ ] SSP

Tone:
- [ ] Add different waveforms?
    - It now generates a cleanish sine wave. Are other waveforms needed?
- [ ] Add a simple delay
- [ ] Add some structure to create melodies or tunes
- [ ] Add option to set the bits per sample?
    - It now always has a bits per sample of 8, which is probably fine.

Resampling/bitdepth:
- [ ] Take another look at upsampling, it is kinda buggy still
    - Especially when samplerates are not multiples of eachother.
- [ ] Downsampling?
    - Might not be needed, since it is always better to upsample other sources,
      than to downsample a high SR source.
    - It can be useful when streaming the data to another sink though. 
- [ ] Write code to transform bitdepth
- [ ] Move buffer_to_sample and sample_to_buffer functions to PCM
    - [ ] Fix endianness? (Will probably be fixed when using the new functions)

SDCard:
- [ ] Add option to have multiple files playing back
    - Can be useful to simultaneously play back music and some notification
      sound.
- [ ] Way to simply play back (multiple?) audio files, to signal information
- [ ] Add method to play back music from SDcard? 

Advanced:
- [ ] Add some wifi capabilities
- [ ] Add spotify connect support 
- [ ] Add some way for multiple devices to communicate and sync audio (a la sonos)
- [ ] Add battery management and notification sounds

### Interesting reads
- [[1]: Audio over Bluetooth: most detailed information about profiles, codecs, and devices](https://habr.com/en/post/456182/)
- [[2]: Bluetooth stack modifications to improve audio quality on headphones without AAC, aptX, or LDAC codecs](https://habr.com/en/post/456476/)
