# Sony XDR-F1HD Remote (RMT-CF1A)

The elusive Sony XDR-F1HD IR remote is a part that doesn't seem to come up on eBay very often. I don't have one either, but I wanted to automate running some tests on my unit that involved powering it up and down again, among other things. [Kkonrad](https://github.com/kkonradpl/xdr-i2c) wrote some code to send the "power" command for the Arduino, but I wanted more.

After a lot of digging, I came across [all of the remote codes that Ted Timmons contributed](http://lirc.sourceforge.net/remotes/sony/RMT-CF1A) some time in 2008 to the Linux remote control driver codebase. Combining these with the previous code, I'm now able to send all of the IR commands with an ESP32.

## Code Table

Below is the code table from [here](http://lirc.sourceforge.net/remotes/sony/RMT-CF1A):

| **Button** | **Code** |
|------------|----------|
| sleep      | 0x06B    |
| power      | 0xA8B    |
| key\-1     | 0x00B    |
| key\-2     | 0x80B    |
| key\-3     | 0x40B    |
| key\-4     | 0xC0B    |
| key\-5     | 0x20B    |
| key\-6     | 0xA0B    |
| key\-7     | 0x60B    |
| key\-8     | 0xE0B    |
| key\-9     | 0x10B    |
| key\-0     | 0x90B    |
| enter      | 0x0CB    |
| preset\+   | 0xBCB    |
| preset\-   | 0x7CB    |
| hd\-scan   | 0x0EB    |
| scan       | 0x8EB    |
| tune\+     | 0xCEB    |
| tune\-     | 0x2EB    |
| band       | 0xF6B    |
| menu       | 0x12B    |
| bright     | 0xD2B    |
| display    | 0x92B    |

To use codes here in the below code, you'll need to **bit shift it up 8 bits and OR 0xC8**. For example, to use power, do this:

```
uint32_t code = (0xA8B << 8) | 0xC8;
```

## IR ESP-32 Code

To use this code, attach an IR LED through a resistor to pin 26 (or change ``PIN_IR``):

```c
#include "driver/gpio.h"
#include "rom/ets_sys.h"

#define PIN_IR 26

// based on https://github.com/kkonradpl/xdr-i2c
static void ir_carrier(uint16_t time)
{
    uint8_t i;
    uint8_t c = time / 25;
    for(i=0; i<c; i++) /* approx. 40kHz */
    {
        gpio_set_level(PIN_IR, 1);
        ets_delay_us(13);
        gpio_set_level(PIN_IR, 0);
        ets_delay_us(13);
    }
}

// based on https://github.com/kkonradpl/xdr-i2c
static void ir_sendcode(uint32_t code)
{
    ir_carrier(2400);
    ets_delay_us(600);
    for(int i=19; i>=0; i--)
    {
        ir_carrier(((code >> i) & 1) ? 1200 : 600);
        ets_delay_us(600);
    }
}
```

Then, call ``ir_sendcode``. You'll also likely need to configure the GPIO the first time as follows:

```c
gpio_set_direction(PIN_IR, GPIO_MODE_OUTPUT);
gpio_set_pull_mode(PIN_IR, GPIO_PULLUP_PULLDOWN);
```