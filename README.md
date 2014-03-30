nocturn
=======

This is an attempt att providing an application to enable usage of the
Novation Nocturn controller in a Linux environment.

It is, and will be for some time, work in progress, but the hopes are that
someone might find this useful.

Basically, the Nocturn speaks MIDI in terms of the data sent and received,
sending CC when the knobs and buttons are operated, and receiving CC in order
to light up the LED rings and buttons. However, it doesn't appear as a USB MIDI
device when plugged in, just as a nondescript USB device, so an application
is required to convert the raw USB data to MIDI. After that, the MIDI data
can be used as any other MIDI controller, by patching it via ALSA.

Also note that, with all of the controls except the slider being incrementors
the Nocturn doesn't output absolute values, but instead control change values
with relative values, i.e. 1 for an increment of 1 and 127 for a corresponding
decrement. Higher values can be sent if the knobs are turned fast enough,
e.g. increment values of 2 or 3, or decrement values of 126 or 125.

The application could principally handle this, using an internal edit map
to convert the increments and decrements to absolute values, but nothing
like that has been implemented yet.
