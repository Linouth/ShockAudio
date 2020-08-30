idf_component_register(SRCS "audio_element.c" "sdcard_stream.c" "i2s_stream.c" "a2dp_stream.c"
                            "io.c" "mixer.c"
                       INCLUDE_DIRS "."
                       REQUIRES "sdcard bt")
