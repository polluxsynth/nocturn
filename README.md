nocturn
=======

This is an attempt att providing an application to enable usage of the
Novation Nocturn controller in a Linux environment. See the bottom of this
page for support directly via ALSA in the Linux kernel.

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

Currently, only output from the Nocturn is supported. (However, as a test/demo,
the daemon lights up a couple of the LED rings on the Nocturn when it starts).

NOTE: The Linux kernel actually has support for Novation MIDI devices, although
at the time of writing not specifically for the Nocturn. This can be done
by adding the following to the Linux kernel's sound/usb/quirks-table.h

	{
		USB_DEVICE(0x1235, 0x000a),
		.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
			/* .vendor_name = "Novation", */
			/* .product_name = "Nocturn", */
			.ifnum = 0,
			.type = QUIRK_MIDI_RAW_BYTES
		}
	},

This has been has been tested on Linux 3.16, together with the xtor
Blofeld editor.
