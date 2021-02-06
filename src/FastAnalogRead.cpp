  /*
  * FastAnalogRead.cpp
  * Arduino library for eliminating noise in analogRead inputs without decreasing responsiveness
  *
  * Copyright (c) 2016 Damien Clarke
  *
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to deal
  * in the Software without restriction, including without limitation the rights
  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  * copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all
  * copies or substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  * SOFTWARE.
  */

  #include <Arduino.h>
  #include "FastAnalogRead.h"

  const FastAnalogFixed FastAnalogRead::const_c0 = 0;
  const FastAnalogFixed FastAnalogRead::const_c1 = 1;
  const FastAnalogFixed FastAnalogRead::const_c2 = 2;

  void FastAnalogRead::begin(int pin, bool sleepEnable, FastAnalogFixed snapMultiplier){
      pinMode(pin, INPUT ); // ensure button pin is an input
      digitalWrite(pin, LOW ); // ensure pullup is off on button pin
      
      this->pin = pin;
      this->sleepEnable = sleepEnable;
      setSnapMultiplier(snapMultiplier);  
  }


  void FastAnalogRead::update()
  {
    rawValue = analogRead(pin);
    this->update(rawValue);
  }

  void FastAnalogRead::update(uint16_t rawValueRead)
  {
    rawValue = rawValueRead;
    prevResponsiveValue = responsiveValue;
    responsiveValue = getResponsiveValue(rawValue);
    responsiveValueHasChanged = responsiveValue != prevResponsiveValue;
  }

  int FastAnalogRead::getResponsiveValue(FastAnalogFixed newValue)
  {
    static const FastAnalogFixed errorMargin = 0.4;

    // if sleep and edge snap are enabled and the new value is very close to an edge, drag it a little closer to the edges
    // This'll make it easier to pull the output values right to the extremes without sleeping,
    // and it'll make movements right near the edge appear larger, making it easier to wake up
    if (sleepEnable && edgeSnapEnable)
    {
      if(newValue < activityThreshold)
        newValue = newValue - activityThreshold + newValue;
      else if (newValue > analogResolution - activityThreshold)
        newValue = newValue + activityThreshold - analogResolution + newValue;//(newValue * 2) - analogResolution + activityThreshold;
    }

    // get difference between new input value and current smooth value


  //  if (newVaue > )

    FastAnalogFixed diff;

    if (newValue > smoothValue)
      diff = (newValue - smoothValue).getInteger();
    else
      diff = (smoothValue - newValue).getInteger();

  //  unsigned int diff = abs(newValue - smoothValue);

    // measure the difference between the new value and current value
    // and use another exponential moving average to work out what
    // the current margin of error is
    errorEMA += ((newValue - smoothValue) - errorEMA) * errorMargin;// 0.4;

    // if sleep has been enabled, sleep when the amount of error is below the activity threshold
    if(sleepEnable) {
      // recalculate sleeping status
      sleeping = abs(errorEMA) < activityThreshold;
    }

    // if we're allowed to sleep, and we're sleeping
    // then don't update responsiveValue this loop
    // just output the existing responsiveValue
    if(sleepEnable && sleeping) {
      return smoothValue.getInteger();
    }

    // use a 'snap curve' function, where we pass in the diff (x) and get back a number from 0-1.
    // We want small values of x to result in an output close to zero, so when the smooth value is close to the input value
    // it'll smooth out noise aggressively by responding slowly to sudden changes.
    // We want a small increase in x to result in a much higher output value, so medium and large movements are snappy and responsive,
    // and aren't made sluggish by unnecessarily filtering out noise. A hyperbola (f(x) = 1/x) curve is used.
    // First x has an offset of 1 applied, so x = 0 now results in a value of 1 from the hyperbola function.
    // High values of x tend toward 0, but we want an output that begins at 0 and tends toward 1, so 1-y flips this up the right way.
    // Finally the result is multiplied by 2 and capped at a maximum of one, which means that at a certain point all larger movements are maximally snappy

    // then multiply the input by SNAP_MULTIPLER so input values fit the snap curve better.
    FastAnalogFixed snap = snapCurve(diff * snapMultiplier);

    // when sleep is enabled, the emphasis is stopping on a responsiveValue quickly, and it's less about easing into position.
    // If sleep is enabled, add a small amount to snap so it'll tend to snap into a more accurate position before sleeping starts.
    //if(sleepEnable) 
    //{
    //  snap *= const_c1; // ??????????
    //}

    // calculate the exponential moving average based on the snap
    smoothValue += (newValue - smoothValue) * snap;

    // ensure output is in bounds
    if(smoothValue < const_c0)
      smoothValue = 0;
    else if(smoothValue > (analogResolution - const_c1))
      smoothValue = analogResolution - const_c1;

    // expected output is an integer
    return smoothValue.getInteger();
  }

  FastAnalogFixed FastAnalogRead::snapCurve(FastAnalogFixed x)
  {
    FastAnalogFixed y = const_c1 / (x + const_c1);
    y = (const_c1 - y) * const_c2;
    return (y > 1.0) ? const_c1 : y;
  }

  void FastAnalogRead::setSnapMultiplier(FastAnalogFixed newMultiplier)
  {
    if(newMultiplier > const_c1)
    {
      snapMultiplier = const_c1; 
      return;
    }

    if(newMultiplier < const_c0)
    {
      snapMultiplier = const_c0;
      return;
    }

    snapMultiplier = newMultiplier;
  }

  void FastAnalogRead::enableFastADC(bool enable)
  {
    static bool enabled = false;

    if ((enable && enabled) || (!enable && !enabled)) return;

  #ifdef __AVR__
    static uint8_t prevState;
    if (enable)
    {
      prevState = ADCSRA & 0b111;
      ADCSRA = (ADCSRA & 0b11111000) | 0b100; // Wooooo
      enabled = true;
    } else {
      ADCSRA = (ADCSRA & 0b11111000) | prevState;
      enabled = false;
    }
  #endif
  #ifdef __SAMD21__

    static uint32_t prevCTRLB, prevAVGCTRL, prevSAMPCTRL;

    if (enable)
    {
      ADC->CTRLA.bit.ENABLE = 0;          // ADC off
      while (ADC->STATUS.bit.SYNCBUSY);   // wait...

      prevCTRLB    = ADC->CTRLB.reg & 11100000000;
      prevAVGCTRL  = ADC->AVGCTRL.reg;
      prevSAMPCTRL = ADC->SAMPCTRL.reg;
      
      ADC->CTRLB.reg = (ADC->CTRLB.reg & 0b1111100011111111) | ADC_CTRLB_PRESCALER_DIV64;
          // clear prescaler bits, then set them as needed
      ADC->AVGCTRL.reg  = ADC_AVGCTRL_SAMPLENUM_1 | ADC_AVGCTRL_ADJRES(0x0);   // 1 sample, adjust result by 0
      ADC->SAMPCTRL.reg = 0x0;                       // sampling Time Length = 0

      ADC->CTRLA.bit.ENABLE = 1;          // ADC on
      while (ADC->STATUS.bit.SYNCBUSY);   // wait

      enabled = true;
    } else {
      ADC->CTRLA.bit.ENABLE = 0;          // ADC off
      while (ADC->STATUS.bit.SYNCBUSY);   // wait...
      
      ADC->CTRLB.reg = (ADC->CTRLB.reg & 0b1111100011111111) | prevCTRLB;
      ADC->AVGCTRL.reg  = prevAVGCTRL; 
      ADC->SAMPCTRL.reg = prevSAMPCTRL; 

      ADC->CTRLA.bit.ENABLE = 1;          // ADC on
      while (ADC->STATUS.bit.SYNCBUSY);   // wait

      enabled = false;
    }
  #endif
  }
