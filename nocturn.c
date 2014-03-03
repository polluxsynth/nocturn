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
#include <libusb.h> /* This also brings in thing like uint8_t etc */

static struct libusb_device_handle *devh = NULL;
uint16_t vendor_id = 0x1235;
uint16_t product_id = 0x000a;

/* Magical initiation strings.
 * From De Wet van Niekerk (dewert) - dvan.ca - dewert@gmail.com
 * (Github: dewert/nocturn-linux-midi)
 * The protocol was reverse-engineered by felicitus (timoahummel on github).
 * 
 * In fact there is nothing magical about this. 0xB0 is the MIDI status
 * byte for Control Change, and after that it's all a question of running
 * status.
 */
char *init_data[] = { "b00000", "28002b4a2c002e35", "2a022c722e30", "7f00" };

/*
 * CC definitions for Nocturn:
 *
 * From Nocturn:
 * CC64..71: Incrementors 1..8: Value 1 => increase, value 127 => decrease
 * (If more than one increase/decrease per message interval, rare, 
 * then we get 2,3,4 or 126,125,124, etc.)
 * CC72,73: LSB, MSB of slider (8 bits total: 7 in LSB, 1 in MSB)
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

int main(int argc, char **argv)
{
  int stat;
  struct libusb_device *dev;
  struct libusb_device_descriptor descr;
  uint8_t ep0, ep1;
  uint8_t rx_ep = -1, tx_ep = -1;
  int written;

  libusb_init(NULL);
  devh = libusb_open_device_with_vid_pid(NULL, vendor_id, product_id);
  printf("Got USB device: %p\n", devh);
  if (!devh) {
    printf("Couldn't find Nocturn, exiting\n");
    exit(1);
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
  printf("Interface 0: i/f 0 ep 1 %d\n", config0->interface[0].altsetting[0].endpoint[1].bEndpointAddress);

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
  printf("Interface 0: i/f 0 ep 0 %d\n", config1->interface[0].altsetting[0].endpoint[0].bEndpointAddress);
  printf("Interface 0: i/f 0 ep 1 %d\n", config1->interface[0].altsetting[0].endpoint[1].bEndpointAddress);

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
  
  /* Clean up */
  libusb_close(devh);
  libusb_exit(NULL);

  /* Be happy */
  return 0;
}
