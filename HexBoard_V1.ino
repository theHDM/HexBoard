// Hardware Information:
// Teensy LC set to 48MHz with USB type MIDI
#include <FastLED.h>

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_UART.h"
#include "Adafruit_BLEMIDI.h"
#include "BluefruitConfig.h"

#define FACTORYRESET_ENABLE         0
#define MINIMUM_FIRMWARE_VERSION    "0.7.0"

#define LEDS_PIN 17
#define NUM_LEDS 140

Adafruit_BluefruitLE_UART ble(Serial1, -1);

Adafruit_BLEMIDI blemidi(ble);

bool bleModuleEnabled = false;

bool isConnected = false;

// Bluetooth error messages
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}
//Bluetooth callbacks
void connected(void)
{
  isConnected = true;
  Serial.println("CONNECTED!");
  //Zach make bluetooth light go solid once connected
}
void disconnected(void)
{
  Serial.println("disconnected");
  isConnected = false;
  //Zach make bluetooth light start blinking again once disconnected
}

CRGB leds[NUM_LEDS];

void init_leds()
{
  FastLED.addLeds<WS2811, LEDS_PIN, RGB>(leds, NUM_LEDS);
  /*Max Brightness. Recommended 100 or lower for usb power to stay under 2 amps. Increase up to 255 at your own risk.
  Board can handle full power (theoretically 8.4 amps at max brightness) while only getting warm, but external power
  is required to meet this. Use capable power supply on EXT PWR headers.*/
  FastLED.setBrightness(200);
  FastLED.setMaxPowerInVoltsAndMilliamps(5,1000);
  for (int i=0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
}

bool bluetooth = false;
void init_bluetooth()
{
/* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ) {
      error(F("Couldn't factory reset"));
    }
  }

  //ble.sendCommandCheckOK(F("AT+uartflow=off"));
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  /* Set BLE callbacks */
  ble.setConnectCallback(connected);
  ble.setDisconnectCallback(disconnected);

  Serial.println(F("Enable MIDI: "));
  if ( ! blemidi.begin(true) )
  {
    error(F("Could not enable Bluetooth MIDI"));
  }

  ble.verbose(false);
  Serial.print(F("Waiting for a connection..."));
  
  bluetooth = true;
  //Zach add bluetooth button blinking here until connection made
}
void shutdown_bluetooth()
{
  ble.end();
  bluetooth = false;
}

//
// Button matrix and LED locations
// Portrait orientation top view:
//            9   8   7   6   5   4   3   2   1
//         20  19  18  17  16  15  14  13  12  11
//           29  28  27  26  25  24  23  22  21
//         40  39  38  37  36  35  34  33  32  31
//           49  48  47  46  45  44  43  42  41
//         60  59  58  57  56  55  54  53  52  51
//   10      69  68  67  66  65  64  63  62  61
// 30      80  79  78  77  76  75  74  73  72  71
//   50      89  88  87  86  85  84  83  82  81
// 70     100 99  98  97  96  95  94  93  92  91
//   90     109 108 107 106 105 104 103 102 101
//110     120 119 118 117 116 115 114 113 112 111
//  130     129 128 127 126 125 124 123 122 121
//        140 139 138 137 136 135 134 133 132 131

//DIAGNOSTICS
int diagnostics = 0;

// Define digital button matrix pins
const byte columns[] = { 25, 24,  9,  8,  7,  6,  5,  4,  3,  2};                // Column pins in order from right to left
const byte rows[]    = {10, 11, 12, 13, 14, 15, 16, 18, 19, 20, 21, 22, 23, 26}; // Row pins in order from top to bottom
// 16 & 17 reserved for lights.
const byte columnCount      = sizeof(columns);                          // The number of columns in the matrix
const byte rowCount         = sizeof(rows);                             // The number of rows in the matrix
const byte elementCount     = columnCount * rowCount;// The number of elements in the matrix

// Since MIDI only uses 7 bits, we can give greater values special meanings.
// (see commandPress)
const int OCT_DN = 128;
const int OCT_UP = 129;
const int BT_TOG = 130;
const int LAY_MD = 131;
const int LGH_MD = 132;
const int PTB_DN = 133;
const int PTB_UP = 134;
const int BK_TOG = 135;
const int UNUSED = 255;

#define ROW_FLIP(x, ix, viii, vii, vi, v, iv, iii, ii, i) i, ii, iii, iv, v, vi, vii, viii, ix, x
//hacky macro because we typed them in wrong???
//#define NO_FLIP(x, ix, viii, vii, vi, v, iv, iii, ii, i) i, x, ix, viii, vii, vi, v, iv, iii, ii


// MIDI note value tables
const byte wickiHaydenLayout[elementCount] = {
ROW_FLIP(BK_TOG, 90,  92,  94,  96,  98, 100, 102, 104, 106),
ROW_FLIP(     83,  85,  87,  89,  91,  93,  95,  97,  99, 101),
ROW_FLIP(LGH_MD, 78,  80,  82,  84,  86,  88,  90,  92,  94),
ROW_FLIP(     71,  73,  75,  77,  79,  81,  83,  85,  87,  89),
ROW_FLIP(LAY_MD, 66,  68,  70,  72,  74,  76,  78,  80,  82),
ROW_FLIP(     59,  61,  63,  65,  67,  69,  71,  73,  75,  77),
ROW_FLIP(OCT_UP, 54,  56,  58,  60,  62,  64,  66,  68,  70),
ROW_FLIP(     47,  49,  51,  53,  55,  57,  59,  61,  63,  65),
ROW_FLIP(OCT_DN, 42,  44,  46,  48,  50,  52,  54,  56,  58),
ROW_FLIP(     35,  37,  39,  41,  43,  45,  47,  49,  51,  53),
ROW_FLIP(PTB_UP, 30,  32,  34,  36,  38,  40,  42,  44,  46),
ROW_FLIP(     23,  25,  27,  29,  31,  33,  35,  37,  39,  41),
ROW_FLIP(PTB_DN, 18,  20,  22,  24,  26,  28,  30,  32,  34),
ROW_FLIP(     11,  13,  15,  17,  19,  21,  23,  25,  27,  29)
};
const byte harmonicTableLayout[elementCount] = {
ROW_FLIP(BK_TOG, 83,  76,  69,  62,  55,  48,  41,  34,  27),
ROW_FLIP(     86,  79,  72,  65,  58,  51,  44,  37,  30,  23),
ROW_FLIP(LGH_MD, 82,  75,  68,  61,  54,  47,  40,  33,  26),
ROW_FLIP(     85,  78,  71,  64,  57,  50,  43,  36,  29,  22),
ROW_FLIP(LAY_MD, 81,  74,  67,  60,  53,  46,  39,  32,  25),
ROW_FLIP(     84,  77,  70,  63,  56,  49,  42,  35,  28,  21),
ROW_FLIP(OCT_UP, 80,  73,  66,  59,  52,  45,  38,  31,  24),
ROW_FLIP(     83,  76,  69,  62,  55,  48,  41,  34,  27,  20),
ROW_FLIP(OCT_DN, 79,  72,  65,  58,  51,  44,  37,  30,  23),
ROW_FLIP(     82,  75,  68,  61,  54,  47,  40,  33,  26,  19),
ROW_FLIP(PTB_UP, 78,  71,  64,  57,  50,  43,  36,  29,  22),
ROW_FLIP(     81,  74,  67,  60,  53,  46,  39,  32,  25,  18),
ROW_FLIP(PTB_DN, 77,  70,  63,  56,  49,  42,  35,  28,  21),
ROW_FLIP(     80,  73,  66,  59,  52,  45,  38,  31,  24,  17)
};
const byte gerhardLayout[elementCount] = {
ROW_FLIP(BK_TOG, 74,  73,  72,  71,  70,  69,  68,  67,  66),
ROW_FLIP(     71,  70,  69,  68,  67,  66,  65,  64,  63,  62),
ROW_FLIP(LGH_MD, 67,  66,  65,  64,  63,  62,  61,  60,  59),
ROW_FLIP(     64,  63,  62,  61,  60,  59,  58,  57,  56,  55),
ROW_FLIP(LAY_MD, 60,  59,  58,  57,  56,  55,  54,  53,  52),
ROW_FLIP(     57,  56,  55,  54,  53,  52,  51,  50,  49,  48),
ROW_FLIP(OCT_UP, 53,  52,  51,  50,  49,  48,  47,  46,  45),
ROW_FLIP(     50,  49,  48,  47,  46,  45,  44,  43,  42,  41),
ROW_FLIP(OCT_DN, 46,  45,  44,  43,  42,  41,  40,  39,  38),
ROW_FLIP(     43,  42,  41,  40,  39,  38,  37,  36,  35,  34),
ROW_FLIP(PTB_UP, 39,  38,  37,  36,  35,  34,  33,  32,  31),
ROW_FLIP(     36,  35,  34,  33,  32,  31,  30,  29,  28,  27),
ROW_FLIP(PTB_DN, 32,  31,  30,  29,  28,  27,  26,  25,  24),
ROW_FLIP(     29,  28,  27,  26,  25,  24,  23,  22,  21,  20)
};
// LEDs for OCT_UP/OCT_DN status.
const byte octUpSW = 70 - 1;
const byte octDnSW = 90 - 1;
const byte layMdSW = 50 - 1;

const byte *currentLayout = wickiHaydenLayout;

// Global time variables
unsigned long   currentTime; // Program loop consistent variable for time in milliseconds since power on
unsigned long timeBothPressed;
const byte      debounceTime = 2; // Global digital button debounce time in milliseconds

// Variables for holding digital button states and activation times
byte            activeButtons[elementCount];                            // Array to hold current note button states
byte            previousActiveButtons[elementCount];                    // Array to hold previous note button states for comparison
unsigned long   activeButtonsTime[elementCount];                        // Array to track last note button activation time for debounce


// MIDI channel assignment
byte midiChannel = 0;                                                   // Current MIDI channel (changed via user input)

// Octave modifier
int octave = 0;// Apply a MIDI note number offset (changed via user input in steps of 12)

bool blackKeys = true; // whether the black keys should be dimmer

// Velocity levels
byte velocity = 95;                                                     // Default velocity
// END SETUP SECTION
// ------------------------------------------------------------------------------------------------------------------------------------------------------------

void setup()
{ 
  // Set pinModes for the digital button matrix.
  for (int pinNumber = 0; pinNumber < columnCount; pinNumber++)                   // For each column pin...
  {
    pinMode(columns[pinNumber], INPUT_PULLUP);                                  // set the pinMode to INPUT_PULLUP (+3.3V / HIGH).
  }
  for (int pinNumber = 0; pinNumber < rowCount; pinNumber++)                      // For each row pin...
  {
    pinMode(rows[pinNumber], INPUT);                                            // Set the pinMode to INPUT (0V / LOW).
  }
  Serial.begin(115200);
  if (bleModuleEnabled == true){
    init_bluetooth();
  }
  init_leds();
  setOctLED();
  setLayoutLEDs();
  leds[layMdSW] = CRGB::Red;

  // Print diagnostic troubleshooting information to serial monitor
  diagnosticTest();
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// START LOOP SECTION
void loop()
{
  // Store the current time in a uniform variable for this program loop
  currentTime = millis();

  // Read and store the digital button states of the scanning matrix
  readDigitalButtons();

  // Act on those buttons
  playNotes();

  // Held Buttons
  heldButtons();

  // Do the LEDS
  FastLED.show();
}
// END LOOP SECTION
// ------------------------------------------------------------------------------------------------------------------------------------------------------------


// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// START FUNCTIONS SECTION

void diagnosticTest()
{
  if (diagnostics > 0) {
    Serial.println("Zach was here");
  }
}

void readDigitalButtons()
{
  if (diagnostics == 1){
    Serial.println();
  }
  // Button Deck
  for (byte rowIndex = 0; rowIndex < rowCount; rowIndex++) // Iterate through each of the row pins.
  {
    byte rowPin = rows[rowIndex]; // Hold the currently selected row pin in a variable.
    pinMode(rowPin, OUTPUT); // Set that row pin to OUTPUT mode and...
    digitalWrite(rowPin, LOW); // set the pin state to LOW turning it into a temporary ground.
    for  (byte columnIndex = 0; columnIndex < columnCount; columnIndex++)// Now iterate through each of the column pins that are connected to the current row pin.
    {
      byte columnPin = columns[columnIndex]; // Hold the currently selected column pin in a variable.
      pinMode(columnPin, INPUT_PULLUP); // Set that row pin to INPUT_PULLUP mode (+3.3V / HIGH).
      byte buttonNumber = columnIndex + (rowIndex * columnCount); // Assign this location in the matrix a unique number.
      delayMicroseconds(50); // Delay to give the pin modes time to change state (false readings are caused otherwise).
      previousActiveButtons[buttonNumber] = activeButtons[buttonNumber]; // Track the "previous" variable for comparison.
      byte buttonState = digitalRead(columnPin); // (don't)Invert reading due to INPUT_PULLUP, and store the currently selected pin state.
      if (buttonState == LOW ) {
        if (diagnostics == 1){
          Serial.print("1");
        } else if (diagnostics == 2) {
          Serial.println(buttonNumber);
        }
        if (!previousActiveButtons[buttonNumber]) {
          // newpress time
          activeButtonsTime[buttonNumber] = millis();
        }
        // TODO: Implement debounce?
        activeButtons[buttonNumber] = 1;
      } else {
        // Otherwise, the button is inactive, write a 0.
        if (diagnostics == 1){
          Serial.print("0");
        }
        activeButtons[buttonNumber] = 0;
      }
      // Set the selected column pin back to INPUT mode (0V / LOW).
      pinMode(columnPin, INPUT);
    }
    // Set the selected row pin back to INPUT mode (0V / LOW).
    pinMode(rowPin, INPUT);
  }
}

void playNotes()
{
  for (int i = 0; i < elementCount; i++) // For all buttons in the deck
  {
    if (activeButtons[i] != previousActiveButtons[i]) // If a change is detected
    {
      if (activeButtons[i] == 1) // If the button is active (newpress)
      {
        if (currentLayout[i] < 128) {
          //leds[i] = CRGB::White;
          leds[i] = CHSV((currentLayout[i] % 12) * 21, 255, 255);
          noteOn(midiChannel, (currentLayout[i] + octave) % 128 , velocity);
        } else {
          commandPress(currentLayout[i]);
        }
      } else {
      // If the button is inactive (released)
        if (currentLayout[i] < 128) {
          setLayoutLED(i);
          //leds[i] ;
          noteOff(midiChannel, (currentLayout[i] + octave) % 128, 0);
        } else {
          commandRelease(currentLayout[i]);
        }
      }
    }
  }
}

void heldButtons()
{
  for (int i = 0; i < elementCount; i++) {
    if (activeButtons[i]) {
      //if (
    }
  }
}

// MIDI PACKET FUNCTIONS

// Send MIDI Note On
void noteOn(byte channel, byte pitch, byte velocity)
{
  if (isConnected == true){
    blemidi.send(0x90, pitch, velocity);
  }
  else {
    usbMIDI.sendNoteOn(pitch, velocity, channel);
  }
}
// Send MIDI Note Off
void noteOff(byte channel, byte pitch, byte velocity)
{
  if (isConnected == true){
    blemidi.send(0x80, pitch, velocity);
  }
  else {
    usbMIDI.sendNoteOff(pitch, velocity, channel);
  }
}

void commandPress(byte command)
{
  // Keep octave between ~-12~ 0 and 24
  if(command == OCT_DN) {
    if (octave >= 12) {
      octave -= 12;
      setOctLED(); leds[octDnSW] = CRGB::White;
    }
  } else if (command == OCT_UP) {
    if (octave <= 12) {
      octave += 12;
      setOctLED(); leds[octUpSW] = CRGB::White;
    }
  } else if (command == LAY_MD) {
    if (currentLayout == wickiHaydenLayout) {
      currentLayout = harmonicTableLayout;
    } else if (currentLayout == harmonicTableLayout) {
      currentLayout = gerhardLayout;
    } else {
      currentLayout = wickiHaydenLayout;
    }
    setLayoutLEDs();
  } else if (command == BK_TOG) {
    blackKeys = !blackKeys;
    setLayoutLEDs();
  }
}
void commandRelease(byte command)
{
  if (command == OCT_DN) {
    setOctLED();
  } else if (command == OCT_UP) {
    setOctLED();
  }
}


void setOctLED()
{
  /*if (octave <= -12) {
    leds[octUpSW].setRGB(0xA0, 0, 0x20);
    leds[octDnSW] = CRGB::Black; // No lower to go.
  } else */if (octave <= 0) {
    leds[octUpSW] = CRGB::Purple;
    leds[octDnSW] = CRGB::Black; // No lower to go.
  } else if (octave <= 12) {
    leds[octUpSW] = CRGB::Blue;
    leds[octDnSW].setRGB(0xA0, 0, 0x20);
  } else if (octave <= 24) {
    leds[octUpSW] = CRGB::Black; // No higher to go.
    leds[octDnSW] = CRGB::Purple;
  }
}

void setLayoutLEDs()
{
  for (int i = 0; i < elementCount; i++) {
    if (currentLayout[i] <= 127) {
      setLayoutLED(i);
    }
  }
}
void setLayoutLED(int i) {
  leds[i] = CHSV((currentLayout[i] % 12) * 21, 255, 120);
  // black keys darker
  if(blackKeys){
    // LEET programmers stuff
    switch(currentLayout[i] % 12) {
      // If it is one of the black keys, fall through to case 10.
      case 1:
      case 3:
      case 6:
      case 8:
      // bitshift by 2 (efficient division by four)
      case 10: leds[i] >>= 2; break;
      // otherwise it was a white key. Do nothing
      default: break;
    }
  }
}

// Control Change
// 1st byte = Event type (0x0B = Control Change).
// 2nd byte = Event type bitwise ORed with MIDI channel.
// 3rd byte = MIDI CC number (7-bit range 0-127).
// 4th byte = Control value (7-bit range 0-127).
void controlChange(byte channel, byte control, byte value)
{

}

// Pitch Bend
// (14 bit value 0-16363, neutral position = 8192)
// 1st byte = Event type (0x0E = Pitch bend change).
// 2nd byte = Event type bitwise ORed with MIDI channel.
// 3rd byte = The 7 least significant bits of the value.
// 4th byte = The 7 most significant bits of the value.
void pitchBendChange(byte channel, byte lowValue, byte highValue)
{

}

// END FUNCTIONS SECTION
// ------------------------------------------------------------------------------------------------------------------------------------------------------------

// END OF PROGRAM
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
