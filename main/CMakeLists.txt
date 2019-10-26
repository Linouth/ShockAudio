# set(COMPONENT_SRCS "main.c")
# set(COMPONENT_ADD_INCLUDEDIRS "")

# register_component()

idf_component_register(SRCS "main.c"
                            # "sdcard.c"
                            # "tone.c"
                            # "bluetooth.c"
                            INCLUDE_DIRS ".")
