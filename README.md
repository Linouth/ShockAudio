# ShockAudio <sub>(Will probably change name later)</sub>

This firmware is used to make a network/bluetooth speaker using an ESP32.
Plan is to make a custom PCB as well.



### Todo
Upgrade to new audio\_element system:
- [x] Write a generic audio\_element struct/system, which makes use of callback
  functions to do the actual work. (read/write buffers, init/deinit interfaces)
- [x] Convert sdcard source to the new system
    - Still a bit crude
- [x] Convert bluetooth source to the new system
    - [x] A2DP part
    - [x] AVRCP part
    - This needs to be implemented more (state change, save pos in info struct,
      etc.), but it is converted to the new system.
- [ ] Convert tone source to the new system
    - This needs a rewrite anyways
- [ ] For now Add a way to have multiple inputs/outputs
    - [x] Many > one will just be the mixer (e.g. `mixer_add_element()`). 
    - [ ] One > Many has to be a new element type? This will be necessary if you
      want to send the data to multiple streams (e.g. i2s and tcp).
- [x] Convert mixer to the new system and add support for multiple input streams
- [x] Update: Add 'user data' var to `io_t` struct which will hold the AEL
  `info_t` struct corresponding to that specific buffer
    - Done, _Needs testing though_
    - This is probably not the best way to do this. I am thinking about just
      taking the control and info parts out of the audio_element completely.
- [ ] ! Add a way to pause AEL threads (infinite loops from clogging the cpu)

General:
- [x] Sources should write directly to the context buffer, not first write to
  an intermediate buffer.
    - This will safe quite some memory.
    - This can be done now with the new `io_t` struct. Give the input of the
      next element the output of the previous element.
- [x] Split mixer, so that it only mixes the data, not send it to the renderer.
  Or at least return the data somehow.
    - This is required to stream the data to other sinks as well.
    - Done as a result of the new AEL system
- [x] Think about how the sources are interacting with their context
    - Directly or through helper functions? 
    - Now it's a mix of both and it's a mess.
    - Done as a result of the new AEL system
- [ ] Add some status management (Playing, paused, stopped, etc.)
    - Pausing a source should preferably pause the task itself.
    - When setting status to 'stopped' the task should stop. To start it again,
      the init function has to be called again.
- [ ] Create decoders (mp3, aac(?) and ogg vorbis (spotify))
    - Add codec\_type to info struct.
    - Have decoder just pass input to output if no decoding necessary.

Bluetooth:
- SBC is pretty good, see [2]
- [ ] Add some proper volume handling
- [ ] Add some proper metadata handling (with posibility to export data for e.g. display)
- [ ] Add ability for media control from sink
- [ ] SSP

Tone:
- [ ] Add different waveforms?
- [ ] Add a simple delay
- [ ] Add some structure to create melodies or tunes
- [ ] Add option to set the bits per sample?
    - It now always has a bits per sample of 8, which is probably fine.

Resampling/bitdepth:
- [ ] Take another look at upsampling.
- [ ] Downsampling?
    - Might not be needed, since it is always better to upsample other sources,
      than to downsample a high SR source.
    - It can be useful when streaming the data to another sink though. 
- [ ] Write code to transform bitdepth

SDCard:
- [ ] Add option to have multiple files playing back
    - Can be useful to simultaneously play back music and some notification
      sound.

Advanced:
- [ ] Add some wifi capabilities
    - Spotify?
    - Chromecast?
- [ ] Add spotify connect support 
- [ ] Add some way for multiple devices to communicate and sync audio (a la sonos)
- [ ] Add battery management and battery notification sounds

### Interesting reads
- [[1]: Audio over Bluetooth: most detailed information about profiles, codecs, and devices](https://habr.com/en/post/456182/)
- [[2]: Bluetooth stack modifications to improve audio quality on headphones without AAC, aptX, or LDAC codecs](https://habr.com/en/post/456476/)
