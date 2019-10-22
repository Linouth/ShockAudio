# ShockAudio <sub>(Will probably change name later)</sub>

This firmware is used to make a network/bluetooth speaker using an ESP32. Plan is to make a custom PCB as well.

*This is developed for the SHOCK committee of Scintilla*

### Todo
Basic:
- [x] Split i2s export and SD audio source into separate tasks
- [x] Add a state struct to pass to the tasks. This struct will contain flags for the audio sources to toggle and which buffers the sources can use. It should also add weights to the sources for later mixing.
- [x] Add pointer to ringbuffer to the audio\_write\_ringbuf function.
    - I mean, it works but its pretty messy right now.
- [ ] Add support for multiple sample rates (stretch or shrink signal)
- [ ] Add support for multiple bit per sample (pad with zeros or remove data)
- [ ] Add basic mixing of channels **(Still needs testing with another source)**
    - Yeah... This does not really work yet. 
- [ ] Fix endianness

Advanced:
- [ ] Add bluetooth sink as audio source (duh...)
    - It is kind of working? Can connect, can receive data, data still needs to be reshaped
        - Not really actually
    - [ ] Add SSP
    - [ ] ~~(Wayyy later) Add more advanced codecs like aptX [Unofficial open source implementation](https://github.com/Arkq/openaptx)~~
        - SBC is actually really good, and probably superior to AptX (After modifications like [2])
- [ ] Rework all memory related code, it's a mess right now...
- [ ] Rework the whole modular system and split into ESPIDF components
- [ ] Add some wifi capabilities
- [ ] Add spotify connect support 


Straight forward tasks:
- [ ] Move audio\_source enum to some global header (config.h?)
- [ ] Get rid of audio\_state struct and make a PCM configuration struct
- [ ] Move audio.c stuff to renderer component
- [ ] Write function to resample audio data
- [ ] Write function to transform bits per sample
- [ ] Create Mixer component
- [ ] Add states to audio sources (Running, Stopped, Uninitialized, etc.)
    - With corresponding functions to set and get this state
    - Possibly suspend main tasks in the source when stopped / paused


### Interesting reads
- [[1]: Audio over Bluetooth: most detailed information about profiles, codecs, and devices](https://habr.com/en/post/456182/)
- [[2]: Bluetooth stack modifications to improve audio quality on headphones without AAC, aptX, or LDAC codecs](https://habr.com/en/post/456476/)
