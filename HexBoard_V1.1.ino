// Copyright 2022-2023 Jared DeCook and Zach DeCook
// Licenced under the GNU GPL Version 3.
// Hardware Information:
// Generic RP2040 running at 133MHz with 16MB of flash
// https://github.com/earlephilhower/arduino-pico
// (Additional boards manager URL: https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json)
// Tools > USB Stack > (Adafruit TinyUSB)
// Sketch > Export Compiled Binary
//
// Brilliant resource for dealing with hexagonal coordinates. https://www.redblobgames.com/grids/hexagons/
// Might be useful for animations and stuff like that.

// Menu library documentation https://github.com/Spirik/GEM

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "LittleFS.h"
#include <MIDI.h>
#include <Adafruit_NeoPixel.h>
#define GEM_DISABLE_GLCD
#include <GEM_u8g2.h>
#include <Wire.h>
#include <Rotary.h>

// Change before compile depending on target hardware
// 1 = HexBoard 1.0 (dev unit)
// 2 = HexBoard 1.1 (first retail unit)
#define ModelNumber 2

// USB MIDI object //
Adafruit_USBD_MIDI usb_midi;
// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

// LED SETUP //
#define LED_PIN 22
#define LED_COUNT 140
#if ModelNumber == 1
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);
int stripBrightness = 110;
int defaultBrightness = 70;
int dimBrightness = 20;
int pressedBrightness = 255;
#elif ModelNumber == 2
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
int stripBrightness = 110;
int defaultBrightness = 140;
int dimBrightness = 40;
int pressedBrightness = 255;
#endif


// ENCODER SETUP //
#define ROTA 20  // Rotary encoder A
#define ROTB 21  // Rotary encoder B
Rotary rotary = Rotary(ROTA, ROTB);
const int encoderClick = 24;
int encoderState = 0;
int encoderLastState = 1;
int8_t encoder_val = 0;
uint8_t encoder_state;

// Create an instance of the U8g2 graphics library.
#if ModelNumber == 1
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/U8X8_PIN_NONE);
#elif ModelNumber == 2
U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2(U8G2_R2, /* reset=*/U8X8_PIN_NONE);
#endif
int screenBrightness = stripBrightness / 2;

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

// DIAGNOSTICS //
// 1 = Full button test (1 and 0)
// 2 = Button test (button number)
// 3 = MIDI output test
// 4 = Loop timing readout in milliseconds
int diagnostics = 0;

// BUTTON MATRIX PINS //
#if ModelNumber == 1
const byte columns[] = { 14, 15, 13, 12, 11, 10, 9, 8, 7, 6 };  // Column pins in order from right to left
#elif ModelNumber == 2
const byte columns[] = { 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };  // New board revision
#endif
const int m1p = 4;  // Multiplexing chip control pins
const int m2p = 5;
const int m4p = 2;
const int m8p = 3;
// 16 & 17 reserved for lights.
const byte columnCount = sizeof(columns);          // The number of columns in the matrix
const byte rowCount = 14;                          // The number of rows in the matrix
const byte elementCount = columnCount * rowCount;  // The number of elements in the matrix

// Since MIDI only uses 7 bits, we can give greater values special meanings.
// (see commandPress)
const int CMDB_1 = 128;
const int CMDB_2 = 129;
const int CMDB_3 = 130;
const int CMDB_4 = 131;
const int CMDB_5 = 132;
const int CMDB_6 = 133;
const int CMDB_7 = 134;
const int UNUSED = 255;

// LED addresses for CMD buttons.
#if ModelNumber == 1
const byte cmdBtn1 = 10 - 1;
const byte cmdBtn2 = 30 - 1;
const byte cmdBtn3 = 50 - 1;
const byte cmdBtn4 = 70 - 1;
const byte cmdBtn5 = 90 - 1;
const byte cmdBtn6 = 110 - 1;
const byte cmdBtn7 = 130 - 1;
#else
const byte cmdBtn1 = 0;
const byte cmdBtn2 = 20;
const byte cmdBtn3 = 40;
const byte cmdBtn4 = 60;
const byte cmdBtn5 = 80;
const byte cmdBtn6 = 100;
const byte cmdBtn7 = 120;
#endif

// MIDI NOTE LAYOUTS //
#if ModelNumber == 1
#define ROW_FLIP(x, ix, viii, vii, vi, v, iv, iii, ii, i) i, ii, iii, iv, v, vi, vii, viii, ix, x
//hacky macro because I (Jared) messed up the board layout - I'll do better next time! xD
#else
#define ROW_FLIP(i, ii, iii, iv, v, vi, vii, viii, ix, x) i, ii, iii, iv, v, vi, vii, viii, ix, x
//fixed it the second time around!
#endif

// MIDI note layout tables
const byte wickiHaydenLayout[elementCount] = {
  ROW_FLIP(CMDB_1, 90, 92, 94, 96, 98, 100, 102, 104, 106),
  ROW_FLIP(83, 85, 87, 89, 91, 93, 95, 97, 99, 101),
  ROW_FLIP(CMDB_2, 78, 80, 82, 84, 86, 88, 90, 92, 94),
  ROW_FLIP(71, 73, 75, 77, 79, 81, 83, 85, 87, 89),
  ROW_FLIP(CMDB_3, 66, 68, 70, 72, 74, 76, 78, 80, 82),
  ROW_FLIP(59, 61, 63, 65, 67, 69, 71, 73, 75, 77),
  ROW_FLIP(CMDB_4, 54, 56, 58, 60, 62, 64, 66, 68, 70),
  ROW_FLIP(47, 49, 51, 53, 55, 57, 59, 61, 63, 65),
  ROW_FLIP(CMDB_5, 42, 44, 46, 48, 50, 52, 54, 56, 58),
  ROW_FLIP(35, 37, 39, 41, 43, 45, 47, 49, 51, 53),
  ROW_FLIP(CMDB_6, 30, 32, 34, 36, 38, 40, 42, 44, 46),
  ROW_FLIP(23, 25, 27, 29, 31, 33, 35, 37, 39, 41),
  ROW_FLIP(CMDB_7, 18, 20, 22, 24, 26, 28, 30, 32, 34),
  ROW_FLIP(11, 13, 15, 17, 19, 21, 23, 25, 27, 29)
};
const byte harmonicTableLayout[elementCount] = {
  ROW_FLIP(CMDB_1, 95, 88, 81, 74, 67, 60, 53, 46, 39),
  ROW_FLIP(98, 91, 84, 77, 70, 63, 56, 49, 42, 35),
  ROW_FLIP(CMDB_2, 94, 87, 80, 73, 66, 59, 52, 45, 38),
  ROW_FLIP(97, 90, 83, 76, 69, 62, 55, 48, 41, 34),
  ROW_FLIP(CMDB_3, 93, 86, 79, 72, 65, 58, 51, 44, 37),
  ROW_FLIP(96, 89, 82, 75, 68, 61, 54, 47, 40, 33),
  ROW_FLIP(CMDB_4, 92, 85, 78, 71, 64, 57, 50, 43, 36),
  ROW_FLIP(95, 88, 81, 74, 67, 60, 53, 46, 39, 32),
  ROW_FLIP(CMDB_5, 91, 84, 77, 70, 63, 56, 49, 42, 35),
  ROW_FLIP(94, 87, 80, 73, 66, 59, 52, 45, 38, 31),
  ROW_FLIP(CMDB_6, 90, 83, 76, 69, 62, 55, 48, 41, 34),
  ROW_FLIP(93, 86, 79, 72, 65, 58, 51, 44, 37, 30),
  ROW_FLIP(CMDB_7, 89, 82, 75, 68, 61, 54, 47, 40, 33),
  ROW_FLIP(92, 85, 78, 71, 64, 57, 50, 43, 36, 29)
};
const byte gerhardLayout[elementCount] = {
  ROW_FLIP(CMDB_1, 86, 85, 84, 83, 82, 81, 80, 79, 78),
  ROW_FLIP(83, 82, 81, 80, 79, 78, 77, 76, 75, 74),
  ROW_FLIP(CMDB_2, 79, 78, 77, 76, 75, 74, 73, 72, 71),
  ROW_FLIP(76, 75, 74, 73, 72, 71, 70, 69, 68, 67),
  ROW_FLIP(CMDB_3, 72, 71, 70, 69, 68, 67, 66, 65, 64),
  ROW_FLIP(69, 68, 67, 66, 65, 64, 63, 62, 61, 60),
  ROW_FLIP(CMDB_4, 65, 64, 63, 62, 61, 60, 59, 58, 57),
  ROW_FLIP(62, 61, 60, 59, 58, 57, 56, 55, 54, 53),
  ROW_FLIP(CMDB_5, 58, 57, 56, 55, 54, 53, 52, 51, 50),
  ROW_FLIP(55, 54, 53, 52, 51, 50, 49, 48, 47, 46),
  ROW_FLIP(CMDB_6, 51, 50, 49, 48, 47, 46, 45, 44, 43),
  ROW_FLIP(48, 47, 46, 45, 44, 43, 42, 41, 40, 39),
  ROW_FLIP(CMDB_7, 44, 43, 42, 41, 40, 39, 38, 37, 36),
  ROW_FLIP(41, 40, 39, 38, 37, 36, 35, 34, 33, 32)
};
const byte* currentLayout = wickiHaydenLayout;

const unsigned int pitches[128] = {
  16, 17, 18, 19, 21, 22, 23, 25, 26, 28, 29, 31,                                  // Octave 0
  33, 35, 37, 39, 41, 44, 46, 49, 52, 55, 58, 62,                                  // Octave 1
  65, 69, 73, 78, 82, 87, 93, 98, 104, 110, 117, 123,                              // Octave 2
  131, 139, 147, 156, 165, 175, 185, 196, 208, 220, 233, 247,                      // Octave 3
  262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494,                      // Octave 4
  523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988,                      // Octave 5
  1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976,          // Octave 6
  2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951,          // Octave 7
  4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902,          // Octave 8
  8372, 8870, 9397, 9956, 10548, 11175, 11840, 12544, 13290, 14080, 14917, 15804,  //9
  16744,                                                                           // C10
  17740,                                                                           // C#10
  18795,                                                                           // D10
  19912,                                                                           // D#10
  21096,                                                                           // E10
  22350,                                                                           // F10
  23680                                                                            // F#10
};
#define TONEPIN 23

// Global time variables
unsigned long currentTime = 0;   // Program loop consistent variable for time in milliseconds since power on
unsigned long previousTime = 0;  // Used to check speed of the loop in diagnostics mode 4
int loopTime = 0;                // Used to keep track of how long each loop takes. Useful for rate-limiting.
int screenTime = 0;              // Used to dim screen after a set time to prolong the lifespan of the OLED

// Pitch bend and mod wheel variables
int pitchBendNeutral = 0;   // The center position for the pitch bend "wheel." Could be adjusted for global tuning?
int pitchBendPosition = 0;  // The actual pitch bend variable used for sending MIDI
int pitchBendSpeed = 1024;  // The amount the pitch bend moves every time modPitchTime hits it's limit.
int modPitchTime = 0;       // Used to rate-limit pitch bend and mod wheel updates
byte modWheelPosition = 0;  // Actual mod wheel variable used for sending MIDI
byte modWheelSpeed = 6;     // The amount the mod wheel moves every time modPitchTime hits it's limit.
bool pitchModToggle = 1;    // Used to toggle between pitch bend and mod wheel

// Variables for holding digital button states and activation times
byte activeButtons[elementCount];               // Array to hold current note button states
byte previousActiveButtons[elementCount];       // Array to hold previous note button states for comparison
unsigned long activeButtonsTime[elementCount];  // Array to track last note button activation time for debounce

// Variables for sequencer mode
typedef struct {
  bool steps[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  bool bank = 0;
  int state = 0;  // TODO: change to enum: normal, mute, solo, mute&solo
  int instrument = 0;
} Lane;
#define STATE_MUTE 1
#define STATE_SOLO 2

#define NLANES 7
Lane lanes[NLANES];

int sequencerStep = 0;  // 0 - 31

// You have to push a button to switch modes
bool sequencerMode = 0;

/*
THESE CAN BE USED TO RESET THE SEQUENCE POSITION
void handleStart(void);
void handleContinue(void);
void handleStop(void);

THIS WILL BE USED FOR THE SEQUENCER CLOCK (24 frames per quarter note)
void handleTimeCodeQuarterFrame(byte data);
We should be able to adjust the division in the menu to have different sequence speeds.

*/

void handleNoteOn(byte channel, byte pitch, byte velocity) {
  // Rosegarden sends its metronome this way. Using for testing...
  if (1 == sequencerMode && 10 == channel && 100 == pitch) {
    sequencerPlayNextNote();
  }
}

// MENU SYSTEM SETUP //
// Create menu page object of class GEMPage. Menu page holds menu items (GEMItem) and represents menu level.
// Menu can have multiple menu pages (linked to each other) with multiple menu items each
GEMPage menuPageMain("HexBoard MIDI Controller");
GEMPage menuPageLayout("Layout");
GEMPage menuPageTesting("Unfinished Alpha Tests");

GEMItem menuItemLayout("Layout", menuPageLayout);
void wickiHayden();  //Forward declarations
void harmonicTable();
void gerhard();
GEMItem menuItemWickiHayden("Wicki-Hayden", wickiHayden);
GEMItem menuItemHarmonicTable("Harmonic Table", harmonicTable);
GEMItem menuItemGerhard("Gerhard", gerhard);

void setLayoutLEDs();  //Forward declaration
byte key = 0;
SelectOptionByte selectKeyOptions[] = { { "C", 0 }, { "C#", 1 }, { "D", 2 }, { "D#", 3 }, { "E", 4 }, { "F", 5 }, { "F#", 6 }, { "G", 7 }, { "G#", 8 }, { "A", 9 }, { "A#", 10 }, { "B", 11 } };
GEMSelect selectKey(sizeof(selectKeyOptions) / sizeof(SelectOptionByte), selectKeyOptions);
GEMItem menuItemKey("Key:", key, selectKey, setLayoutLEDs);

byte scale = 0;
SelectOptionByte selectScaleOptions[] = { { "ALL", 0 }, { "Major", 1 }, { "HarMin", 2 }, { "MelMin", 3 }, { "NatMin", 4 }, { "PentMaj", 5 }, { "PentMin", 6 }, { "Blues", 7 }, { "NONE", 8 }, { "NONE", 9 }, { "NONE", 10 }, { "NONE", 11 } };
GEMSelect selectScale(sizeof(selectScaleOptions) / sizeof(SelectOptionByte), selectScaleOptions);
GEMItem menuItemScale("Scale:", scale, selectScale, applySelectedScale);

std::array<byte, 12> selectedScale;
// Scale arrays
const std::array<byte, 12> chromaticScale = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
const std::array<byte, 12> majorScale = { 0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0, 0 };
const std::array<byte, 12> harmonicMinorScale = { 0, 2, 3, 5, 7, 8, 11, 0, 0, 0, 0, 0 };
const std::array<byte, 12> melodicMinorScale = { 0, 2, 3, 5, 7, 9, 11, 0, 0, 0, 0, 0 };
const std::array<byte, 12> naturalMinorScale = { 0, 2, 3, 5, 7, 8, 10, 0, 0, 0, 0, 0 };
const std::array<byte, 12> pentatonicMajorScale = { 0, 2, 4, 7, 9, 0, 0, 0, 0, 0, 0, 0 };
const std::array<byte, 12> pentatonicMinorScale = { 0, 3, 5, 7, 10, 0, 0, 0, 0, 0, 0, 0 };
const std::array<byte, 12> bluesScale = { 0, 3, 5, 6, 7, 10, 0, 0, 0, 0, 0, 0 };

// Function to apply the selected scale
void applySelectedScale() {
  switch (scale) {
    case 0:  // All notes
      selectedScale = chromaticScale;
      break;
    case 1:  // Major scale
      selectedScale = majorScale;
      break;
    case 2:  // Harmonic minor scale
      selectedScale = harmonicMinorScale;
      break;
    case 3:  // Melodic minor scale
      selectedScale = melodicMinorScale;
      break;
    case 4:  // Natural minor scale
      selectedScale = naturalMinorScale;
      break;
    case 5:  // Pentatonic major scale
      selectedScale = pentatonicMajorScale;
      break;
    case 6:  // Pentatonic minor scale
      selectedScale = pentatonicMinorScale;
      break;
    case 7:  // Blues scale
      selectedScale = bluesScale;
      break;
    default:
      break;
  }
  setLayoutLEDs();
}

bool scaleLock = false;  // For enabling built-in buzzer for sound generation without a computer
GEMItem menuItemScaleLock("Scale Lock:", scaleLock, setLayoutLEDs);

int transpose = 0;
SelectOptionInt selectTransposeOptions[] = {
  { "-12", -12 }, { "-11", -11 }, { "-10", -10 }, { "-9", -9 }, { "-8", -8 }, { "-7", -7 }, { "-6", -6 }, { "-5", -5 }, { "-4", -4 }, { "-3", -3 }, { "-2", -2 }, { "-1", -1 }, { "0", 0 }, { "+1", 1 }, { "+2", 2 }, { "+3", 3 }, { "+4", 4 }, { "+5", 5 }, { "+6", 6 }, { "+7", 7 }, { "+8", 8 }, { "+9", 9 }, { "+10", 10 }, { "+11", 11 }, { "+12", 12 }
};
GEMSelect selectTranspose(sizeof(selectTransposeOptions) / sizeof(SelectOptionByte), selectTransposeOptions);
void validateTranspose();  // Forward declaration
GEMItem menuItemTranspose("Transpose:", transpose, selectTranspose, validateTranspose);

SelectOptionInt selectBendSpeedOptions[] = { { "too slo", 128 }, { "Turtle", 256 }, { "Slow", 512 }, { "Medium", 1024 }, { "Fast", 2048 }, { "Cheetah", 4096 }, { "Instant", 16384 } };
GEMSelect selectBendSpeed(sizeof(selectBendSpeedOptions) / sizeof(SelectOptionInt), selectBendSpeedOptions);
GEMItem menuItemBendSpeed("Pitch Bend:", pitchBendSpeed, selectBendSpeed);

SelectOptionByte selectModSpeedOptions[] = { { "too slo", 1 }, { "Turtle", 2 }, { "Slow", 3 }, { "Medium", 6 }, { "Fast", 12 }, { "Cheetah", 32 }, { "Instant", 127 } };
GEMSelect selectModSpeed(sizeof(selectModSpeedOptions) / sizeof(SelectOptionByte), selectModSpeedOptions);
GEMItem menuItemModSpeed("Mod Wheel:", modWheelSpeed, selectModSpeed);

void setBrightness();  //Forward declaration
SelectOptionByte selectBrightnessOptions[] = { { "Night", 10 }, { "Dim", 30 }, { "Low", 70 }, { "Medium", 110 }, { "High", 160 }, { "Highest", 210 }, { "MAX(!!)", 255 } };
GEMSelect selectBrightness(sizeof(selectBrightnessOptions) / sizeof(SelectOptionByte), selectBrightnessOptions);
GEMItem menuItemBrightness("Brightness:", stripBrightness, selectBrightness, setBrightness);

bool buzzer = false;  // For enabling built-in buzzer for sound generation without a computer
GEMItem menuItemBuzzer("Buzzer:", buzzer);

// For use when testing out unfinished features
GEMItem menuItemTesting("Testing", menuPageTesting);
boolean release = false;  // Whether this is a release or not
GEMItem menuItemVersion("V0.2.1 ", release, GEM_READONLY);
void sequencerSetup();  //Forward declaration
// For enabling basic sequencer mode - not complete
GEMItem menuItemSequencer("Sequencer:", sequencerMode, sequencerSetup);

// Create menu object of class GEM_u8g2. Supply its constructor with reference to u8g2 object we created earlier
byte menuItemHeight = 10;
byte menuPageScreenTopOffset = 10;
byte menuValuesLeftOffset = 78;
GEM_u8g2 menu(u8g2, GEM_POINTER_ROW, GEM_ITEMS_COUNT_AUTO, menuItemHeight, menuPageScreenTopOffset, menuValuesLeftOffset);


// MIDI channel assignment
byte midiChannel = 1;  // Current MIDI channel (changed via user input)

// Velocity levels
byte midiVelocity = 100;  // Default velocity

// END SETUP SECTION
// ------------------------------------------------------------------------------------------------------------------------------------------------------------

void setup() {
  lanes[0].instrument = 36;  // Bass Drum 1
  lanes[1].instrument = 40;  // Electric Snare
  lanes[2].instrument = 46;  // Open Hi-Hat
  lanes[3].instrument = 42;  // Closed Hi-Hat
  lanes[4].instrument = 49;  // Crash Cymbal 1
  lanes[5].instrument = 45;  // Low Tom
  lanes[6].instrument = 50;  // Hi Tom

#if defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040)
  // Manual begin() is required on core without built-in support for TinyUSB such as mbed rp2040
  TinyUSB_Device_Init(0);
#endif
  usb_midi.setStringDescriptor("HexBoard MIDI");
  // Initialize MIDI, and listen to all MIDI channels
  // This will also call usb_midi's begin()
  MIDI.begin(MIDI_CHANNEL_OMNI);
  // Callback will be run by MIDI.read()
  MIDI.setHandleNoteOn(handleNoteOn);
  //MIDI.setHandleTimeCodeQuarterFrame(handleTimeCodeQuarterFrame);

  Wire.setSDA(16);
  Wire.setSCL(17);

  pinMode(encoderClick, INPUT_PULLUP);

  Serial.begin(115200);  // Set serial to make uploads work without bootsel button

  LittleFSConfig cfg;       // Configure file system defaults
  cfg.setAutoFormat(true);  // Formats file system if it cannot be mounted.
  LittleFS.setConfig(cfg);
  LittleFS.begin();  // Mounts file system.
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
  }


  // Set pinModes for the digital button matrix.
  for (int pinNumber = 0; pinNumber < columnCount; pinNumber++)  // For each column pin...
  {
    pinMode(columns[pinNumber], INPUT_PULLUP);  // set the pinMode to INPUT_PULLUP (+3.3V / HIGH).
  }
  pinMode(m1p, OUTPUT);  // Setting the row multiplexer pins to output.
  pinMode(m2p, OUTPUT);
  pinMode(m4p, OUTPUT);
  pinMode(m8p, OUTPUT);

  strip.begin();                         // INITIALIZE NeoPixel strip object
  strip.show();                          // Turn OFF all pixels ASAP
  strip.setBrightness(stripBrightness);  // Set BRIGHTNESS (max = 255)
  setCMD_LEDs();
  strip.setPixelColor(cmdBtn1, strip.ColorHSV(65536 / 12, 255, pressedBrightness));
  selectedScale = chromaticScale;  // Set default scale
  setLayoutLEDs();

  u8g2.begin();               //Menu and graphics setup
  u8g2.setBusClock(1000000);  // Speed up display
  u8g2.setContrast(stripBrightness / 2);
  menu.setSplashDelay(0);
  menu.init();
  setupMenu();
  menu.drawMenu();

  // wait until device mounted, maybe
  for (int i = 0; i < 5 && !TinyUSBDevice.mounted(); i++) delay(1);

  // Print diagnostic troubleshooting information to serial monitor
  diagnosticTest();
}

void setup1() {  //Second core exclusively runs encoder
  //pinMode(ROTA, INPUT_PULLUP);
  //pinMode(ROTB, INPUT_PULLUP);
  //encoder_init();
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// START LOOP SECTION
void loop() {

  // Time tracking function
  timeTracker();

  // Reduces wear-and-tear on OLED panel
  screenSaver();

  // Read and store the digital button states of the scanning matrix
  readDigitalButtons();

  if (sequencerMode) {
    // Cause newpresses to change stuff
    sequencerToggleThingies();

    // If it's time to play notes, play them
    sequencerMaybePlayNotes();
  } else {
    // Act on those buttons
    playNotes();

    if (pitchModToggle) {
      // Pitch bend stuff
      pitchBend();
    } else {
      // mod wheel stuff
      modWheel();
    }

    // Held buttons
    heldButtons();
  }

  // Do the LEDS
  strip.show();

  // Read any new MIDI messages
  MIDI.read();

  // Read menu navigation functions
  menuNavigation();
}

void loop1() {
  rotate();  // Reads the encoder on the second core to avoid missed steps
  //readEncoder();
}
// END LOOP SECTION
// ------------------------------------------------------------------------------------------------------------------------------------------------------------


// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// START FUNCTIONS SECTION

void timeTracker() {
  loopTime = currentTime - previousTime;
  if (diagnostics == 4) {  //Print out the time it takes to run each loop
    Serial.println(loopTime);
  }
  // Update previouTime variable to give us a reference point for next loop
  previousTime = currentTime;
  // Store the current time in a uniform variable for this program loop
  currentTime = millis();
}

void diagnosticTest() {
  if (diagnostics > 0) {
    Serial.println("Zach was here");
  }
}

void commandPress(byte command) {
  if (command == CMDB_1) {
    midiVelocity = 100;
    setCMD_LEDs();
    strip.setPixelColor(cmdBtn1, strip.ColorHSV(65536 / 12, 255, pressedBrightness));
  }
  if (command == CMDB_2) {
    midiVelocity = 60;
    setCMD_LEDs();
    strip.setPixelColor(cmdBtn2, strip.ColorHSV(65536 / 3, 255, pressedBrightness));
  }
  if (command == CMDB_3) {
    midiVelocity = 20;
    setCMD_LEDs();
    strip.setPixelColor(cmdBtn3, strip.ColorHSV(65536 / 2, 255, pressedBrightness));
  }
  if (command == CMDB_4) {
    pitchModToggle = !pitchModToggle;  // Toggles between pitch bend and mod wheel
  }
  if (command == CMDB_5) {
  }
  if (command == CMDB_6) {
  }
  if (command == CMDB_7) {
  }
}
void commandRelease(byte command) {
}

void pitchBend() {  //todo: possibly add a check where if no notes are active, make the pitch bend instant.

  // Default: no pitch change
  int pitchBendTarget = 0;
  if (activeButtons[cmdBtn5] && !activeButtons[cmdBtn6] && !activeButtons[cmdBtn7]) {
    pitchBendTarget = 8191;  // Whole pitch up
  } else if (activeButtons[cmdBtn5] && activeButtons[cmdBtn6] && !activeButtons[cmdBtn7]) {
    pitchBendTarget = 4096;  // Half pitch up
  } else if (!activeButtons[cmdBtn5] && activeButtons[cmdBtn6] && activeButtons[cmdBtn7]) {
    pitchBendTarget = -4096;  // Half pitch down
  } else if (!activeButtons[cmdBtn5] && !activeButtons[cmdBtn6] && activeButtons[cmdBtn7]) {
    pitchBendTarget = -8192;  // Whole pitch down
  }

  // Approach the target, sendPitchBend based on timing
  if (pitchBendPosition != pitchBendTarget) {
    modPitchTime = modPitchTime + loopTime;
    if (modPitchTime >= 20) {  // Only run this loop every 20 ms to avoid overrunning MIDI
      modPitchTime = 0;        // Reset clock.
      // if distance between current value and target is less than the mod speed,
      if (abs(pitchBendPosition - pitchBendTarget) < pitchBendSpeed) {
        // don't go past the target.
        pitchBendPosition = pitchBendTarget;
      } else if (pitchBendPosition > pitchBendTarget) {
        // otherwise, subtract (or add) the speed to approach the target.
        pitchBendPosition = pitchBendPosition - pitchBendSpeed;
      } else if (pitchBendPosition < pitchBendTarget) {
        pitchBendPosition = pitchBendPosition + pitchBendSpeed;
      }
      // Always send the pitchBend because pitchBendPosition changed.
      MIDI.sendPitchBend(pitchBendPosition, midiChannel);
    }
  }

  if (pitchBendPosition == pitchBendTarget && modPitchTime != 200) {  // When the pitchbend hits the target...
    modPitchTime = 200;                                               // ...reset the clock for instant responsiveness upon button change
  }
  // Set mode indicator button red if in pitch bend mode
  strip.setPixelColor(cmdBtn4, strip.ColorHSV(0, 255, defaultBrightness));
  // Set the lights
  if (pitchBendPosition > 0) {
    strip.setPixelColor(cmdBtn5, strip.ColorHSV(0, 255, ((pitchBendPosition / 32) - 1)));
    strip.setPixelColor(cmdBtn6, strip.ColorHSV(0, 255, (-pitchBendPosition / 32)));
    strip.setPixelColor(cmdBtn7, strip.ColorHSV(0, 255, 0));
  }
  if (pitchBendPosition == 0) {
    strip.setPixelColor(cmdBtn5, strip.ColorHSV(0, 255, 0));
    strip.setPixelColor(cmdBtn6, strip.ColorHSV(0, 255, 255));
    strip.setPixelColor(cmdBtn7, strip.ColorHSV(0, 255, 0));
  }
  if (pitchBendPosition < 0) {
    strip.setPixelColor(cmdBtn5, strip.ColorHSV(0, 255, 0));
    strip.setPixelColor(cmdBtn6, strip.ColorHSV(0, 255, (pitchBendPosition / 32)));
    strip.setPixelColor(cmdBtn7, strip.ColorHSV(0, 255, ((-pitchBendPosition / 32) - 1)));
  }
}

void modWheel() {  ///IN THE MIDDLE OF HACKING SOMETHING TOGETHER - pardon the mess
  //BIG IDEA! Set target based on what keys are being pressed. Then use that target as a variable instead of the static numbers hard coded per target.
  byte modWheelTarget = 0;
  if (activeButtons[cmdBtn5] && !activeButtons[cmdBtn6] && !activeButtons[cmdBtn7]) {
    modWheelTarget = 127;
  } else if (activeButtons[cmdBtn5] && activeButtons[cmdBtn6] && !activeButtons[cmdBtn7]) {
    modWheelTarget = 100;
  } else if (!activeButtons[cmdBtn5] && activeButtons[cmdBtn6] && !activeButtons[cmdBtn7]) {
    modWheelTarget = 75;
  } else if (activeButtons[cmdBtn5] && activeButtons[cmdBtn7]) {
    modWheelTarget = 75;
  } else if (!activeButtons[cmdBtn5] && activeButtons[cmdBtn6] && activeButtons[cmdBtn7]) {
    modWheelTarget = 50;
  } else if (!activeButtons[cmdBtn5] && !activeButtons[cmdBtn6] && activeButtons[cmdBtn7]) {
    modWheelTarget = 25;
  } else if (!activeButtons[cmdBtn5] && !activeButtons[cmdBtn6] && !activeButtons[cmdBtn7]) {
    modWheelTarget = 0;
  }

  if (modWheelPosition != modWheelTarget) {  // Only runs the clock when mod target differs from actual
    modPitchTime = modPitchTime + loopTime;
    if (modPitchTime >= 20) {  // If it has been over 20 ms from last run,
      modPitchTime = 0;        // reset clock.
      // if distance between current value and target is less than the mod speed,
      if (abs(modWheelPosition - modWheelTarget) < modWheelSpeed) {
        // don't go past the target.
        modWheelPosition = modWheelTarget;
      } else if (modWheelPosition > modWheelTarget) {
        // otherwise, subtract (or add) the speed to approach the target.
        modWheelPosition = modWheelPosition - modWheelSpeed;
      } else if (modWheelPosition < modWheelTarget) {
        modWheelPosition = modWheelPosition + modWheelSpeed;
      }
      // Always send the control change because modWheelPosition changed.
      MIDI.sendControlChange(1, modWheelPosition, midiChannel);
    }
  }
  if (modWheelPosition == modWheelTarget && modPitchTime != 200) {
    modPitchTime = 200;  // Resets clock when modwheel hits target for instant responsiveness upon button change
  }
  // Set mode indicator button green if in mod wheel mode
  strip.setPixelColor(cmdBtn4, strip.ColorHSV(21854, 255, defaultBrightness));
  // Set the pixel colors based on the (new) modWheelPosition.
  if (modWheelPosition == 0) {
    strip.setPixelColor(cmdBtn5, strip.ColorHSV(21854, 255, 0));
    strip.setPixelColor(cmdBtn6, strip.ColorHSV(21854, 255, 0));
    strip.setPixelColor(cmdBtn7, strip.ColorHSV(21854, 255, 0));
  } else if (modWheelPosition > 0 && modWheelPosition < 25) {
    strip.setPixelColor(cmdBtn5, strip.ColorHSV(21854, 255, 0));
    strip.setPixelColor(cmdBtn6, strip.ColorHSV(21854, 255, 0));
    strip.setPixelColor(cmdBtn7, strip.ColorHSV(21854, 255, (modWheelPosition * 10)));
  } else if (modWheelPosition == 25) {
    strip.setPixelColor(cmdBtn5, strip.ColorHSV(21854, 255, 0));
    strip.setPixelColor(cmdBtn6, strip.ColorHSV(21854, 255, 0));
    strip.setPixelColor(cmdBtn7, strip.ColorHSV(21854, 255, 255));
  } else if (modWheelPosition > 25 && modWheelPosition < 75) {
    strip.setPixelColor(cmdBtn5, strip.ColorHSV(21854, 255, 0));
    strip.setPixelColor(cmdBtn6, strip.ColorHSV(21854, 255, ((modWheelPosition - 25) * 5)));
    strip.setPixelColor(cmdBtn7, strip.ColorHSV(21854, 255, 255));
  } else if (modWheelPosition == 75) {
    strip.setPixelColor(cmdBtn5, strip.ColorHSV(21854, 255, 0));
    strip.setPixelColor(cmdBtn6, strip.ColorHSV(21854, 255, 255));
    strip.setPixelColor(cmdBtn7, strip.ColorHSV(21854, 255, 255));
  } else if (modWheelPosition > 75 && modWheelPosition < 125) {
    strip.setPixelColor(cmdBtn5, strip.ColorHSV(21854, 255, ((modWheelPosition - 75) * 5)));
    strip.setPixelColor(cmdBtn6, strip.ColorHSV(21854, 255, 255));
    strip.setPixelColor(cmdBtn7, strip.ColorHSV(21854, 255, 255));
  } else if (modWheelPosition >= 125) {
    strip.setPixelColor(cmdBtn5, strip.ColorHSV(21854, 255, 255));
    strip.setPixelColor(cmdBtn6, strip.ColorHSV(21854, 255, 255));
    strip.setPixelColor(cmdBtn7, strip.ColorHSV(21854, 255, 255));
  }
}

// BUTTONS //
void readDigitalButtons() {
  if (diagnostics == 1) {
    Serial.println();
  }
  // Button Deck
  for (int rowIndex = 0; rowIndex < rowCount; rowIndex++)  // Iterate through each of the row pins on the multiplexing chip.
  {
    digitalWrite(m1p, rowIndex & 1);
    digitalWrite(m2p, (rowIndex & 2) >> 1);
    digitalWrite(m4p, (rowIndex & 4) >> 2);
    digitalWrite(m8p, (rowIndex & 8) >> 3);
    for (byte columnIndex = 0; columnIndex < columnCount; columnIndex++)  // Now iterate through each of the column pins that are connected to the current row pin.
    {
      byte columnPin = columns[columnIndex];                              // Hold the currently selected column pin in a variable.
      pinMode(columnPin, INPUT_PULLUP);                                   // Set that row pin to INPUT_PULLUP mode (+3.3V / HIGH).
      byte buttonNumber = columnIndex + (rowIndex * columnCount);         // Assign this location in the matrix a unique number.
      delayMicroseconds(10);                                              // Delay to give the pin modes time to change state (false readings are caused otherwise).
      previousActiveButtons[buttonNumber] = activeButtons[buttonNumber];  // Track the "previous" variable for comparison.
      byte buttonState = digitalRead(columnPin);                          // (don't)Invert reading due to INPUT_PULLUP, and store the currently selected pin state.
      if (buttonState == LOW) {
        if (diagnostics == 1) {
          Serial.print("1");
        } else if (diagnostics == 2) {
          Serial.println(buttonNumber);
        }
        if (!previousActiveButtons[buttonNumber]) {
          // newpress time
          activeButtonsTime[buttonNumber] = millis();
        }
        activeButtons[buttonNumber] = 1;
      } else {
        // Otherwise, the button is inactive, write a 0.
        if (diagnostics == 1) {
          Serial.print("0");
        }
        activeButtons[buttonNumber] = 0;
      }
      // Set the selected column pin back to INPUT mode (0V / LOW).
      pinMode(columnPin, INPUT);
    }
  }
}

// Function to check if a note is within the selected scale
bool isNotePlayable(byte note) {
  if (!scaleLock) {
    return true;  // Return true unconditionally if the toggle is disabled
  }
  for (int k = 0; k < 12; k++) {
    if (note == selectedScale[k]) {
      return true;
    }
  }
  return false;
}
// Used by things not affected by scaleLock
bool isNoteLit(byte note) {
  for (int k = 0; k < 12; k++) {
    if (note == selectedScale[k]) {
      return true;
    }
  }
  return false;
}

void playNotes() {
  for (int i = 0; i < elementCount; i++)  // For all buttons in the deck
  {
    if (activeButtons[i] != previousActiveButtons[i])  // If a change is detected
    {
      if (activeButtons[i] == 1)  // If the button is active (newpress)
      {
        if (currentLayout[i] < 128) {
          byte note = (currentLayout[i] - key + transpose) % 12;
          if (isNotePlayable(note)) {  // If note is within the selected scale, light up and play
            strip.setPixelColor(i, strip.ColorHSV(((currentLayout[i] - key + transpose) % 12) * 5006, 255, pressedBrightness));
            noteOn(midiChannel, (currentLayout[i] + transpose) % 128, midiVelocity);
          }
        } else {
          commandPress(currentLayout[i]);
        }
      } else {
        // If the button is inactive (released)
        if (currentLayout[i] < 128) {
          byte note = (currentLayout[i] - key + transpose) % 12;
          if (isNotePlayable(note)) {
            setLayoutLED(i);
            noteOff(midiChannel, (currentLayout[i] + transpose) % 128, 0);
          }
        } else {
          commandRelease(currentLayout[i]);
        }
      }
    }
  }
}

void heldButtons() {
  for (int i = 0; i < elementCount; i++) {
    if (activeButtons[i]) {
      //if (
    }
  }
}

void sequencerToggleThingies() {
  // For all buttons in the deck
  for (int i = 0; i < elementCount; i++) {
    // Some change was made
    if (activeButtons[i] != previousActiveButtons[i]) {
      // newpress
      if (activeButtons[i]) {
        int stripN = i / 20;
        if (stripN >= NLANES) continue;  // avoid an error.
        int step = map2step(i % 20);
        if (step >= 0) {
          int offset = lanes[stripN].bank * 16;
          lanes[stripN].steps[step + offset] = !lanes[stripN].steps[step + offset];
          int color = 0;
          if (lanes[stripN].steps[step + offset]) color = 255;
          strip.setPixelColor(i, color, color, color);
        } else if (step == -1) {  // switching banks
          lanes[stripN].bank = !lanes[stripN].bank;
          int offset = lanes[stripN].bank * 16;
          for (int j = 0; j < 16; j++) {
            int color = 0;
            if (lanes[stripN].steps[j + offset]) color = 255;
            strip.setPixelColor((stripN * 20) + step2map(j), color, color, color);
          }
        }
      }
    }
  }
}
// TODO: Redefine these for hexboard v2
int map2step(int i) {
  if (i >= 10) {
    return (19 - i) * 2;
  }
  if (i <= 8) {
    return ((8 - i) * 2) + 1;
  }
  return -1;
}
int step2map(int step) {
  if (step % 2) {
    return 8 - ((step - 1) / 2);
  }
  return 19 - (step / 2);
}

void sequencerMaybePlayNotes() {
  // TODO: sometimes call sequencerPlayNextNote();
}

// Do the next note, and increment the sequencer counter
void sequencerPlayNextNote() {
  bool anySolo = false;
  for (int i = 0; i < NLANES; i++) {
    // bitwise check if it's soloed
    if (lanes[i].state & STATE_SOLO) {
      anySolo = true;
    }
  }

  for (int i = 0; i < NLANES; i++) {
    // If something is soloed (not this one), then don't do the thing
    if (anySolo && !(lanes[i].state & STATE_SOLO)) {
      continue;
    }
    // If this one was muted, don't do the thing.
    if (lanes[i].state & STATE_MUTE) {
      continue;
    }
    int offset = lanes[i].bank * 16;
    if (lanes[i].steps[sequencerStep + offset]) {
      // do the thing.
      noteOn(midiChannel, lanes[i].instrument % 128, midiVelocity);
      // TODO: Change when the noteoff is played?
      noteOff(midiChannel, lanes[i].instrument % 128, 0);
    }
  }

  // increment and confine to limit
  sequencerStep++;
  sequencerStep %= 16;
}

// Return the first note that is currently held.
byte getHeldNote() {
  for (int i = 0; i < elementCount; i++) {
    if (activeButtons[i]) {
      if (currentLayout[i] < 128) {
        return (currentLayout[i] + transpose) % 128;
      }
    }
  }
  return 128;
}

// MIDI AND OTHER OUTPUTS //
// Send Note On
void noteOn(byte channel, byte pitch, byte velocity) {
  MIDI.sendNoteOn(pitch, velocity, channel);
  if (diagnostics == 3) {
    Serial.print(pitch);
    Serial.print(", ");
    Serial.print(velocity);
    Serial.print(", ");
    Serial.println(channel);
  }
  if (buzzer) {
    tone(TONEPIN, pitches[pitch]);
  }
}
// Send Note Off
void noteOff(byte channel, byte pitch, byte velocity) {
  MIDI.sendNoteOff(pitch, velocity, channel);
  noTone(TONEPIN);
  if (buzzer) {
    byte anotherPitch = getHeldNote();
    if (anotherPitch < 128) {
      tone(TONEPIN, pitches[anotherPitch]);
    } else {
    }
  }
}

// LEDS //
void setCMD_LEDs() {
  strip.setPixelColor(cmdBtn1, strip.ColorHSV(65536 / 12, 255, dimBrightness));
  strip.setPixelColor(cmdBtn2, strip.ColorHSV(65536 / 3, 255, dimBrightness));
  strip.setPixelColor(cmdBtn3, strip.ColorHSV(65536 / 2, 255, dimBrightness));
  strip.setPixelColor(cmdBtn4, strip.ColorHSV(0, 255, defaultBrightness));
}

void setLayoutLEDs() {
  for (int i = 0; i < elementCount; i++) {
    if (currentLayout[i] <= 127) {
      setLayoutLED(i);
    }
  }
}
void setLayoutLED(int i) {
  int note = (currentLayout[i] - key + transpose) % 12;
  if (scaleLock) {
    strip.setPixelColor(i, strip.ColorHSV(note * 5006, 255, 0));
  } else {
    strip.setPixelColor(i, strip.ColorHSV(note * 5006, 255, dimBrightness));
  }

  // Scale highlighting
  if (isNoteLit(note)) {
    strip.setPixelColor(i, strip.ColorHSV(note * 5006, 255, defaultBrightness));
  }
}

// ENCODER //
// rotary encoder pin change interrupt handler
void readEncoder() {
  encoder_state = (encoder_state << 4) | (digitalRead(ROTB) << 1) | digitalRead(ROTA);
  Serial.println(encoder_val);
  switch (encoder_state) {
    case 0x23: encoder_val++; break;
    case 0x32: encoder_val--; break;
    default: break;
  }
}
void rotate() {
  unsigned char result = rotary.process();
  if (result == DIR_CW) {
    encoder_val++;
  } else if (result == DIR_CCW) {
    encoder_val--;
  }
}
// rotary encoder init
void encoder_init() {
  // enable pin change interrupts
  attachInterrupt(digitalPinToInterrupt(ROTA), readEncoder, RISING);
  attachInterrupt(digitalPinToInterrupt(ROTB), readEncoder, RISING);
  encoder_state = (digitalRead(ROTB) << 1) | digitalRead(ROTA);
  interrupts();
}

// MENU //
void menuNavigation() {
  if (menu.readyForKey()) {
    encoderState = digitalRead(encoderClick);
    if (encoderState > encoderLastState) {
      menu.registerKeyPress(GEM_KEY_OK);
      screenTime = 0;
    }
    encoderLastState = encoderState;
    if (encoder_val < 0) {
      menu.registerKeyPress(GEM_KEY_UP);
      encoder_val = 0;
      screenTime = 0;
    }
    if (encoder_val > 0) {
      menu.registerKeyPress(GEM_KEY_DOWN);
      encoder_val = 0;
      screenTime = 0;
    }
  }
}

void setupMenu() {
  // Add menu items to Main menu page
  menuPageMain.addMenuItem(menuItemLayout);
  menuPageMain.addMenuItem(menuItemKey);
  menuPageMain.addMenuItem(menuItemScale);
  menuPageMain.addMenuItem(menuItemScaleLock);
  menuPageMain.addMenuItem(menuItemTranspose);
  menuPageMain.addMenuItem(menuItemBendSpeed);
  menuPageMain.addMenuItem(menuItemModSpeed);
  menuPageMain.addMenuItem(menuItemBrightness);
  menuPageMain.addMenuItem(menuItemBuzzer);
  menuPageMain.addMenuItem(menuItemTesting);
  // Add menu items to Layout Select page
  menuPageLayout.addMenuItem(menuItemWickiHayden);
  menuPageLayout.addMenuItem(menuItemHarmonicTable);
  menuPageLayout.addMenuItem(menuItemGerhard);
  // Add menu items to Testing page
  menuPageTesting.addMenuItem(menuItemSequencer);
  menuPageTesting.addMenuItem(menuItemVersion);
  // Specify parent menu page for the other menu pages
  menuPageLayout.setParentMenuPage(menuPageMain);
  menuPageTesting.setParentMenuPage(menuPageMain);
  // Add menu page to menu and set it as current
  menu.setMenuPageCurrent(menuPageMain);
}

void wickiHayden() {
  currentLayout = wickiHaydenLayout;
  setLayoutLEDs();
  if (ModelNumber != 1) {
    u8g2.setDisplayRotation(U8G2_R2);
  }
  menu.setMenuPageCurrent(menuPageMain);
  menu.drawMenu();
}
void harmonicTable() {
  currentLayout = harmonicTableLayout;
  setLayoutLEDs();
  if (ModelNumber != 1) {
    u8g2.setDisplayRotation(U8G2_R1);
  }
  menu.setMenuPageCurrent(menuPageMain);
  menu.drawMenu();
}
void gerhard() {
  currentLayout = gerhardLayout;
  setLayoutLEDs();
  if (ModelNumber != 1) {
    u8g2.setDisplayRotation(U8G2_R1);
  }
  menu.setMenuPageCurrent(menuPageMain);
  menu.drawMenu();
}

void setBrightness() {
  strip.setBrightness(stripBrightness);
  setLayoutLEDs();
}

// Validation routine of transpose variable
void validateTranspose() {
  //Need to add some code here to make sure transpose doesn't get out of hand
  /*something like
  if ((transpose + LOWEST NOTE IN ARRAY) < 0) {
    transpose = 0;
  } */
  setLayoutLEDs();
}

void screenSaver() {
  if (screenTime <= 10000) {
    screenTime = screenTime + loopTime;
    if (screenBrightness != stripBrightness / 2) {
      screenBrightness = stripBrightness / 2;
      u8g2.setContrast(screenBrightness);
    }
  }
  if (screenTime > 10000)
    if (screenBrightness != 1) {
      screenBrightness = 1;
      u8g2.setContrast(screenBrightness);
    }
}
void sequencerSetup() {
  if (sequencerMode) {
    strip.clear();
    strip.show();
  } else {
    setLayoutLEDs();
    setCMD_LEDs();
  }
}

// END FUNCTIONS SECTION
