# ShockAudio <sub>(Will probably change name later)</sub>

This firmware is used to make a network/bluetooth speaker using an ESP32. Plan is to make a custom PCB as well.

*This is developed for the SHOCK committee of Scintilla*

### Todo
Basic:
- [x] Split i2s export and SD audio source into separate tasks
- [x] Add a state struct to pass to the tasks. This struct will contain flags for the audio sources to toggle and which buffers the sources can use. It should also add weights to the sources for later mixing.
- [x] Add pointer to ringbuffer to the audio\_write\_ringbuf function.
- [ ] Add support for multiple sample rates (stretch or shrink signal)
- [ ] Add support for multiple bit per sample (pad with zeros or remove data)
- [ ] Add basic mixing of channels **(Still needs testing)**
    - Tested and it's not yet working

Advanced:
- [ ] Add bluetooth sink as audio source (duh...)
    - [ ] (Wayyy later) Add more advanced codecs like aptX [Unofficial open source implementation](https://github.com/Arkq/openaptx)
- [ ] Add some wifi capabilities
- [ ] Add spotify connect support 

Maybe:
- [ ] Split into separate ESP-IDF components


### Interesting reads
- [Audio over Bluetooth: most detailed information about profiles, codecs, and devices](https://habr.com/en/post/456182/)
- [Bluetooth stack modifications to improve audio quality on headphones without AAC, aptX, or LDAC codecs](https://habr.com/en/post/456476/)
