// Hardware Information:
// Generic RP2040 running at 133MHz with 16MB of flash

// Brilliant resource for dealing with hexagonal coordinates. https://www.redblobgames.com/grids/hexagons/
// Might be useful for animations and stuff like that.

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <Adafruit_NeoPixel.h>

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

#define LED_PIN 22
#define LED_COUNT 140

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);

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

// DIAGNOSTICS
// 1 = Full button test (1 and 0)
// 2 = Button test (button number)
// 3 = MIDI output test
int diagnostics = 0;

// Define digital button matrix pins
const byte columns[] = { 14, 15, 13, 12, 11, 10, 9, 8, 7, 6 };  // Column pins in order from right to left
const int m1p = 4;                                              // Multiplexing chip control pins
const int m2p = 5;
const int m4p = 2;
const int m8p = 3;
// 16 & 17 reserved for lights.
const byte columnCount = sizeof(columns);          // The number of columns in the matrix
const byte rowCount = 14;                          // The number of rows in the matrix
const byte elementCount = columnCount * rowCount;  // The number of elements in the matrix

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
//hacky macro because I (Jared) messed up the board layout - I'll do better next time! xD

// MIDI note value tables
const byte wickiHaydenLayout[elementCount] = {
  ROW_FLIP(BK_TOG, 90, 92, 94, 96, 98, 100, 102, 104, 106),
  ROW_FLIP(83, 85, 87, 89, 91, 93, 95, 97, 99, 101),
  ROW_FLIP(LGH_MD, 78, 80, 82, 84, 86, 88, 90, 92, 94),
  ROW_FLIP(71, 73, 75, 77, 79, 81, 83, 85, 87, 89),
  ROW_FLIP(LAY_MD, 66, 68, 70, 72, 74, 76, 78, 80, 82),
  ROW_FLIP(59, 61, 63, 65, 67, 69, 71, 73, 75, 77),
  ROW_FLIP(OCT_UP, 54, 56, 58, 60, 62, 64, 66, 68, 70),
  ROW_FLIP(47, 49, 51, 53, 55, 57, 59, 61, 63, 65),
  ROW_FLIP(OCT_DN, 42, 44, 46, 48, 50, 52, 54, 56, 58),
  ROW_FLIP(35, 37, 39, 41, 43, 45, 47, 49, 51, 53),
  ROW_FLIP(PTB_UP, 30, 32, 34, 36, 38, 40, 42, 44, 46),
  ROW_FLIP(23, 25, 27, 29, 31, 33, 35, 37, 39, 41),
  ROW_FLIP(PTB_DN, 18, 20, 22, 24, 26, 28, 30, 32, 34),
  ROW_FLIP(11, 13, 15, 17, 19, 21, 23, 25, 27, 29)
};
const byte harmonicTableLayout[elementCount] = {
  ROW_FLIP(BK_TOG, 83, 76, 69, 62, 55, 48, 41, 34, 27),
  ROW_FLIP(86, 79, 72, 65, 58, 51, 44, 37, 30, 23),
  ROW_FLIP(LGH_MD, 82, 75, 68, 61, 54, 47, 40, 33, 26),
  ROW_FLIP(85, 78, 71, 64, 57, 50, 43, 36, 29, 22),
  ROW_FLIP(LAY_MD, 81, 74, 67, 60, 53, 46, 39, 32, 25),
  ROW_FLIP(84, 77, 70, 63, 56, 49, 42, 35, 28, 21),
  ROW_FLIP(OCT_UP, 80, 73, 66, 59, 52, 45, 38, 31, 24),
  ROW_FLIP(83, 76, 69, 62, 55, 48, 41, 34, 27, 20),
  ROW_FLIP(OCT_DN, 79, 72, 65, 58, 51, 44, 37, 30, 23),
  ROW_FLIP(82, 75, 68, 61, 54, 47, 40, 33, 26, 19),
  ROW_FLIP(PTB_UP, 78, 71, 64, 57, 50, 43, 36, 29, 22),
  ROW_FLIP(81, 74, 67, 60, 53, 46, 39, 32, 25, 18),
  ROW_FLIP(PTB_DN, 77, 70, 63, 56, 49, 42, 35, 28, 21),
  ROW_FLIP(80, 73, 66, 59, 52, 45, 38, 31, 24, 17)
};
const byte gerhardLayout[elementCount] = {
  ROW_FLIP(BK_TOG, 74, 73, 72, 71, 70, 69, 68, 67, 66),
  ROW_FLIP(71, 70, 69, 68, 67, 66, 65, 64, 63, 62),
  ROW_FLIP(LGH_MD, 67, 66, 65, 64, 63, 62, 61, 60, 59),
  ROW_FLIP(64, 63, 62, 61, 60, 59, 58, 57, 56, 55),
  ROW_FLIP(LAY_MD, 60, 59, 58, 57, 56, 55, 54, 53, 52),
  ROW_FLIP(57, 56, 55, 54, 53, 52, 51, 50, 49, 48),
  ROW_FLIP(OCT_UP, 53, 52, 51, 50, 49, 48, 47, 46, 45),
  ROW_FLIP(50, 49, 48, 47, 46, 45, 44, 43, 42, 41),
  ROW_FLIP(OCT_DN, 46, 45, 44, 43, 42, 41, 40, 39, 38),
  ROW_FLIP(43, 42, 41, 40, 39, 38, 37, 36, 35, 34),
  ROW_FLIP(PTB_UP, 39, 38, 37, 36, 35, 34, 33, 32, 31),
  ROW_FLIP(36, 35, 34, 33, 32, 31, 30, 29, 28, 27),
  ROW_FLIP(PTB_DN, 32, 31, 30, 29, 28, 27, 26, 25, 24),
  ROW_FLIP(29, 28, 27, 26, 25, 24, 23, 22, 21, 20)
};
// LEDs for OCT_UP/OCT_DN status.
const byte octUpSW = 70 - 1;
const byte octDnSW = 90 - 1;
const byte layMdSW = 50 - 1;

const byte *currentLayout = wickiHaydenLayout;

// Global time variables
unsigned long currentTime;  // Program loop consistent variable for time in milliseconds since power on
const byte debounceTime = 2;  // Global digital button debounce time in milliseconds

// Variables for holding digital button states and activation times
byte activeButtons[elementCount];               // Array to hold current note button states
byte previousActiveButtons[elementCount];       // Array to hold previous note button states for comparison
unsigned long activeButtonsTime[elementCount];  // Array to track last note button activation time for debounce


// MIDI channel assignment
byte midiChannel = 1;  // Current MIDI channel (changed via user input)

// Octave modifier
int octave = 0;  // Apply a MIDI note number offset (changed via user input in steps of 12)

bool blackKeys = true;  // whether the black keys should be dimmer

// Velocity levels
byte midiVelocity = 95;  // Default velocity
// END SETUP SECTION
// ------------------------------------------------------------------------------------------------------------------------------------------------------------

void setup() {
#if defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040)
  // Manual begin() is required on core without built-in support for TinyUSB such as mbed rp2040
  TinyUSB_Device_Init(0);
#endif

  //usb_midi.setStringDescriptor("HexBoard MIDI");

  // Initialize MIDI, and listen to all MIDI channels
  // This will also call usb_midi's begin()
  MIDI.begin(MIDI_CHANNEL_OMNI);

  // Set serial to make uploads work without bootsel button
  Serial.begin(115200);

  // Set pinModes for the digital button matrix.
  for (int pinNumber = 0; pinNumber < columnCount; pinNumber++)  // For each column pin...
  {
    pinMode(columns[pinNumber], INPUT_PULLUP);  // set the pinMode to INPUT_PULLUP (+3.3V / HIGH).
  }
  pinMode(m1p, OUTPUT);  // Setting the row multiplexer pins to output.
  pinMode(m2p, OUTPUT);
  pinMode(m4p, OUTPUT);
  pinMode(m8p, OUTPUT);
  Serial.begin(115200);
  strip.begin();             // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();              // Turn OFF all pixels ASAP
  strip.setBrightness(128);  // Set BRIGHTNESS (max = 255)
  setOctLED();
  setLayoutLEDs();
  strip.setPixelColor(layMdSW, 255, 0, 0);

  // wait until device mounted
  while (!TinyUSBDevice.mounted()) delay(1);

  // Print diagnostic troubleshooting information to serial monitor
  diagnosticTest();
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// START LOOP SECTION
void loop() {
  // Store the current time in a uniform variable for this program loop
  currentTime = millis();

  // Read and store the digital button states of the scanning matrix
  readDigitalButtons();

  // Act on those buttons
  playNotes();

  // Held Buttons
  heldButtons();

  // Do the LEDS
  strip.show();

  // Read any new MIDI messages
  MIDI.read();
}
// END LOOP SECTION
// ------------------------------------------------------------------------------------------------------------------------------------------------------------


// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// START FUNCTIONS SECTION

void diagnosticTest() {
  if (diagnostics > 0) {
    Serial.println("Zach was here");
  }
}

void commandPress(byte command) {
  // Keep octave between ~-12~ 0 and 24
  if (command == OCT_DN) {
    if (octave >= 12) {
      octave -= 12;
      setOctLED();
      strip.setPixelColor(octDnSW, 120, 120, 120);
    }
  } else if (command == OCT_UP) {
    if (octave <= 12) {
      octave += 12;
      setOctLED();
      strip.setPixelColor(octUpSW, 120, 120, 120);
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
void commandRelease(byte command) {
  if (command == OCT_DN) {
    setOctLED();
  } else if (command == OCT_UP) {
    setOctLED();
  }
}

// END FUNCTIONS SECTION
// ------------------------------------------------------------------------------------------------------------------------------------------------------------

// END OF PROGRAM
// ------------------------------------------------------------------------------------------------------------------------------------------------------------