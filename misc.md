## Will the device work without the HD "IDM" module attached?
Not really. The stock controller gets unhappy if it can't reach the IDM module via i2c. It will NOT boot and instead sit on the flashing "WAIT" screen if it's unreachable.

If momentarily reset while the radio is on, it will reset to it's uninitialized state but still responds to i2c commands, so it will be disabled until the radio is powered off and on again. Until then, the radio is analog only. However, if the IDM module is disconnected for more than a few seconds, the controller freaks out and resets everything to try initialization again. This will reset the DSP and knock out audio.

## What're "normal" voltages on the DSP power pins?
The following were recorded when tuned to a strong local analog FM station with the radio running for ~10 minutes:

| Pin    | Volts |
|--------|-------|
| Pin 5  | 8.15v |
| Pin 6  | 3.29v |
| Pin 9  | 3.29v |
| Pin 10 | 3.29v |
| Pin 11 | 1.78v |

## Some notes about I2S
There are two I2S lines in the radio, both between the IDM module and DSP. It appears that the DSP is the master of both of them. Here is a table of details for them:

| Name        | Purpose          | DSP Pins | IDM Pins | WS       | BCLK       | Bits/Sample |
|-------------|------------------|----------|----------|----------|------------|-------------|
| HD Audio    | Decoded HD audio | 20-22    | 15-17    | 44.1 kHz | 2.8224 MHz | 32, stereo  |
| Baseband IQ | Baseband IQ      | 16-19    | 8-11     | ~325 kHz | ~10.4 MHz  | 16, mono    |

### HD Audio
The HD audio line carries the decoded HD audio for the current subchannel, if decoding. While the DSP serves as the master generating the clock, the IDM is what actually delivers data.

**When audio is available on HD, the "blend" pin is pulled high to 3.3v** to signal that digital audio should be output. If HD is unavailable, that pin is pulled low to signal that analog audio should be output instead. This is a digital pin only; the DSP is responsible for fading between analog and digital. This can be easily confirmed by tuning to an HD2 subchannel and pulling that pin to ground.

Additionally, the service manual labels an unused pin on the DSP called "I2S_OUT". I haven't tested it, but my theory is that this can be pulled high or low and instead of accepting the HD audio as usual, it will reverse the direction and output the demodulated analog audio for an external DAC or something. I haven't confirmed this though!

### Baseband IQ
The baseband IQ line is arguably a lot more interesting. It outputs the digitized I and Q channels to form a complex signal that can be used for decoding HD. This same kind of signal could also be written to disk to serve as a digitized copy of the RF received. The purpose of this is to send the baseband RF to the IDM module to lock and capture HD.

The DSP serves both as the master generating the clock and the source of the data. The I2S method for this is somewhat unusual. Instead of being a single serial line carrying left and right channels, this is instead a parallel line. The same WS and BCLK lines are shared to deliver two channels across two data lines, each of which handle their own channel. The word select line does nothing but signal the start and stop bits for samples; the value is meaningless.

## What're the bare minimum pins needed for the stock controller?

At a minimum, you need to provide five pins to the controller:

| Name       | Pin      | Level  |
|------------|----------|--------|
| LED+B      | **5P**/1 | 5v     |
| MICON+B    | **5P**/2 | 3.3v   |
| GND        | **5P**/4 | Ground |
| DTUNER_SDA | **6P**/1 | N/A    |
| DTUNER_SCL | **6P**/2 | N/A    |

However, **the tuner will not boot** until you connect DTUNER_POWER (5P/3) to +3.3v. This pin is what signals power to be supplied to the DSP and IDM, so they'll have no power til you bring that to 3.3v.

Additionally, you won't have any RDS unless you connect the DTUNER_RDS (6P/3) pin. The RDS line appears to just be a clock that signals to the controller to read an i2c register from the DSP (so it doesn't actually carry RDS).

I haven't found any consequences of disconnecting the other pins (except that the reset button on the back of the radio won't reset the controller) but I'm sure there's something I'm missing.

*By the way, note that the pads on the controller are especially fragile and are also very easy to accidentally short to each other or ground. I've learned this the hard way by destroying many of the pads on both of my controllers. Luckily there are test points nearby that can be used as a replacement, but still not ideal...*
