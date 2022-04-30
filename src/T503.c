#include "T503.h"
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <stdarg.h>


static volatile int s_should_exit = 0;

static int t503_init_libusb(struct t503_context *);
static void t503_exit_libusb(struct t503_context *);
static int t503_init_libevdev(struct t503_context *);
static void t503_exit_libevdev(struct t503_context *);

static void sigint_handler(int sig); 
static void transfer_cb(struct libusb_transfer *);
static void t503_parse_transfer(struct libusb_transfer *);


#ifdef NDEBUG
#define debugf
#else
static inline int debugf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int retn = vfprintf(stdout, fmt, args);
    va_end(args);
    return retn;
}
#endif

static inline int errorf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int retn = vfprintf(stderr, fmt, args);
    va_end(args);
    return retn;
}

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif


struct HID_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdHID;
    uint8_t bCountryCode;
    uint8_t bNumDescriptors;
    uint8_t bDescriptorType2;
    uint16_t wDescriptorLength;
}
#if defined(__GNUC__)
__attribute__ ((packed))
#endif
;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif



static void t503_exit_libusb(struct t503_context *ctx) {
    for (size_t i = 0; i < T503_N_ENDPOINTS; i++) {
        assert(ctx->libusb_transfers[i]);
        libusb_free_transfer(ctx->libusb_transfers[i]);
    }

    assert(ctx->libusb_handle);
    libusb_release_interface(ctx->libusb_handle, T503_INTERFACE_2);
    libusb_release_interface(ctx->libusb_handle, T503_INTERFACE_1);
    libusb_close(ctx->libusb_handle);

    assert(ctx->libusb_config);
    libusb_free_config_descriptor(ctx->libusb_config);
    assert(ctx->libusb_ctx);
    libusb_exit(ctx->libusb_ctx);
}

static void t503_exit_libevdev(struct t503_context *ctx) {
    assert(ctx->libevdev_uidev);
    assert(ctx->libevdev_dev);
    libevdev_uinput_destroy(ctx->libevdev_uidev);
    libevdev_free(ctx->libevdev_dev);
}

void t503_exit(struct t503_context *ctx) {
    t503_exit_libevdev(ctx);
    t503_exit_libusb(ctx);
}

static void sigint_handler(int sig) {
    debugf("SIGINT detected, exiting the loop.\n");
    s_should_exit = 1;
}


static void display_device_descriptor(struct libusb_device_descriptor const *desc) {
    debugf("Device Descriptor:\n");

    debugf("bLength: %d\n", desc->bLength);
    debugf("bDescriptorType: %d\n", desc->bDescriptorType);
    debugf("bcdUSB: %x\n", desc->bcdUSB);
    debugf("bDeviceClass: %d\n", desc->bDeviceClass);
    debugf("bDeviceSubClass: %d\n", desc->bDeviceSubClass);

    debugf("bDeviceProtocol: %d\n", desc->bDeviceProtocol);
    debugf("bMaxPacketSize0: %d\n", desc->bMaxPacketSize0);
    debugf("idVendor: %x\n", desc->idVendor);
    debugf("idProduct: %x\n", desc->idProduct);
    debugf("bcdDevice: %x\n", desc->bcdDevice);

    debugf("iManufacturer: %x\n", desc->iManufacturer);
    debugf("iProduct: %x\n", desc->iProduct);
    debugf("iSerialNumber: %x\n", desc->iSerialNumber);
    debugf("bNumConfigurations: %x\n\n", desc->bNumConfigurations);
}

static void display_config_descriptor(struct libusb_config_descriptor const *desc) {
    debugf("Configuration Descriptor:\n");

    debugf("bLength: %d\n", desc->bLength);
    debugf("bDescriptorType: %d\n", desc->bDescriptorType);
    debugf("wTotalLength: %d\n", desc->wTotalLength);
    debugf("bNumInterfaces: %d\n", desc->bNumInterfaces);
    debugf("bConfigurationValue: %d\n", desc->bConfigurationValue);

    debugf("iConfiguration: %d\n", desc->iConfiguration);
    debugf("bmAttributes: %d\n", desc->bmAttributes);
    debugf("MaxPower: %d\n", desc->MaxPower);

    debugf("extra_length: %d\n\n", desc->extra_length);
}

static void display_interface_descriptor(struct libusb_interface_descriptor const *conf) {
    debugf("Interface Descriptor:\n");

    debugf("bLength: %d\n", conf->bLength);
    debugf("bDescriptorType: %d\n", conf->bDescriptorType);
    debugf("bInterfaceNumber: %d\n", conf->bInterfaceNumber);
    debugf("bAlternateSetting: %d\n", conf->bAlternateSetting);
    debugf("bNumEndpoints: %d\n", conf->bNumEndpoints);

    debugf("bInterfaceClass: %d\n", conf->bInterfaceClass);
    debugf("bInterfaceSubClass: %d\n", conf->bInterfaceSubClass);
    debugf("bInterfaceProtocol: %d\n", conf->bInterfaceProtocol);
    debugf("iInterface: %d\n", conf->iInterface);

    debugf("extra_length: %d\n\n", conf->extra_length);
}

static void display_endpoint_descriptor(struct libusb_endpoint_descriptor const *desc) {
    debugf("Endpoint Descriptor:\n");

    debugf("bLength: %d\n", desc->bLength);
    debugf("bDescriptorType: %d\n", desc->bDescriptorType);
    debugf("bEndpointAddress: %d\n", desc->bEndpointAddress);
    debugf("bEndpointAddress: endpoint number %d\n",
        desc->bEndpointAddress & LIBUSB_ENDPOINT_ADDRESS_MASK);
    debugf("bEndpointAddress: Is direction IN? %d\n",
        (desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN);
    debugf("bmAttributes: %d\n", desc->bmAttributes);
    debugf("bmAttributes: endpoint transfer type %d\n",
        desc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK);
    debugf("bmAttributes: synchronization type %d\n",
        (desc->bmAttributes & LIBUSB_ISO_SYNC_TYPE_MASK) >> 2);
    debugf("bmAttributes: usage type %d\n",
        (desc->bmAttributes & LIBUSB_ISO_USAGE_TYPE_MASK) >> 4);
    debugf("wMaxPacketSize: %d\n", desc->wMaxPacketSize);

    debugf("bInterval: %d\n", desc->bInterval);
    debugf("bRefresh: %d\n", desc->bRefresh);
    debugf("bSynchAddress: %d\n", desc->bSynchAddress);

    debugf("extra_length: %d\n\n", desc->extra_length);
}

static void display_HID_descriptor(struct HID_descriptor const *desc) {
    debugf("HID Descriptor:\n");

    debugf("bLength: %d\n", desc->bLength);
    debugf("bDescriptorType: %d\n", desc->bDescriptorType);
    debugf("bcdHID: %d\n", desc->bcdHID);
    debugf("bCountryCode: %d\n", desc->bCountryCode);
    debugf("bNumDescriptors: %d\n", desc->bNumDescriptors);

    debugf("bDescriptorType2: %d\n", desc->bDescriptorType2);
    debugf("wDescriptorLength: %d\n\n", desc->wDescriptorLength);

}

static void display_transfer(struct libusb_transfer *transfer) {
    if (transfer == NULL) {
        debugf("No transfer... Why?\n");
    } else {
        debugf("struct libusb_transfer:\n");
        debugf("flags: %x \n", transfer->flags);
        debugf("endpoint: %x \n", transfer->endpoint);
        debugf("type: %x \n", transfer->type);
        debugf("timeout: %d \n", transfer->timeout);
        debugf("status: %d \n", transfer->status);

        debugf("length: %d \n", transfer->length);
        debugf("actual_length: %d \n", transfer->actual_length);
        debugf("buffer: %p \n", transfer->buffer);

        for (size_t i = 0; i < transfer->actual_length; i++){
            debugf("%02x", transfer->buffer[i]);
        }
        debugf("\n");
    }
    return;
}

static void transfer_cb(struct libusb_transfer *transfer) {
    display_transfer(transfer);

    switch (transfer->status) {
    case LIBUSB_TRANSFER_CANCELLED:
        break;
    case LIBUSB_TRANSFER_COMPLETED:
        t503_parse_transfer(transfer);
        libusb_submit_transfer(transfer);
        break;
    default:
        debugf("Unhandled status: %d\n", transfer->status);
    }
}

static int t503_init_libusb(struct t503_context *ctx) {
    int retn = libusb_init(&ctx->libusb_ctx);
    if (retn) goto error_libusb_init;

    libusb_device **devices;
    libusb_device *device = NULL;
    retn = libusb_get_device_list(ctx->libusb_ctx, &devices);
    if (retn < 0) goto error_libusb_get_device_list;
    size_t ndevices = retn;

    for (size_t i = 0; i < ndevices; i++) {
        libusb_device *curr_device = devices[i];
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(curr_device, &desc);
        if (desc.idVendor == T503_ID_VENDOR
            && desc.idProduct == T503_ID_PRODUCT) {
                device = curr_device;
                display_device_descriptor(&desc);
                break;
        }
    }
    if (!device) {
        retn = 1;
        goto error_device_not_found;
    }

    retn = libusb_get_config_descriptor(device, 0, &ctx->libusb_config);
    if (retn < 0) goto error_libusb_get_config_descriptor;
    display_config_descriptor(ctx->libusb_config);

    display_interface_descriptor(ctx->libusb_config->interface[0].altsetting);
    display_interface_descriptor(ctx->libusb_config->interface[1].altsetting);
    display_interface_descriptor(ctx->libusb_config->interface[2].altsetting);
    
    display_endpoint_descriptor(&ctx->libusb_config->interface[0].altsetting->endpoint[0]);
    display_endpoint_descriptor(&ctx->libusb_config->interface[0].altsetting->endpoint[1]);
    display_endpoint_descriptor(&ctx->libusb_config->interface[1].altsetting->endpoint[0]);
    display_endpoint_descriptor(&ctx->libusb_config->interface[2].altsetting->endpoint[0]);

    display_HID_descriptor((const struct HID_descriptor *)
        ctx->libusb_config->interface[1].altsetting->extra);
    display_HID_descriptor((const struct HID_descriptor *)
        ctx->libusb_config->interface[2].altsetting->extra);

    libusb_device_handle *handle;
    retn = libusb_open(device, &handle);
    if (retn) goto error_libusb_open;
    ctx->libusb_handle = handle;

    unsigned char buf[1024] = {};
    libusb_get_string_descriptor_ascii(ctx->libusb_handle, 0, buf, sizeof(buf));
    debugf("string 0: %s\n", buf);
    libusb_get_string_descriptor_ascii(ctx->libusb_handle, 1, buf, sizeof(buf));
    debugf("string 1: %s\n", buf);
    libusb_get_string_descriptor_ascii(ctx->libusb_handle, 2, buf, sizeof(buf));
    debugf("string 2: %s\n", buf);
    libusb_get_string_descriptor_ascii(ctx->libusb_handle, 3, buf, sizeof(buf));
    debugf("string 3: %s\n", buf);

    retn = libusb_set_auto_detach_kernel_driver(handle, 1);
    if (retn) goto error_libusb_set_auto_detach_kernel_driver;

    retn = libusb_claim_interface(handle, T503_INTERFACE_1);
    if (retn) goto error_libusb_claim_interface_1;
    retn = libusb_claim_interface(handle, T503_INTERFACE_2);
    if (retn) goto error_libusb_claim_interface_2;


    ctx->libusb_endpoint_addresses[0] = ctx->libusb_config->interface[T503_INTERFACE_1].altsetting
                                         ->endpoint[0].bEndpointAddress;
    ctx->libusb_endpoint_addresses[1] = ctx->libusb_config->interface[T503_INTERFACE_2].altsetting
                                         ->endpoint[0].bEndpointAddress;


    for (size_t i = 0; i < T503_N_ENDPOINTS; i++) {
        ctx->libusb_transfers[i] = libusb_alloc_transfer(0);
        if (!ctx->libusb_transfers[i]) {
            for (size_t j = 0; j < i; j++) {
                libusb_free_transfer(ctx->libusb_transfers[j]);
            }
            retn = 3;
            goto error_libusb_alloc_transfer;
        }
        libusb_fill_interrupt_transfer(
            ctx->libusb_transfers[i], handle, ctx->libusb_endpoint_addresses[i],
            ctx->buffer[i], T503_IO_BUFFER_SIZE,
            transfer_cb, (void *)ctx, 0
        );
    }


    libusb_free_device_list(devices, 1);
    return 0;

/* Warning: the label below should be in reversed order compared to the corresponding above */

error_libusb_alloc_transfer:
    libusb_release_interface(handle, T503_INTERFACE_2);
error_libusb_claim_interface_2:
    libusb_release_interface(handle, T503_INTERFACE_1);
error_libusb_claim_interface_1:
error_libusb_set_auto_detach_kernel_driver:
    libusb_close(handle);
error_libusb_open:
    libusb_free_config_descriptor(ctx->libusb_config);
error_libusb_get_config_descriptor:
error_device_not_found:
    libusb_free_device_list(devices, 1);
error_libusb_get_device_list:
    libusb_exit(ctx->libusb_ctx);
error_libusb_init:
    if (retn < 0) {
        errorf("T503 Error: %s\n", libusb_strerror(retn));
    } else {
        errorf("Some error happens, position = %d\n", retn);
    }

    assert(ctx->libusb_handle == NULL);
    return -1;
}

static int t503_init_libevdev(struct t503_context *ctx) {
    struct libevdev *dev = libevdev_new();
    if (!dev) goto error_libevdev_new;

    int retn;

    libevdev_set_name(dev, "test device");

    retn = libevdev_enable_event_type(dev, EV_ABS);
    if (retn == -1) goto error_libevdev_enable_events;
    struct input_absinfo absinfo_x = {
        0, 0, T503_MAX_X, 0, 0, T503_RESOLUTION_X
    };
    struct input_absinfo absinfo_y = {
        0, 0, T503_MAX_Y, 0, 0, T503_RESOLUTION_Y
    };
    struct input_absinfo absinfo_p = {
        0, 0, T503_MAX_PRESSURE, 0, 0, 0
    };
    retn = libevdev_enable_event_code(dev, EV_ABS, ABS_X, &absinfo_x);
    if (retn == -1) goto error_libevdev_enable_events;
    retn = libevdev_enable_event_code(dev, EV_ABS, ABS_Y, &absinfo_y);
    if (retn == -1) goto error_libevdev_enable_events;
    retn = libevdev_enable_event_code(dev, EV_ABS, ABS_PRESSURE, &absinfo_p);
    if (retn == -1) goto error_libevdev_enable_events;

    retn = libevdev_enable_event_type(dev, EV_KEY);
    if (retn == -1) goto error_libevdev_enable_events;
    /* BTN_STYLUS means this device is a pen */
    libevdev_enable_event_code(dev, EV_KEY, BTN_STYLUS, NULL);
    if (retn == -1) goto error_libevdev_enable_events;
#define T503_GEN_ENABLE_EVENT_CODE(key) \
    for (size_t i = 0; i < T503_COUNT_OF(g_mapping_ ## key); i++) { \
        retn = libevdev_enable_event_code(dev, EV_KEY, g_mapping_ ## key[i], NULL); \
        if (retn == -1) goto error_libevdev_enable_events; \
    }
    T503_GEN_ENABLE_EVENT_CODE(1);
    T503_GEN_ENABLE_EVENT_CODE(2);
    T503_GEN_ENABLE_EVENT_CODE(3);
    T503_GEN_ENABLE_EVENT_CODE(4);
    T503_GEN_ENABLE_EVENT_CODE(plus);
    T503_GEN_ENABLE_EVENT_CODE(minus);
#undef T503_GEN_ENABLE_EVENT_CODE


    struct libevdev_uinput *uidev;

    retn = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
    if (retn) goto error_libevdev_uinput_create_from_device;

    ctx->libevdev_dev = dev;
    ctx->libevdev_uidev = uidev;
    return 0;

error_libevdev_uinput_create_from_device:
error_libevdev_enable_events:
    libevdev_free(dev);
error_libevdev_new:
    errorf("Initialize evdev error!\n");
    return -1;
}

int t503_init(struct t503_context *ctx) {
    ctx->libusb_ctx = NULL;
    ctx->libusb_handle = NULL;
    ctx->libusb_config = NULL;
    ctx->libusb_endpoint_addresses[0] = 0;
    ctx->libusb_endpoint_addresses[1] = 0;
    ctx->libusb_transfers[0] = NULL;
    ctx->libusb_transfers[1] = NULL;
    ctx->libevdev_dev = NULL;
    ctx->libevdev_uidev = NULL;
    ctx->prev_mapping = NULL;
    ctx->prev_mapping_len = 0;

    int retn = t503_init_libusb(ctx);
    if (retn) goto error_t503_init_libusb;

    retn = t503_init_libevdev(ctx);
    if (retn) goto error_t503_init_libevdev;


    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        retn = 5;
        goto error_signal;
    } 

    return 0;

error_signal:
    t503_exit_libevdev(ctx);
error_t503_init_libevdev:
    t503_exit_libusb(ctx);
error_t503_init_libusb:
    errorf("T503 Initialization failed!\n");
    return -1;
}

static void t503_parse_transfer(struct libusb_transfer *transfer) {
    uint8_t *data = transfer->buffer;
    struct t503_context *ctx = (struct t503_context *)transfer->user_data;
    uint8_t button_minus = 0, button_plus = 0;
    uint8_t button_1 = 0, button_2 = 0, button_3 = 0, button_4 = 0;
    uint8_t pen_move = 0, pen_down = 0;
    uint16_t pos_x, pos_y, pressure = 0;

    switch (data[0]) {
    case 0x02:
        if (data[1] & 0x01) button_minus = 1;
        if (data[1] & 0x02) button_1 = 1;
        if (data[1] & 0x04) button_2 = 1;
        if (data[1] & ~0x07) debugf("Unknown data sequence 0x02[%02x]\n", data[1]);
        switch (data[3]) {
        case 0x00:
            break;
        case 0x2c:
            button_3 = 1;
            break;
        case 0x2b:
            button_4 = 1;
            break;
        case 0x1d:
            if (button_minus) break;
        default:
            debugf("Unknown data sequence 0x02????[%02x]\n", data[3]);
        }
        break;
    case 0x03:
        switch (data[1]) {
        case 0x00:
            break;
        case 0x02:
            button_plus = 1;
            break;
        default:
            debugf("Unknown data sequence 0x03[%02x]\n", data[1]);
        }
        break;
    case 0x05:
        if (data[1] & 0x01) pen_down = 1;
        if (data[1] & 0xc0) pen_move = 1;
        if (data[1] & ~0xc1) debugf("Unknown data sequence: 0x05[%02x]\n", data[1]);

        pos_x = T503_MAX_X - *(uint16_t *)&data[4];
        pos_y = *(uint16_t *)&data[2];
        pressure = *(uint16_t *)&data[6];
        break;
    default:
        debugf("Unknown data sequence: 0x[%02x]\nFull data sequence: ", data[0]);
        for (size_t i = 0; i < transfer->actual_length; i++) {
            debugf("%02x", data[i]);
        }
        debugf("\n");
    }

    if (button_1) debugf("btn1 ");
    if (button_2) debugf("btn2 ");
    if (button_3) debugf("btn3 ");
    if (button_4) debugf("btn4 ");
    if (button_plus) debugf("btn+ ");
    if (button_minus) debugf("btn- ");
    if (pen_move) debugf("penmov ");
    if (pen_down) debugf("pendown ");
    if (pressure) debugf("pressure=%u ", pressure);
    if (pen_move) debugf("posxy=%u,%u ", pos_x, pos_y);
    debugf("\n");

    if (pen_move) {
        libevdev_uinput_write_event(ctx->libevdev_uidev,
            EV_ABS, ABS_X, pos_x);
        libevdev_uinput_write_event(ctx->libevdev_uidev,
            EV_ABS, ABS_Y, pos_y);
        libevdev_uinput_write_event(ctx->libevdev_uidev,
            EV_ABS, ABS_PRESSURE, pressure);
    }

#define T503_GEN_UINPUT_WRITE_EVENT(key) \
    else if (button_ ## key) { \
        for (size_t i = 0; i < T503_COUNT_OF(g_mapping_ ## key); i++) { \
            libevdev_uinput_write_event(ctx->libevdev_uidev, \
                EV_KEY, g_mapping_ ## key[i], 1); \
        } \
        ctx->prev_mapping = g_mapping_ ## key; \
        ctx->prev_mapping_len = T503_COUNT_OF(g_mapping_ ## key); \
    }
    T503_GEN_UINPUT_WRITE_EVENT(1)
    T503_GEN_UINPUT_WRITE_EVENT(2)
    T503_GEN_UINPUT_WRITE_EVENT(3)
    T503_GEN_UINPUT_WRITE_EVENT(4)
    T503_GEN_UINPUT_WRITE_EVENT(plus)
    T503_GEN_UINPUT_WRITE_EVENT(minus)
#undef T503_GEN_UINPUT_WRITE_EVENT
    else {
        for (size_t i = 0; i < ctx->prev_mapping_len; i++) {
            libevdev_uinput_write_event(ctx->libevdev_uidev,
                EV_KEY, ctx->prev_mapping[i], 0);
        }
    }

    libevdev_uinput_write_event(ctx->libevdev_uidev,
        EV_SYN, SYN_REPORT, 0);
}

int t503_loop(struct t503_context *ctx) {
    for (size_t i = 0; i < T503_N_ENDPOINTS; i++) {
        libusb_submit_transfer(ctx->libusb_transfers[i]);
    }

    struct timeval tm;
    tm.tv_sec = 1;
    tm.tv_usec = 0;
    while (!s_should_exit) {
        libusb_handle_events_timeout_completed(ctx->libusb_ctx, &tm, NULL);
    }

    for (size_t i = 0; i < T503_N_ENDPOINTS; i++) {
        libusb_cancel_transfer(ctx->libusb_transfers[i]);
        libusb_handle_events_timeout_completed(ctx->libusb_ctx, &tm, NULL);
    }

    return 0;
}