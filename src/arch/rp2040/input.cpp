#include "arch/input.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "pio_usb.h"
#include <stdio.h>


static usb_device_t *usb_device = NULL;

void core1_main() {
    sleep_ms(10);

    // To run USB SOF interrupt in core1, create alarm pool in core1.
    static pio_usb_configuration_t config = PIO_USB_DEFAULT_CONFIG;
    config.alarm_pool = (void*)alarm_pool_create(1, 1);
    usb_device = pio_usb_host_init(&config);

    //// Call pio_usb_host_add_port to use multi port
    // const uint8_t pin_dp2 = 8;
    // pio_usb_host_add_port(pin_dp2);

    while (true) {
        pio_usb_host_task();
    }
}


void input_init()
{
    set_sys_clock_khz(120000, true);

    multicore_reset_core1();
    // all USB task run in core1
    multicore_launch_core1(core1_main);
}

static char hold_key = 0;

static char key_lookup[256] = {
// 0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xA  xB  xC  xD  xE  xF
   0,  0,  0,  0,'a','b','c','d','e','f','g','h','i','j','k','l', // 0x
 'm','n','o','p','q','r','s','t','u','v','w','x','y','z','1','2', // 1x
 '3','4','5','6','7','8','9','0',  0,  0,  0,  0,' ','-','=','[', // 2x
']','\\', 0,';','\'','`',',','.','/',  0,  0,  0,  0,  0,  0,  0, // 3x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 4x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 5x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 6x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 7x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 8x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 9x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Ax
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Bx
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Cx
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Dx
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Ex
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Fx
};
static char key_lookup_shift[256] = {
   0,  0,  0,  0,'A','B','C','D','E','F','G','H','I','J','K','L', // 0x
 'M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','!','@', // 1x
 '#','$','%','^','&','*','(',')',  0,  0,  0,  0,' ','_','+','{', // 2x
 '}','|',  0,':','"','~','<','>','?',  0,  0,  0,  0,  0,  0,  0, // 3x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 4x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 5x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 6x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 7x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 8x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 9x
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Ax
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Bx
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Cx
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Dx
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Ex
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Fx
};

static bool update_key(uint8_t mod, uint8_t keycode)
{
    auto lookup = key_lookup;
    if (mod & 0x22) lookup = key_lookup_shift;
    if (!lookup[keycode]) {
        printf("Unknown scancode: %02X:%02X\n", mod, keycode);
        return false;
    }
    hold_key = lookup[keycode];
    return true;
}

static void update()
{
    if (usb_device == NULL)
        return;
    for (int dev_idx = 0; dev_idx < PIO_USB_DEVICE_CNT; dev_idx++) {
        usb_device_t *device = &usb_device[dev_idx];
        if (!device->connected) {
            continue;
        }

        // Print received packet to EPs
        for (int ep_idx = 0; ep_idx < PIO_USB_DEV_EP_CNT; ep_idx++) {
            endpoint_t *ep = pio_usb_get_endpoint(device, ep_idx);

            if (ep == NULL) {
                break;
            }

            uint8_t temp[64];
            int len = pio_usb_get_in_data(ep, temp, sizeof(temp));

            if (len > 0) {
                //printf("%04x:%04x EP 0x%02x:\t", device->vid, device->pid, ep->ep_num);
                //for (int i = 0; i < len; i++) printf("%02x ", temp[i]);
                //printf("\n");
                if (ep->ep_num == 0x81) {
                    bool did_update = false;
                    for(int idx=2; idx<len; idx++) {
                        if (temp[idx] != 0) if (update_key(temp[0], temp[idx])) did_update = true;
                    }
                    if (!did_update) hold_key = 0;
                }
            }
        }
    }
}

char input_getchar()
{
    auto old = hold_key;
    update();
    sleep_us(10);
    if (old != hold_key)
        return hold_key;
    return 0;
}
