/****************************************************************************
 *
 * midi.c - MIDI subsystem
 *
 * Copyright (C) 2014  Ricard Wanderlof <ricard2013@butoba.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ****************************************************************************/

#include <asoundlib.h>

#include "midi.h"
#include "debug.h"
#include <alloca.h>

/* ALSA related stuff */
int seq_port;
snd_seq_t *seq;

/* Initialize ALSA sequencer interface, and create MIDI port */
/* Return list of fds that main loop needs to poll() in order to detect
 * activity. */
/* Returned structure pointer is allocated using malloc. */
struct polls *
midi_init_alsa(void)
{
  struct polls *polls;
  int npfd;
  int i;

  if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
    dbgprintf("Couldn't open ALSA sequencer: %s\n", snd_strerror(errno));
    return NULL;
  }
  snd_seq_set_client_name(seq, "Nocturn");
  seq_port = snd_seq_create_simple_port(seq, "Nocturn port 1",
	 			        SND_SEQ_PORT_CAP_READ |
				        SND_SEQ_PORT_CAP_WRITE |
				        SND_SEQ_PORT_CAP_SUBS_READ |
				        SND_SEQ_PORT_CAP_SUBS_WRITE,
				        SND_SEQ_PORT_TYPE_APPLICATION);
  if (seq_port < 0) {
    dbgprintf("Couldn't create sequencer port: %s\n", snd_strerror(errno));
    return NULL;
  }

  /* Fetch poll descriptor(s) for MIDI input (normally only one) */
  npfd = snd_seq_poll_descriptors_count(seq, POLLIN);
  polls = (struct polls *) malloc(sizeof(struct polls) +
				  npfd * sizeof(struct pollfd));
  polls->npfd = npfd;
  snd_seq_poll_descriptors(seq, polls->pollfds, npfd, POLLIN);

  snd_seq_nonblock(seq, SND_SEQ_NONBLOCK);

  return polls;
}


/* Set up ALSA MIDI subscription according to supplied parameter. */
static int
subscribe(snd_seq_port_subscribe_t *sub)
{
  if (snd_seq_get_port_subscription(seq, sub) == 0) {
    dbgprintf("Connection between editor and device already established\n");
    return 0;
  }

  if (snd_seq_subscribe_port(seq, sub) < 0) {
    dbgprintf("Couldn't estabilsh connection between editor and device\n");
    return -1;
  }

  return 0;
}

/* Make bidirectional MIDI connection to specified remote device */
/* Remote device name is saved after first call (or rather the pointer to it,
 * so the actual string must not be deallocated); subsequent calls may use
 * NULL as the remote_device. */
int
midi_connect(const char *remote_device)
{
  int client;
  snd_seq_port_subscribe_t *sub;
  snd_seq_addr_t my_addr;
  snd_seq_addr_t remote_addr;
  static const char *saved_remote_device = "";

  if (remote_device)
    saved_remote_device = remote_device;

  client = snd_seq_client_id(seq);
  if (client < 0) {
    dbgprintf("Can't get client_id: %d\n", client);
    return client;
  }
  dbgprintf("Client address %d:%d\n", client, seq_port);

  snd_seq_port_subscribe_alloca(&sub);

  /* My address */
  my_addr.client = client;
  my_addr.port = seq_port;

  /* Other devices address */
  if (snd_seq_parse_address(seq, &remote_addr, saved_remote_device) < 0) {
    dbgprintf("Can't locate destination device %s\n", saved_remote_device);
    return -1;
  }

  /* We always attempt to set up subscription in both directions, regardless
   * of which error occurs when setting up the first direction. */

  /* Set up sender and destination in subscription. */
  snd_seq_port_subscribe_set_sender(sub, &my_addr);
  snd_seq_port_subscribe_set_dest(sub, &remote_addr);

  int res = subscribe(sub);

  /* And now, connection in other direction. */
  snd_seq_port_subscribe_set_sender(sub, &remote_addr);
  snd_seq_port_subscribe_set_dest(sub, &my_addr);

  int res2 = subscribe(sub);
  if (res == 0) res = res2; /* if first subscribe() had no error */

  return res;
}


/* Send control change message */
int
midi_send_control_change(int channel, int controller, int value)
{
  snd_seq_event_t sendev;
  snd_seq_ev_clear(&sendev);
  snd_seq_ev_set_subs(&sendev);
  snd_seq_ev_set_controller(&sendev, channel - 1, controller, value);
  snd_seq_ev_set_direct(&sendev);
  int ret = snd_seq_event_output_direct(seq, &sendev);
  dbgprintf("Ch %d:CC %d:%d\n", channel, controller, value);
}

/* Handle MIDI input. To be called when poll() call in main loop indicates
 * that data is available on our fd(s). */
void
midi_input(void)
{
  int midi_status;
  snd_seq_event_t *ev;
  ssize_t evlen;

  while (1)
  {
    midi_status = snd_seq_event_input(seq, &ev);
    dbgprintf("MIDI input status : %d\n", midi_status);
    if (midi_status < 0)
      break;
    evlen = snd_seq_event_length(ev);
    dbgprintf("MIDI event length %d\n", evlen);
    switch (ev->type) {
      case SND_SEQ_EVENT_CONTROLLER:
        dbgprintf("CC: ch %d, param %d, val %d\n", ev->data.control.channel + 1,
                ev->data.control.param, ev->data.control.value);
        /* Do something useful here */
        break;
      default:
        break;
    }
  }
}

/**************************** End of file midi.c ****************************/
