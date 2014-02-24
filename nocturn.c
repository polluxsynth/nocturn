#include <stdio.h>
#include <stdlib.h>
#include <libusb.h>

static struct libusb_device_handle *devh = NULL;
uint16_t vendor_id = 0x1235;
uint16_t product_id = 0x000a;

char *init_data[] = { "b00000", "28002b4a2c002e35", "2a022c722e30", "7f00" };


int digit(unsigned char hexdigit)
{
  return hexdigit - '0' - ((hexdigit > '9') ? ('a' - ('9' + 1)) : 0);
}

int byte(const char *string)
{
  return (digit(string[0]) << 4) | digit(string[1]);
}

int send_hexdata(libusb_device_handle *devh, const char *string, int *written)
{
  unsigned char buf[80];
  unsigned char *p = buf;

  printf("%s: to send %s\n", __func__, string);
  while (*string) {
    *p++ = byte(string);
    string += 2;
  }
  printf("Sending %d bytes: %d %d %d ...\n", p - buf, buf[0], buf[1], buf[2]);

  return libusb_interrupt_transfer(devh, 2, buf, p - buf, written, 500);
}

int main(int argc, char **argv)
{
  int stat;
  struct libusb_device *dev;
  struct libusb_device_descriptor descr;

  libusb_init(NULL);
  devh = libusb_open_device_with_vid_pid(NULL, vendor_id, product_id);
  printf("Got USB device: %p\n", devh);

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
  printf("Interface 1: #altsettings %d\n", config1->interface[0].num_altsetting);
  printf("Interface 1: i/f no %d\n", config1->interface[0].altsetting[0].bInterfaceNumber);
  printf("Interface 1: i/f 1 endpoints %d\n", config1->interface[0].altsetting[0].bNumEndpoints);
  printf("Interface 1: i/f 0 ep 0 %d\n", config1->interface[0].altsetting[0].endpoint[0].bEndpointAddress);
  printf("Interface 1: i/f 0 ep 1 %d\n", config1->interface[0].altsetting[0].endpoint[1].bEndpointAddress);


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

  int written;
  stat = send_hexdata(devh, init_data[0], &written);
  if (!stat) {
    printf("Wrote %d bytes\n", written);
    stat = send_hexdata(devh, init_data[1], &written);
  }
  if (!stat) {
    printf("Wrote %d bytes\n", written);
    stat = send_hexdata(devh, init_data[2], &written);
  }
  if (!stat) {
    printf("Wrote %d bytes\n", written);
    stat = send_hexdata(devh, init_data[3], &written);
  }
  if (!stat)
    printf("Wrote %d bytes\n", written);
  if (stat < 0) {
    printf("sending initial usb data: %d\n", stat);
    exit(2);
  }

  stat = send_hexdata(devh, "4800", &written);
  if (stat < 0) {
    printf("sending test usb data: %d\n", stat);
    exit(2);
  }
  stat = send_hexdata(devh, "4060", &written);
  if (stat < 0) {
    printf("sending test usb data: %d\n", stat);
    exit(2);
  }
  printf("Wrote %d bytes\n", written);

  int showall = 0;
  while (1) {
    unsigned char buf[10] = { 0 };
    int read;
    stat = libusb_interrupt_transfer(devh, 129, buf, 10, &read, 0);
    if (stat == 0) {
      if (showall || buf[1] < 96 || buf[1] > 103)
        /* 96..103 are knobs pressed, but often generate random events */
        printf("Read %d bytes: %d %d %d %d %d %d\n", read, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    } else
      printf("Read stat %d\n", stat);
  }
  
  


  libusb_close(devh);
  libusb_exit(NULL);

  return 0;
}
