# ShockAudio <sub>(Will probably change name later)</sub>

This firmware is used to make a network/bluetooth speaker using an ESP32. Plan is to make a custom PCB as well.

### Todo
General:
- [x] Move audio\_source enum to some global header (config.h?)
    - Moved to audio.h global header
- [x] Get rid of audio\_state struct and make a PCM configuration struct
- [x] Move audio.c stuff to renderer component
- [x] Create general 'Audio Source' component with structs/enums for state etc
- [x] Move SD audio source to its own component
    - Done, ~~still needs testing though.~~
- [x] Add functions/macros to audio_source component to init, start, pause, stop all sources
    - Not all, but play and pause are state elements now.
- [ ] Look into separating I2S and Mixer from main loop/task
- [ ] Create Mixer component
- [ ] Add proper states to audio sources (Running, Stopped, Uninitialized, etc.)
    - With corresponding functions to set and get this state
    - Possibly suspend main tasks in the source when stopped / paused

Bluetooth:
- [ ] Change state on sink and source
- [ ] Add some proper volume handling
- [ ] Add some proper metadata handling (with posibility to export data for e.g. display)
- [ ] Add ability for media control from sink
- [ ] Clean code and sort callbacks and handler functions
- [ ] Better enum names?
- [ ] Take another look at the A2D codec config
- [ ] SSP
- [ ] ~~(Wayyy later) Add more advanced codecs like aptX [Unofficial open source implementation](https://github.com/Arkq/openaptx)~~
    - SBC is actually really good, and probably superior to AptX (After modifications like [2])

Tone:
- [ ] Fix.

Resampling/bitdepth:
- [ ] Take another look at upsampling, it is kinda buggy still
    - Especially when samplerates are not multiples of eachother.
- [ ] Downsampling? might not be needed
- [ ] Write code to transform bitdepth
- [ ] Fix endianness?

Advanced:
- [ ] Add some wifi capabilities
- [ ] Add spotify connect support 
- [ ] Add some way for multiple devices to communicate and sync audio (a la sonos)

### Interesting reads
- [[1]: Audio over Bluetooth: most detailed information about profiles, codecs, and devices](https://habr.com/en/post/456182/)
- [[2]: Bluetooth stack modifications to improve audio quality on headphones without AAC, aptX, or LDAC codecs](https://habr.com/en/post/456476/)
