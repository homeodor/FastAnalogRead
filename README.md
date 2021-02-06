# FastAnalogRead

## The Why

FastAnalogRead is a fork of a brilliant [ResponsiveAnalogRead](https://github.com/dxinteractive/ResponsiveAnalogRead) by dxinteractive. However, as great as the algorithm of the original is, the library has two major problems:

1. It uses floats as its numbers of choice, and
2. It still uses native Arduino approach for reading analog lines.

Floats are bad, unless you have a FPU-enabled microcontroller. They are, like, really slow. So for all of the AVR and SAMD21-based boards a better choice would be to eliminate them altogether. This library uses fixed point numbers and requires an extra library, [FixedPoints](https://github.com/Pharap/FixedPointsArduino).

Arduino's analog reading is really slow, too. It may take as long as several milliseconds to do the thing, and, counter-intuitively, it's even worse on newer ARM-based Arduinos. That's why this library uses the [approach discussed here](https://www.avdweb.nl/arduino/adc-dac/fast-10-bit-adc). It's off by default, but there are static methods that enable or disable it on AVRs and SAMD21s. When envoked, the method will affect native `analogRead()` as well, not only the FastAnalogRead.

So, how fast is it? I've benchmarked an Arduino Leonardo and an Arduino MKR Zero, by making 20000 readings on a single floating ADC pin:

|            | ResponsiveAnalogRead, stock ADC | ResponsiveAnalogRead, fast ADC | FastAnalogRead, stock ADC | FastAnalogRead, fast ADC |
| --- | --- | --- | --- | --- |
| Arduino Leonardo | 5451 ms | 3729 ms | 3210 ms | 1787 ms |
| Arduino MKR Zero | 18833 ms | 2425 ms | 17133 ms | 924 ms | 

While it's shocking to see how ARM-based Arduinos are actually four times slower when it comes to ADC reading, it is also really satisfying to know that with all the tweaks it runs 20× faster than it was ever possible. Also, while Leonardo still has troubles with fixed point 32 bit numbers, ARM is natively 32 bit, so this alone makes it 5× faster:

|            | ResponsiveAnalogRead calculations | FastAnalogRead calculations |
| --- | --- | --- |
| Arduino Leonardo | 3322 ms | 2001 ms |
| Arduino MKR Zero | 2219 ms | 496 ms |

What's the catch? Oh why, I'm glad you asked. Obviously, by replacing floats with fixes and by speeding up the ADC we're losing precision. The comparison of different ADC speeds [has already been made](https://www.avdweb.nl/arduino/adc-dac/fast-10-bit-adc), how about floats vs fixes? All values correspond to the ADC resolution, i.e. it's 6 out of 1024 for AVR and 83 out of 4096 for ARM.

|            | Average delta, stock ADC | Max delta, stock ADC | Average delta, fast ADC | Max delta, fast ADC |
| --- | --- | --- | --- | --- |
| Arduino Leonardo | 0 | 6  | 0 | 1 |
| Arduino MKR Zero | 0 | 83 | 0 | 13 | 

So, I believe this kind of precision tradeoff is more than acceptable, when it comes to a floating (i.e., random) pin! If you need extra precision, you would want to begin with reducing noise in your analog line in the first place...

## Pros and cons

Pros:
1. It's up to 3 times faster on AVR-based Arduinos, and up to 20 times on SAMD21-based
2. It has no significant precision penalty
3. The API is identical to ResponsiveAnalogRead: all you need to do is change the header and the class name

Cons:
1. It uses an extra library, [FixedPoints](https://github.com/Pharap/FixedPointsArduino)
2. ADC tweaks are only for SAMD21 and AVR boards at this moment.
3. Probably no advantage over ResponsiveAnalogRead for MCUs with FPUs, i.e. Cortex-M4F or ESP32

## Fair warning

As of now, the library runs fine in a production code of an actual MIDI controller, the [MIDI Dobrynya](https://www.mididobrynya.com/). With that said, it has not been extensively tested and may contain bugs and possibly even traces of nuts.

## The original description of the library

![FastAnalogRead](https://user-images.githubusercontent.com/345320/50956817-c4631a80-1510-11e9-806a-27583707ca91.jpg)

FastAnalogRead is an Arduino library for eliminating noise in analogRead inputs without decreasing responsiveness. It sets out to achieve the following:

1. Be able to reduce large amounts of noise when reading a signal. So if a voltage is unchanging aside from noise, the values returned should never change due to noise alone.
2. Be extremely responsive (i.e. not sluggish) when the voltage changes quickly.
3. Have the option to be responsive when a voltage *stops* changing - when enabled the values returned must stop changing almost immediately after. When this option is enabled, a very small sacrifice in accuracy is permitted.
4. The returned values must avoid 'jumping' up several numbers at once, especially when the input signal changes very slowly. It's better to transition smoothly as long as that smooth transition is short.

You can preview the way the algorithm works with [sleep enabled](http://codepen.io/dxinteractive/pen/zBEbpP) (minimising the time spend transitioning between values) and with [sleep disabled](http://codepen.io/dxinteractive/pen/ezdJxL) (transitioning responsively and accurately but smooth).

An article discussing the design of the algorithm can be found [here](http://damienclarke.me/code/posts/writing-a-better-noise-reducing-analogread).

## How to use

Here's a basic example:

```Arduino
// include the FastAnalogRead library
#include <FastAnalogRead.h>

// define the pin you want to use
const int ANALOG_PIN = A0;

// make a FastAnalogRead object
FastAnalogRead analog;

// the next optional argument is snapMultiplier, which is set to 0.01 by default
// you can pass it a value from 0 to 1 that controls the amount of easing
// increase this to lessen the amount of easing (such as 0.1) and make the responsive values more responsive
// but doing so may cause more noise to seep through if sleep is not enabled

void setup() {
  // begin serial so we can see analog read values through the serial monitor
  Serial.begin(9600);
  // call a begin function, pass in the pin, and either true or false depending on if you want sleep enabled
  // enabling sleep will cause values to take less time to stop changing and potentially stop changing more abruptly,
  // where as disabling sleep will cause values to ease into their correct position smoothly and with slightly greater accuracy
  analog.begin(ANALOG_PIN, true);
  // enable faster ADC mode globally
  FastAnalogRead::enableFastADC();
}

void loop() {
  // update the FastAnalogRead object every loop
  analog.update();

  Serial.print(analog.getRawValue());
  Serial.print("\t");
  Serial.print(analog.getValue());
  
  // if the responsive value has change, print out 'changed'
  if(analog.hasChanged()) {
    Serial.print("\tchanged");
  }
  
  Serial.println("");
  delay(20);
}
```

### Using your own ADC

```Arduino
#include <FastAnalogRead.h>

FastAnalogRead analog;

void setup() {
  // begin serial so we can see analog read values through the serial monitor
  Serial.begin(9600);
  // initialize
  analog.begin(0, true);
  // obviously FastAnalogRead::enableFastADC() will do nothing with external ADC, so it's not here
}

void loop() {
  // read from your ADC
  // update the FastAnalogRead object every loop
  int reading = YourADCReadMethod();
  analog.update(reading);
  Serial.print(analog.getValue());
  
  Serial.println("");
  delay(20);
}
```

### Smoothing multiple inputs

```Arduino
#include <FastAnalogRead.h>

FastAnalogRead analogOne(A1, true);
FastAnalogRead analogTwo(A2, true);

void setup() {
  // begin serial so we can see analog read values through the serial monitor
  Serial.begin(9600);
  // initialize both objects
  analogOne.begin(A1, true);
  analogTwo.begin(A2, true);
  //  enable faster ADC mode globally
  FastAnalogRead::enableFastADC();
}

void loop() {
  // update the FastAnalogRead objects every loop
  analogOne.update();
  analogTwo.update();
  
  Serial.print(analogOne.getValue());
  Serial.print(analogTwo.getValue());
  
  Serial.println("");
  delay(20);
}
```

## How to install

In the Arduino IDE, go to Sketch > Include libraries > Manage libraries, and search for FastAnalogRead.
You can also just use the files directly from the src folder.

You also need [FixedPoints](https://github.com/Pharap/FixedPointsArduino) library, which can be done in exactly the same way.

Look at the example in the examples folder for an idea on how to use it in your own projects.
The source files are also heavily commented, so check those out if you want fine control of the library's behaviour.

## Constructor arguments

- `pin` - int, the pin to read (e.g. A0).
- `sleepEnable` - boolean, sets whether sleep is enabled. Defaults to true. Enabling sleep will cause values to take less time to stop changing and potentially stop changing more abruptly, where as disabling sleep will cause values to ease into their correct position smoothly.
- `snapMultiplier` - float, a value from 0 to 1 that controls the amount of easing. Defaults to 0.01. Increase this to lessen the amount of easing (such as 0.1) and make the responsive values more responsive, but doing so may cause more noise to seep through if sleep is not enabled.

## Basic methods

- `int getValue() // get the responsive value from last update`
- `int getRawValue() // get the raw analogRead() value from last update`
- `bool hasChanged() // returns true if the responsive value has changed during the last update`
- `void update(); // updates the value by performing an analogRead() and calculating a responsive value based off it`
- `void update(int rawValue); // updates the value by accepting a raw value and calculating a responsive value based off it (version 1.1.0+)`
- `bool isSleeping() // returns true if the algorithm is in sleep mode (version 1.1.0+)`

## Other methods (settings)

### Faster ADC (static)

Works for SAMD21 (ARM) and AVR-based boards. Changes ADC speed globally, for all instances of FastAnalogRead or any other analog-reading functions.

- `void enableFastADC()` 
- `void disableFastADC()` 

### Sleep

- `void enableSleep()`
- `void disableSleep()`

Sleep allows you to minimise the amount of responsive value changes over time. Increasingly small changes in the output value to be ignored, so instead of having the responsiveValue slide into position over a couple of seconds, it stops when it's "close enough". It's enabled by default. Here's a summary of how it works:

1. "Sleep" is when the output value decides to ignore increasingly small changes.
2. When it sleeps, it is less likely to start moving again, but a large enough nudge will wake it up and begin responding as normal.
3. It classifies changes in the input voltage as being "active" or not. A lack of activity tells it to sleep.

### Activity threshold
- `void setActivityThreshold(float newThreshold) // the amount of movement that must take place for it to register as activity and start moving the output value. Defaults to 4.0. (version 1.1+)`

### Snap multiplier
- `void setSnapMultiplier(float newMultiplier)`

SnapMultiplier is a value from 0 to 1 that controls the amount of easing. Increase this to lessen the amount of easing (such as 0.1) and make the responsive values more responsive, but doing so may cause more noise to seep through when sleep is not enabled.

### Edge snapping
- `void enableEdgeSnap() // edge snap ensures that values at the edges of the spectrum (0 and 1023) can be easily reached when sleep is enabled`

### Analog resolution
- `void setAnalogResolution(int resolution)`

If your ADC is something other than 10bit (1024), set that using this.

## License

Licensed under the MIT License (MIT)

Copyright (c) 2021, Alexander Golovanov
Original code is copyright (c) 2016, Damien Clarke

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
