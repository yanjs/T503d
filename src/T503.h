#pragma once
#include <libusb-1.0/libusb.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <linux/input-event-codes.h>

#include "T503_key_settings.inc"

#define T503_COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

static const int g_mapping_plus[] = {
    T503_BUTTON_PLUS
};
static const int g_mapping_minus[] = {
    T503_BUTTON_MINUS
};
static const int g_mapping_1[] = {
    T503_BUTTON_1
};
static const int g_mapping_2[] = {
    T503_BUTTON_2
};
static const int g_mapping_3[] = {
    T503_BUTTON_3
};
static const int g_mapping_4[] = {
    T503_BUTTON_4
};

#define T503_ID_VENDOR 0x8f2
#define T503_ID_PRODUCT 0x6811
#define T503_INTERFACE_1 1
#define T503_INTERFACE_2 2
#define T503_N_ENDPOINTS 2
#define T503_IO_BUFFER_SIZE 8

#define T503_MAX_X 4095
#define T503_MAX_Y 4095
#define T503_RESOLUTION_X 2
#define T503_RESOLUTION_Y 3
#define T503_MAX_PRESSURE 2047


struct t503_context {
    libusb_context *libusb_ctx;
    libusb_device_handle *libusb_handle;
    struct libusb_config_descriptor *libusb_config;
    struct libusb_transfer *libusb_transfers[T503_N_ENDPOINTS];

    uint8_t libusb_endpoint_addresses[T503_N_ENDPOINTS];

    struct libevdev *libevdev_dev;
    struct libevdev_uinput *libevdev_uidev;

    
    uint8_t buffer[T503_N_ENDPOINTS][T503_IO_BUFFER_SIZE];
    const int *prev_mapping;
    size_t prev_mapping_len;

};

int t503_init(struct t503_context *);
void t503_exit(struct t503_context *);
int t503_loop(struct t503_context *);
