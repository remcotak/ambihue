// Include the necessary library's
#include <Adafruit_NeoPixel.h>
#include "FastLED.h"

// Ledstrip variables
#define LED_PIN 6
#define NUM_LEDS 60
#define SPEED 115200

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (e.g. High Density LED strip)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

CRGB leds[NUM_LEDS];
uint8_t * ledsRaw = (uint8_t *)leds;

// A 'magic word' (along with LED count & checksum) precedes each block
// of LED data; this assists the microcontroller in syncing up with the
// host-side software and properly issuing the latch (host I/O is
// likely buffered, making usleep() unreliable for latch).  You may see
// an initial glitchy frame or two until the two come into alignment.
// The magic word can be whatever sequence you like, but each character
// should be unique, and frequent pixel values like 0 and 255 are
// avoided -- fewer false positives.  The host software will need to
// generate a compatible header: immediately following the magic word
// are three bytes: a 16-bit count of the number of LEDs (high byte
// first) followed by a simple checksum value (high byte XOR low byte
// XOR 0x55).  LED data follows, 3 bytes per LED, in order R, G, B,
// where 0 = off and 255 = max brightness.
static const uint8_t magic[] = {
  'A','d','a'};
#define MAGICSIZE  sizeof(magic)
#define HEADERSIZE (MAGICSIZE + 3)

#define MODE_HEADER 0
#define MODE_DATA   2

// If no serial data is received for a while, the LEDs are shut off
// automatically.  This avoids the annoying "stuck pixel" look when
// quitting LED display programs on the host computer.
static const unsigned long serialTimeout = 5000; // 5 seconds

// Variables for the buttons
const int buttonUp = 2;       // the number of the pushbutton pin
const int buttonDown = 3;     // the number of the pushbutton pin
int buttonStateUp = 0;        // variable for reading the pushbutton status
int lastButtonStateUp = 0;    // previous state of the button
int buttonStateDown = 0;      // variable for reading the pushbutton status
int lastButtonStateDown = 0;  // previous state of the button
int state = 0;                // the average

// Variables for the rotary encoder
const int pinA = 12;          // the number of the rotary pin a
const int pinB = 11;          // the number of the rotary pin b
int reading = 0;              // variable for reading the rotary encoder
int changeamnt = 10;          // the change amount for the reading
boolean encA;
boolean encB;
boolean lastA = false;

// Variables for the slider
int sliderPin = A0;           // select the input pin for the slider
int sliderValue = 0;          // variable to store the value coming from the sensor
int ledBrightness = 0;        // keep track of the brightness
int maxBrightness = 75;       // max brightness allowed, from 0 to 255

// Timers
long currentTime = 0;         // keep track of the current time
long lastRotaryTime = 0;
long lastRainbowTime = 0;
long lastWalkTime = 0;

// Color function variables
int lowestReading = 0;        // lowest rotary reading allowed
int highestReading = 1023;    // highest reading allowed
// Rainbow function variables
int rainbowColor = 0;         // current color of the rainbow (0-255)
// Rainbow walk function variables
int walkColor = 0;            // current color of the rainbow walk (0-255)

void setup() {
  // Rotary pinmodes, set the two pins as inputs with internal pullups
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  // Button pinmodes
  pinMode(buttonUp, INPUT);
  pinMode(buttonDown, INPUT);
  // set slider value
  sliderValue = analogRead(sliderPin);
  // map brightness according to the slider
  ledBrightness = map(sliderValue, 0, 1023, 0, 75);
  // Begin transferring data to the ledstrip
  strip.begin();
  ledBrightness = map(sliderValue, 0, 1023, 0, 75);
  strip.setBrightness(ledBrightness);
  strip.show(); // Initialize all pixels to 'off'
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  // begin serial for ambilight readings
  Serial.begin(SPEED);
}

void loop() {
  // Read elapsed time
  currentTime = millis();
  // Keep track of the button presses
  buttons();
  // Keep track of the rotary encoder changes
  rotary();
  // Keep track of the slider for the brightness
  brightness();

  // Switch modes depending on the current state
  switch (state) {
    case 0:
      stripOff();
      break;
    case 1:
      color();
      break;
    case 2:
      rainbow();
      break;
    case 3:
      rainbowWalking();
      break;
    case 4:
      ambilight();
      break;
  }
}

void buttons(){
  // read the state of the pushbutton value:
  buttonStateUp = digitalRead(buttonUp);
  buttonStateDown = digitalRead(buttonDown);

  // only read button changes
  if (buttonStateUp != lastButtonStateUp) {
    if (buttonStateUp == HIGH && state <4 ){
      state += 1;
      // Reset rotary variables
      reading = 0;
      changeamnt = 10;
      lastButtonStateUp = HIGH;
    } else {
      lastButtonStateUp = LOW;
    }
  }
  // only read button changes
  if (buttonStateDown != lastButtonStateDown) {
    if (buttonStateDown == HIGH && state >0 ){
      state -= 1;
      // Reset rotary variables
      reading = 0;
      changeamnt = 10;
      lastButtonStateDown = HIGH;
    } else {
      lastButtonStateDown = LOW;
    }
  }
}

void rotary(){
  // Check if it's time to read
  if (currentTime >= (lastRotaryTime + 5)) {
    // read the two pins
    encA = digitalRead(pinA);
    encB = digitalRead(pinB);
    // check if A has gone from high to low
    if ((!encA) && (lastA)) {
      // check if B is high 
      if (encB) {
        // Clockwise rotation
        // when state is 1, the rotary value should loop
        if (state == 1) {
            if (reading + changeamnt <= highestReading) {
              reading = reading + changeamnt;
            } else if (reading + changeamnt > highestReading) {
              reading = lowestReading;
            }
        } else {
          reading = reading + changeamnt; 
        }
      } else {
        // Counter clockwise rotation
        // when state is 1, the rotary value should loop
        if (state == 1) {
          if (reading - changeamnt >= lowestReading) {
            reading = reading - changeamnt; 
          } else if (reading - changeamnt < lowestReading) {
            reading = highestReading;
          }
        } else {
          // prevent reading from going negative
          if (reading - changeamnt > 0) {
            reading = reading - changeamnt;
          } else if (reading - changeamnt <= 0) {
            reading = 0;
          }
        }
      }
    }
    // store reading of A and millis for next loop
    lastA = encA;
    lastRotaryTime = currentTime;
  }
}

void brightness() {
  // read the value from the sensor
  sliderValue = analogRead(sliderPin);
  // map values from 0-123 to 0-25, which the leds can read
  ledBrightness = map(sliderValue, 0, 1023, 0, maxBrightness);
}

void stripOff(){
  // turn of the led strip
  for (int i=0; i< strip.numPixels(); i++) {
    strip.setPixelColor(i, 0,0,0);
  }
  strip.show();
}

void color(){
  // this function changes the color based on the rotary value
  changeamnt = 5;
  int val = reading;
  
  int redVal = 0; //red value
  int grnVal = 0; //green value
  int bluVal = 0; //blue value

  if ( reading < 341) {
    // Lowest third of the potentiometer's range (0-340)
    val = (reading * 3) / 4; // Normalize to 0-255

    if (val == 0) {
      redVal = 255;         // Red from full to off
      grnVal = 0;           // Green from off to full
      bluVal = 0;           // Blue off
    } else {
      redVal = 256 - val;   // Red from full to off
      grnVal = val;         // Green from off to full
      bluVal = 0;           // Blue off
    }
  } else if (reading < 682) {
    // Middle third of potentiometer's range (341-681)
    val = ( (reading-341) * 3) / 4; // Normalize to 0-255

    redVal = 0;             // Red off
    grnVal = 256 - val;     // Green from full to off
    bluVal = val;           // Blue from off to full
  } else {
    // Upper third of potentiometer"s range (682-1023)
    val = ( (reading-683) * 3) / 4; // Normalize to 0-255

    redVal = val;           // Red from off to full
    grnVal = 0;             // Green off
    bluVal = 256 - val;     // Blue from full to off
  }

  for (int i=0; i< strip.numPixels(); i++) {
    strip.setPixelColor(i, redVal,grnVal,bluVal);
  }
  strip.setBrightness(ledBrightness);
  strip.show();
}

void rainbow(){
  // loop through the color ranges
  easeReading();
  // delay the function loop on the reading
  if (currentTime - lastRainbowTime > reading) {
    if (rainbowColor >= 255){
      rainbowColor = 0;
      for (int i=0; i< strip.numPixels(); i++) {
        strip.setPixelColor(i, Wheel(((i/strip.numPixels())+ rainbowColor)& 255));
      }
    } else {
      rainbowColor +=1;
      for (int i=0; i< strip.numPixels(); i++) {
        strip.setPixelColor(i, Wheel(((i/strip.numPixels())+ rainbowColor)& 255));
      }
    }
    strip.setBrightness(ledBrightness);
    strip.show();
    lastRainbowTime = currentTime;
  }
}

void rainbowWalking(){
  // let the colors walk over the led strip
  easeReading();
  // delay the function loop on the reading
  if (currentTime - lastWalkTime > reading) {
    if (walkColor >= 255){
      walkColor = 0;
      for (int i=0; i< strip.numPixels(); i++) {
        strip.setPixelColor(i, Wheel(((i*256/strip.numPixels())+ walkColor)& 255));
      }
    } else {
      walkColor += 1;
      for (int i=0; i< strip.numPixels(); i++) {
        strip.setPixelColor(i, Wheel(((i*256/strip.numPixels())+ walkColor)& 255));
      }
    }
    strip.setBrightness(ledBrightness);
    strip.show();
    lastWalkTime = currentTime;
  }
}

void ambilight(){
  // Dirty trick: the circular buffer for serial data is 256 bytes,
  // and the "in" and "out" indices are unsigned 8-bit types -- this
  // much simplifies the cases where in/out need to "wrap around" the
  // beginning/end of the buffer.  Otherwise there'd be a ton of bit-
  // masking and/or conditional code every time one of these indices
  // needs to change, slowing things down tremendously.
  uint8_t
    buffer[256],
  indexIn       = 0,
  indexOut      = 0,
  mode          = MODE_HEADER,
  hi, lo, chk, i, spiFlag;
  int16_t
    bytesBuffered = 0,
  hold          = 0,
  c;
  int32_t
    bytesRemaining;
  unsigned long
    startTime,
  lastByteTime,
  lastAckTime,
  t;
  int32_t outPos = 0;

  Serial.print("Ada\n"); // Send ACK string to host

  startTime    = micros();
  lastByteTime = lastAckTime = millis();

  while(state == 4){
    // Read button output or else you can't escape the while loop
    buttons();
    // Implementation is a simple finite-state machine.
    // Regardless of mode, check for serial input each time:
    t = millis();
    if((bytesBuffered < 256) && ((c = Serial.read()) >= 0)) {
      buffer[indexIn++] = c;
      bytesBuffered++;
      lastByteTime = lastAckTime = t; // Reset timeout counters
    } 
    else {
      // No data received.  If this persists, send an ACK packet
      // to host once every second to alert it to our presence.
      if((t - lastAckTime) > 1000) {
        Serial.print("Ada\n"); // Send ACK string to host
        lastAckTime = t; // Reset counter
      }
      // If no data received for an extended time, turn off all LEDs.
      if((t - lastByteTime) > serialTimeout) {
        memset(leds, 0,  NUM_LEDS * sizeof(struct CRGB)); //filling Led array by zeroes
        FastLED.show();
        lastByteTime = t; // Reset counter
        state -= 1;
      }
    }

    switch(mode) {
      case MODE_HEADER:
        // In header-seeking mode.  Is there enough data to check?
        if(bytesBuffered >= HEADERSIZE) {
          // Indeed.  Check for a 'magic word' match.
          for(i=0; (i<MAGICSIZE) && (buffer[indexOut++] == magic[i++]););
          if(i == MAGICSIZE) {
            // Magic word matches.  Now how about the checksum?
            hi  = buffer[indexOut++];
            lo  = buffer[indexOut++];
            chk = buffer[indexOut++];
            if(chk == (hi ^ lo ^ 0x55)) {
              // Checksum looks valid.  Get 16-bit LED count, add 1
              // (# LEDs is always > 0) and multiply by 3 for R,G,B.
              bytesRemaining = 3L * (256L * (long)hi + (long)lo + 1L);
              bytesBuffered -= 3;
              outPos = 0;
              memset(leds, 0,  NUM_LEDS * sizeof(struct CRGB));
              mode           = MODE_DATA; // Proceed to latch wait mode
            } 
            else {
              // Checksum didn't match; search resumes after magic word.
              indexOut  -= 3; // Rewind
            }
          } // else no header match.  Resume at first mismatched byte.
          bytesBuffered -= i;
        }
        break;
  
      case MODE_DATA:
        if(bytesRemaining > 0) {
          if(bytesBuffered > 0) {
            if (outPos < sizeof(leds))
              ledsRaw[outPos++] = buffer[indexOut++];   // Issue next byte
            bytesBuffered--;
            bytesRemaining--;
          }
          // If serial buffer is threatening to underrun, start
          // introducing progressively longer pauses to allow more
          // data to arrive (up to a point).
        } 
        else {
          // End of data -- issue latch:
          startTime  = micros();
          mode       = MODE_HEADER; // Begin next header search
          FastLED.show();
        }
    } // end switch
  } // end while
}

void easeReading(){
  // ease the change amount of the reading
  // this allows preciser changement on lower readings
  if (reading < 10) {
    changeamnt = 1;
  } else if (reading < 20) {
    changeamnt = 2;
  } else {
    changeamnt = 5;
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
