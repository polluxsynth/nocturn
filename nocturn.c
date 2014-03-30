/****************************************************************************
 * nocturn : Simple Linux communication application for the Novation Nocturn
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

#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <libusb.h> /* This also brings in thing like uint8_t etc */

static struct libusb_device_handle *devh = NULL;
uint16_t vendor_id = 0x1235;
uint16_t product_id = 0x000a;

/* Magical initiation strings.
 * From De Wet van Niekerk (dewert) - dvan.ca - dewert@gmail.com
 * (Github: dewert/nocturn-linux-midi)
 * The protocol was reverse-engineered by Timo A. Hummel (felicitus on github).
 * 
 * In fact there is nothing magical about this. 0xB0 is the MIDI status
 * byte for Control Change, and after that it's all a question of running
 * status, i.e. the whole set of strings is just a series of control change
 * messages.
 * The question is then what the contol change data actually does. I have not
 * taken a closer look at the CC numbers used to see if they for instance
 * overlap with CC numbers used to control the LED rings etc on the Nocturn.
 * At least one of them seems to affect some the timeout of some messages
 * sent by the Nocturn. But apart from that, this "initialization" is not
 * necessary to get the Nocturn working, and can be omitted.
 */
char *init_data[] = { "b00000", "28002b4a2c002e35", "2a022c722e30", "7f00" };

/*
 * CC definitions for Nocturn:
 *
 * From Nocturn:
 * CC64..71: Incrementors 1..8: Value 1 => increase, value 127 => decrease
 * (If more than one increase/decrease per message interval, rare, 
 * then we get 2,3,4 or 126,125,124, etc.)
 * CC72: slider (7 bits)
 * CC73: slider ? (Arbitrarily 0 or 64 when moving slider)
 * CC74: Speed dial incrementor
 * CC81: Speed dial push (0 = up, 127 = down)
 * CC96..103: Incrementor push/touch (0 = up, 127 = down)
 * CC112..127: Buttons 1..8 upper row, 1..8 lower row (0 = up, 127 = down)
 *
 * To Nocturn:
 * CC64..71 Incrementor LED ring value 0..127
 * CC72..79: LED ring mode: (values are in high nybble of value byte)
 *           0 = show ring from min to value
 *           16 = show ring from max to value
 *           32 = show ring from 0 (center) to value (up or down)
 *           48 = show ring from 0 (center) to value (both directions)
 *           64 = single diode at value
 *           80 = inverted, i.e. all but single diode at value
 * CC80     Speed dial incrementor LED ring value 0..127
 * CC81     Speed dial LED ring mode (see above)
 * CC112..127: Button LEDs: 0 = off, != 0 = on
 */


int digit(uint8_t hexdigit)
{
  return hexdigit - '0' - ((hexdigit > '9') ? ('a' - ('9' + 1)) : 0);
}

int byte(const char *string)
{
  return (digit(string[0]) << 4) | digit(string[1]);
}

void event(int status, int chan, int data1, int data2)
{
  if (status == 176) {
    /* 96 .. 103 range is knob 1..8 presses which seem to be very jittery. */
    if (data1 < 96 || data1 > 103)
      printf("Status %d (chan %d): %d,%d\n", status, chan, data1, data2);
  }
}

enum midi_state { STATUS, DATA1, DATA2 };

void process(int data)
{
  static enum midi_state state = STATUS;
  static int status = -1;
  static int chan = -1;
  static int data1;
  data &= 0xff; /* get rid of sign bit from potential cast */

  if (data & 0x80) {
    status = data & 0xf0;
    chan = data & 0x0f;
    state = DATA1;
  } else 
    /* data bytes */
    switch (state) {
      case STATUS: break; /* Shouldn't happen */
      case DATA1: data1 = data; state = DATA2; break;
      case DATA2: event(status, chan, data1, data);
                  state = DATA1;
                  break;
      default: break;
  }
}

void process_buffer(const char *data, int len)
{
  while (len--)
    process(*data++);
}

int send_data(libusb_device_handle *devh, uint8_t endpoint,
              uint8_t *buf, int len, int *written)
{
  return libusb_interrupt_transfer(devh, endpoint, buf, len, written, 500);
}

int send_hexdata(libusb_device_handle *devh, uint8_t endpoint,
                 const char *string, int *written)
{
  uint8_t buf[80];
  uint8_t *p = buf;

  printf("%s: to send %s\n", __func__, string);
  while (*string) {
    *p++ = byte(string);
    string += 2;
  }
  printf("Sending %d bytes: %d %d %d ...\n", p - buf, buf[0], buf[1], buf[2]);

  return send_data(devh, endpoint, buf, p - buf, written);
}

void rx_cb(struct libusb_transfer *transfer)
{
  int *resubmit = transfer->user_data;
  *resubmit = 1;

  /* We printout buffer either if rx'd >= 3 bytes, and not CC96..103, _or_
   * if we get something other than a timeout event. */
  if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
    process_buffer(transfer->buffer, transfer->actual_length);
#if 0
    int i; for (i = 0; i < transfer->actual_length; i++)
      printf("%d ", transfer->buffer[i]);
    printf("\n");
#endif
  }
}


int main(int argc, char **argv)
{
  int stat;
  struct libusb_device *dev;
  struct libusb_device_descriptor descr;
  uint8_t ep0, ep1;
  uint8_t rx_ep = -1, tx_ep = -1;
  int written;
  libusb_context *ctx;
  int resubmit;

  libusb_init(&ctx);
  devh = libusb_open_device_with_vid_pid(NULL, vendor_id, product_id);
  printf("Got USB device: %p\n", devh);
  if (!devh) {
    printf("Couldn't find Nocturn, exiting\n");
    exit(2);
  }

  dev = libusb_get_device(devh);
  if (!dev) {
    printf("getting usb device: %d\n", stat);
    exit(2);
  }
  stat = libusb_get_device_descriptor(dev, &descr);
  if (!dev) {
    printf("getting usb device descriptor: %d\n", stat);
    exit(2);
  }
  printf("Descr: vendor %04x, product %04x\n",
         descr.idVendor, descr.idProduct);
  printf("Configurations: %d\n", descr.bNumConfigurations);

  struct libusb_config_descriptor *config0;
  stat = libusb_get_config_descriptor(dev, 0, &config0);
  if (stat < 0) {
    printf("getting usb configuration descriptor: %d\n", stat);
    exit(2);
  }
  printf("Configuration 0: interfaces %d\n", config0->bNumInterfaces);
  printf("Interface 0: #altsettings %d\n", config0->interface[0].num_altsetting);
  printf("Interface 0: i/f no %d\n", config0->interface[0].altsetting[0].bInterfaceNumber);
  printf("Interface 0: i/f 0 endpoints %d\n", config0->interface[0].altsetting[0].bNumEndpoints);
  printf("Interface 0: i/f 0 ep 0 %d\n", config0->interface[0].altsetting[0].endpoint[0].bEndpointAddress);
  printf("Interface 0: i/f 0 ep 0 poll interval %d\n", config0->interface[0].altsetting[0].endpoint[0].bInterval);
  printf("Interface 0: i/f 0 ep 1 %d\n", config0->interface[0].altsetting[0].endpoint[1].bEndpointAddress);
  printf("Interface 0: i/f 0 ep 1 poll interval %d\n", config0->interface[0].altsetting[0].endpoint[1].bInterval);

  struct libusb_config_descriptor *config1;
  stat = libusb_get_config_descriptor(dev, 1, &config1);
  if (stat < 0) {
    printf("getting usb configuration descriptor: %d\n", stat);
    exit(2);
  }
  printf("Configuration 1: interfaces %d\n", config1->bNumInterfaces);
  printf("Interface 0: #altsettings %d\n", config1->interface[0].num_altsetting);
  printf("Interface 0: i/f no %d\n", config1->interface[0].altsetting[0].bInterfaceNumber);
  printf("Interface 0: i/f 1 endpoints %d\n", config1->interface[0].altsetting[0].bNumEndpoints);
  printf("Interface 0: i/f 1 ep 0 %d\n", config1->interface[0].altsetting[0].endpoint[0].bEndpointAddress);
  printf("Interface 0: i/f 1 ep 0 poll interval %d\n", config1->interface[0].altsetting[0].endpoint[0].bInterval);
  printf("Interface 0: i/f 1 ep 1 %d\n", config1->interface[0].altsetting[0].endpoint[1].bEndpointAddress);
  printf("Interface 0: i/f 1 ep 1 poll interval %d\n", config1->interface[0].altsetting[0].endpoint[1].bInterval);

  /* We know empirically that it is config1 that is the one we need
   * for communication, so extract the endpoint addresses from this one. */

  ep0 = config1->interface[0].altsetting[0].endpoint[0].bEndpointAddress;
  ep1 = config1->interface[0].altsetting[0].endpoint[1].bEndpointAddress;
  /* bit 7 set indicates a receiving endpoint */
  if (ep0 & 128) rx_ep = ep0; else tx_ep = ep0;
  if (ep1 & 128) rx_ep = ep1; else tx_ep = ep1;
  if (tx_ep < 0 || rx_ep < 0) {
    printf("Failed to set rx and tx endpoints\n");
    exit(2);
  }

  /* Set configuration #1 */
  stat = libusb_set_configuration(devh, 1);
  if (stat < 0) {
    printf("setting usb configuration: %d\n", stat);
    exit(2);
  }

  libusb_detach_kernel_driver(devh, 0); /* need this ? */
  stat = libusb_claim_interface(devh, 0);
  if (stat < 0) {
    printf("claiming usb interface: %d\n", stat);
    exit(2);
  }

  /* Now we're set up and ready to communicate */

  /* First send the magical initiation strings */
#if 0
  stat = send_hexdata(devh, tx_ep, init_data[0], &written);
  if (!stat) {
    printf("Wrote %d bytes\n", written);
    stat = send_hexdata(devh, tx_ep, init_data[1], &written);
  }
  if (!stat) {
    printf("Wrote %d bytes\n", written);
    stat = send_hexdata(devh, tx_ep, init_data[2], &written);
  }
  if (!stat) {
    printf("Wrote %d bytes\n", written);
    stat = send_hexdata(devh, tx_ep, init_data[3], &written);
  }
  if (!stat)
    printf("Wrote %d bytes\n", written);
  if (stat < 0) {
    printf("sending initial usb data: %d\n", stat);
    exit(2);
  }
#endif

#if 1
  stat = send_hexdata(devh, tx_ep, "b04800", &written);
  if (stat < 0) {
    printf("sending test usb data: %d\n", stat);
    exit(2);
  }
#endif
  stat = send_hexdata(devh, tx_ep, "b04060", &written);
  if (stat < 0) {
    printf("sending test usb data: %d\n", stat);
    exit(2);
  }
  printf("Wrote %d bytes\n", written);

#if 1
  stat = send_hexdata(devh, tx_ep, "b05130", &written);
  stat = send_hexdata(devh, tx_ep, "b05030", &written);
#endif
  printf("Wrote %d bytes\n", written);
  
#if 0
  int i;
  for (i = 64; i < 128; i++) {
    uint8_t cc[3] = { 176, i, 0x50 };
    stat = send_data(devh, tx_ep, cc, 3, &written);
    cc[1] = 0x50;
    stat = send_data(devh, tx_ep, cc, 3, &written);
    if (stat < 0) {
      printf("sending usb data: %d\n", stat);
      exit(2);
    }
  }
#endif

  uint8_t buf[10] = { 0 };

  /* Only on stack if we won't return before transfer completes. */
  printf("Alloc transfer\n");
  struct libusb_transfer *transfer = libusb_alloc_transfer(0);

  printf("Fill transfer\n");
  libusb_fill_interrupt_transfer(transfer,
                                 devh,
                                 rx_ep,
                                 buf, 10,
                                 rx_cb, &resubmit,
                                 100);
                           
  printf("Submit transfer\n");      
  stat = libusb_submit_transfer(transfer);
  if (stat != 0) {
    printf("submitting transfer: %d\n", stat);
    exit(2);
  }

  resubmit = 0;


  struct timeval zero_tv = { 0 };
  printf("Now for main loop\n");
  while (1) {
    struct timeval tv;
    int timeout_ms;
    static int first_time = 1;

    /* Set up polling */
    const struct libusb_pollfd **libusb_pollfds = libusb_get_pollfds(ctx);
#define POLLFDS 10
    struct pollfd pollfds[POLLFDS];
    const struct libusb_pollfd **usb_pollfd = libusb_pollfds;
    int fds;
    for (fds = 0; fds < POLLFDS; fds++) {
      if (!*usb_pollfd) break;
      pollfds[fds].fd = (*usb_pollfd)->fd;
      pollfds[fds].events = (*usb_pollfd)->events;
      if (first_time)
        printf("%d: fd %d events %d\n", fds, pollfds[fds].fd, pollfds[fds].events);
      usb_pollfd++;
    }
    if (first_time)
      printf("%d pollfd%s from libusb\n", fds, fds == 1 ? "" : "s");

    /* figure out next timeout. Not really needed for Linux w/ timerfd supp. */
    int timeouts = libusb_get_next_timeout(ctx, &tv);
    if (timeouts < 0) {
      printf("getting next usb timeout: %d\n", timeouts);
      exit(2);
    }
#if 0 /* we probably don't need this, as we'll call handle_events_timeout
       * unconditionally after poll() anyway */
    if (timeouts && !memcmp(&tv, &zero_tv, sizeof(struct timeval))) {
      printf("mainloop: libusb timed out before poll\n");
      libusb_handle_events_timeout(ctx, &zero_tv);
    }
#endif
    if (timeouts) /* timeout set by get_next_timeout */
      timeout_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    else
      timeout_ms = -1; /* infinite timeout, since libusb didn't say */

    /* In practice, poll will return fairly quickly with one POLLOUT fd
     * so we don't want to spam debug with meaningless messages */
    /* printf("mainloop: timeout %d ms\n", timeout_ms); */
    int pollstat = poll(pollfds, fds, timeout_ms);
    if (pollstat < 0) {
      perror("polling usb fds");
      exit(2);
    }
#if 0 /* same here, the output fd is usually available within 100 ms or so */
    printf("mainloop: pollstat %d\n", pollstat);
    if (pollstat) {
      int i;
      for (i = 0; i < fds; i++)
        printf("fd %d: fd %d, revents %d\n", i, pollfds[i].fd, pollfds[i].revents);
    }
#endif
    /* No matter if we get data or timed out, we call libusb */
    libusb_handle_events_timeout(ctx, &zero_tv);

    if (resubmit) {
      resubmit = 0;
      /* printf("Resubmit transfer\n"); */
      stat = libusb_submit_transfer(transfer);
      if (stat != 0) {
        printf("submitting transfer: %d\n", stat);
        exit(2);
      }
    }
    free(libusb_pollfds);
    first_time = 0;
  }

#if 0
  while (1) {
    libusb_handle_events(ctx);
    if (resubmit) {
      resubmit = 0;
      /* printf("Resubmit transfer\n"); */
      stat = libusb_submit_transfer(transfer);
      if (stat != 0) {
        printf("submitting transfer: %d\n", stat);
        exit(2);
      }
    }
  }
#endif

  libusb_free_transfer(transfer);


#if 0
  int showall = 0;
  while (1) {
    uint8_t buf[10] = { 0 };
    int read;
    stat = libusb_interrupt_transfer(devh, rx_ep, buf, 10, &read, 0);
    if (stat == 0) {
      if (showall || buf[1] < 96 || buf[1] > 103)
        /* 96..103 are knobs pressed, but often generate random events */
        printf("Read %d bytes: %d %d %d %d %d %d\n", read, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    } else
      printf("Read stat %d\n", stat);
  }
#endif
  
  /* Clean up */
  libusb_close(devh);
  libusb_exit(ctx);

  /* Be happy */
  return 0;
}
