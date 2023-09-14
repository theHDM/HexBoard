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
// Used this to get my hexagonal animations sorted. http://ondras.github.io/rot.js/manual/#hex/indexing

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
int stripBrightness = 130;
int defaultBrightness = 100;
int dimBrightness = 36;
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

// ./makeLayout.py 90 2 -7
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
// ./makeLayout.py 95 -7 3
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
// ./makeLayout.py 86 -1 -3
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
// This layout can't be created by makeLayout.py
const byte ezMajorLayout[elementCount] = { //Testing layout viability - probably will make this generative so we can easily add scales once figured out
  ROW_FLIP(CMDB_1, 91, 93, 95, 96, 98, 100, 101, 103, 105),
        ROW_FLIP(84, 86, 88, 89, 91, 93, 95, 96, 98, 100),
  ROW_FLIP(CMDB_2, 79, 81, 83, 84, 86, 88, 89, 91, 93),
        ROW_FLIP(72, 74, 76, 77, 79, 81, 83, 84, 86, 88),
  ROW_FLIP(CMDB_3, 67, 69, 71, 72, 74, 76, 77, 79, 81),
        ROW_FLIP(60, 62, 64, 65, 67, 69, 71, 72, 74, 76),
  ROW_FLIP(CMDB_4, 55, 57, 59, 60, 62, 64, 65, 67, 69),
        ROW_FLIP(48, 50, 52, 53, 55, 57, 59, 60, 62, 64),
  ROW_FLIP(CMDB_5, 43, 45, 47, 48, 50, 52, 53, 55, 57),
        ROW_FLIP(36, 38, 40, 41, 43, 45, 47, 48, 50, 52),
  ROW_FLIP(CMDB_6, 31, 33, 35, 36, 38, 40, 41, 43, 45),
        ROW_FLIP(24, 26, 28, 29, 31, 33, 35, 36, 38, 40),
  ROW_FLIP(CMDB_7, 19, 21, 23, 24, 26, 28, 29, 31, 33),
        ROW_FLIP(12, 14, 16, 17, 19, 21, 23, 24, 26, 28)
};
const byte* currentLayout = wickiHaydenLayout;

// These are for standard tuning only
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
const unsigned int pitches19[128] = {
// start close to C
264, 274, 284, 295, 306, 317, 329, 341, 354, 367, 380, 394, 409, 424,
// Octave starting at A=440
440, 456, 473, 491, 509, 528, 548, 568, 589, 611, 634, 657, 682, 707, 733, 761, 789, 818, 848,
880, 913, 947, 982, 1018, 1056, 1095, 1136, 1178, 1222, 1267, 1315, 1363, 1414, 1467, 1521, 1578, 1636, 1697,
1760, 1825, 1893, 1964, 2037, 2112, 2191, 2272, 2356, 2444, 2535, 2629, 2727, 2828, 2933, 3042, 3155, 3272, 3394,
3520, 3651, 3786, 3927, 4073, 4224, 4381, 4544, 4713, 4888, 5070, 5258, 5453, 5656, 5866, 6084, 6310, 6545, 6788,
7040, 7302, 7573, 7854, 8146, 8449, 8763, 9088, 9426, 9776, 10139, 10516, 10907, 11312, 11732, 12168, 12620, 13089, 13576,
14080, 14603, 15146, 15708, 16292, 16897, 17525, 18176, 18852, 19552, 20279, 21032, 21814, 22624, 23465, 24336, 25241, 26179, 27151
};
const unsigned int pitches24[128] = {
// start close to C
262, 269, 277, 285, 294, 302, 311, 320, 330, 339, 349, 359, 370, 381, 392, 403, 415, 427,
// Octave starting at A=440
440, 453, 466, 480, 494, 508, 523, 539, 554, 571, 587, 605, 622, 640, 659, 679, 698, 719, 740, 762, 784, 807, 831, 855,
880, 906, 932, 960, 988, 1017, 1047, 1077, 1109, 1141, 1175, 1209, 1245, 1281, 1319, 1357, 1397, 1438, 1480, 1523, 1568, 1614, 1661, 1710,
1760, 1812, 1865, 1919, 1976, 2033, 2093, 2154, 2217, 2282, 2349, 2418, 2489, 2562, 2637, 2714, 2794, 2876, 2960, 3047, 3136, 3228, 3322, 3420,
3520, 3623, 3729, 3839, 3951, 4067, 4186, 4309, 4435, 4565, 4699, 4836, 4978, 5124, 5274, 5429, 5588, 5751, 5920, 6093, 6272, 6456, 6645, 6840,
7040, 7246, 7459, 7677, 7902, 8134, 8372, 8617, 8870, 9130, 9397, 9673, 9956, 10248
};
// 41-TET 41 equal temperament
const unsigned int pitches41[128] = {
// Start close to C
261, 265, 269, 274, 279, 284, 288, 293, 298, 303, 309, 314, 319, 325, 330, 336, 341, 347, 353, 359, 365, 372, 378, 384, 391, 398, 404, 411, 418, 425, 433,
// Octave
440, 448, 455, 463, 471, 479, 487, 495, 504, 512, 521, 530, 539, 548, 557, 567, 577, 587, 597, 607, 617, 628, 638, 649, 660, 671, 683, 695, 706, 718, 731, 743, 756, 769, 782, 795, 809, 822, 836, 851, 865,
880, 895, 910, 926, 942, 958, 974, 991, 1007, 1025, 1042, 1060, 1078, 1096, 1115, 1134, 1153, 1173, 1193, 1213, 1234, 1255, 1276, 1298, 1320, 1343, 1366, 1389, 1413, 1437, 1461, 1486, 1512, 1537, 1564, 1590, 1617, 1645, 1673, 1701, 1730,
1760, 1790, 1821, 1852, 1883, 1915, 1948, 1981, 2015, 2049, 2084, 2120, 2156, 2193, 2230
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
byte animationStep[elementCount];               // Array to track reactive lighting steps
int animationTime = 0;                          // Used for tracking how long since last lighting update
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
void ezMajor();
GEMItem menuItemWickiHayden("Wicki-Hayden", wickiHayden);
GEMItem menuItemHarmonicTable("Harmonic Table", harmonicTable);
GEMItem menuItemGerhard("Gerhard", gerhard);
GEMItem menuItemEzMajor("EZ Major", ezMajor);

void setLayoutLEDs();  //Forward declaration
byte key = 0;
SelectOptionByte selectKeyOptions[] = { { "C", 0 }, { "C#", 1 }, { "D", 2 }, { "D#", 3 }, { "E", 4 }, { "F", 5 }, { "F#", 6 }, { "G", 7 }, { "G#", 8 }, { "A", 9 }, { "A#", 10 }, { "B", 11 } };
GEMSelect selectKey(sizeof(selectKeyOptions) / sizeof(SelectOptionByte), selectKeyOptions);
GEMItem menuItemKey("Key:", key, selectKey, setLayoutLEDs);

byte scale = 0;
SelectOptionByte selectScaleOptions[] = { { "ALL", 0 }, { "Major", 1 }, { "HarMin", 2 }, { "MelMin", 3 }, { "NatMin", 4 }, { "PentMaj", 5 }, { "PentMin", 6 }, { "Blues", 7 }, { "NONE", 8 }, { "NONE", 9 }, { "NONE", 10 }, { "NONE", 11 } };
GEMSelect selectScale(sizeof(selectScaleOptions) / sizeof(SelectOptionByte), selectScaleOptions);
GEMItem menuItemScale("Scale:", scale, selectScale, applySelectedScale);

const bool (*selectedScale)[12];
// Scale arrays of boolean (for O(1) access instead of O(12/2))
//                                   0 1 2 3 4 5 6 7 8 9 X E
const bool noneScale[12]          = {0,0,0,0,0,0,0,0,0,0,0,0};
const bool chromaticScale[12]     = {1,1,1,1,1,1,1,1,1,1,1,1};
const bool majorScale[12]         = {1,0,1,0,1,1,0,1,0,1,0,1};
const bool harmonicMinorScale[12] = {1,0,1,1,0,1,0,1,1,0,0,1};
const bool melodicMinorScale[12]  = {1,0,1,1,0,1,0,1,0,1,0,1};
const bool naturalMinorScale[12]  = {1,0,1,1,0,1,0,1,1,0,1,0};
const bool pentatonicMajorScale[12]={1,0,1,0,1,0,0,1,0,1,0,0};
const bool pentatonicMinorScale[12]={1,0,0,1,0,1,0,1,0,0,1,0};
const bool bluesScale[12]         = {1,0,0,1,0,1,1,1,0,0,1,0};

// Function to apply the selected scale
void applySelectedScale() {
  switch (scale) {
    case 0:  // All notes
      selectedScale = &chromaticScale;
      break;
    case 1:  // Major scale
      selectedScale = &majorScale;
      break;
    case 2:  // Harmonic minor scale
      selectedScale = &harmonicMinorScale;
      break;
    case 3:  // Melodic minor scale
      selectedScale = &melodicMinorScale;
      break;
    case 4:  // Natural minor scale
      selectedScale = &naturalMinorScale;
      break;
    case 5:  // Pentatonic major scale
      selectedScale = &pentatonicMajorScale;
      break;
    case 6:  // Pentatonic minor scale
      selectedScale = &pentatonicMinorScale;
      break;
    case 7:  // Blues scale
      selectedScale = &bluesScale;
      break;
    default: // Dim all LEDs
      selectedScale = &noneScale;
      break;
  }
  setLayoutLEDs();
}

bool scaleLock = false;  // For disabling all keys not in the selected scale
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
#if ModelNumber == 1
SelectOptionByte selectBrightnessOptions[] = { { "Night", 10 }, { "Dim", 30 }, { "Low", 70 }, { "Medium", 110 }, { "High", 160 }, { "Higher", 210 }, { "MAX(!!)", 255 } };
#elif ModelNumber == 2  // Reducing options to simplify and because the lights aren't as bright.
SelectOptionByte selectBrightnessOptions[] = { { "Dim", 20 }, { "Low", 70 }, { "Medium", 130 }, { "High", 190 }, { "Max", 255 } };
#endif
GEMSelect selectBrightness(sizeof(selectBrightnessOptions) / sizeof(SelectOptionByte), selectBrightnessOptions);
GEMItem menuItemBrightness("Brightness:", stripBrightness, selectBrightness, setBrightness);

byte lightMode = 0;
SelectOptionByte selectLightingOptions[] = { { "Button", 0 }, { "Note", 1 }, { "Octave", 2 }, { "Splash", 3 }, { "Star", 4 } };
GEMSelect selectLighting(sizeof(selectLightingOptions) / sizeof(SelectOptionByte), selectLightingOptions);
GEMItem menuItemLighting("Lighting:", lightMode, selectLighting);

bool buzzer = false;  // For enabling built-in buzzer for sound generation without a computer
GEMItem menuItemBuzzer("Buzzer:", buzzer);

// For use when testing out unfinished features
GEMItem menuItemTesting("Testing", menuPageTesting);
boolean release = false;  // Whether this is a release or not
GEMItem menuItemVersion("V0.4.0 ", release, GEM_READONLY);
void sequencerSetup();  //Forward declaration
// For enabling basic sequencer mode - not complete
GEMItem menuItemSequencer("Sequencer:", sequencerMode, sequencerSetup);

int tones = 12; // Experimental microtonal support
SelectOptionInt selectTonesOptions[] = {{"12", 12}, {"19", 19}, {"24", 24}, {"41", 41}};
GEMSelect selectTones(sizeof(selectTonesOptions)/sizeof(SelectOptionInt), selectTonesOptions);
GEMItem menuItemTones("Tones:", tones, selectTones);

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
  selectedScale = &chromaticScale;  // Set default scale
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

  // Animations
  reactiveLighting();

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

void pitchBend() {
  // Default: no pitch change
  int pitchBendTarget = 0;
  // Otherwise set the targetted value based on the buttons pressed.
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

void modWheel() {
  //Set targetted value based on what keys are being pressed.
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
  note = (note - key + transpose) % 12;
  if(tones != 12 || (*selectedScale)[note]) {
    return true;
  }
  return false;
}
// Used by things not affected by scaleLock
bool isNoteLit(byte note) {
  if(tones != 12 || (*selectedScale)[note%12]){
    return true;
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
          if (isNotePlayable(currentLayout[i])) {  // If note is within the selected scale, light up and play
            //strip.setPixelColor(i, strip.ColorHSV(keyColor(currentLayout[i]), 255, pressedBrightness));
            noteOn(midiChannel, (currentLayout[i] + transpose) % 128, midiVelocity);
          }
        } else {
          commandPress(currentLayout[i]);
        }
      } else {
        // If the button is inactive (released)
        if (currentLayout[i] < 128) {
          if (isNotePlayable(currentLayout[i])) {
            //setLayoutLED(i);
            noteOff(midiChannel, (currentLayout[i] + transpose) % 128, 0);
          }
        } else {
          commandRelease(currentLayout[i]);
        }
      }
    }
  }
}

void reactiveLighting() {
  animationTime = animationTime + loopTime;
  if (animationTime >= 33) {                                             // If it has been at least 33 ms (30fps) from last run,
    animationTime = 0;                                                   // reset clock.
    setLayoutLEDs();                                                     // Start by setting the lights to their defaults so we can "paint" on top of it.
    for (int i = 0; i < elementCount; i++) {                             // Scanning through the buttons
      if (isNotePlayable(currentLayout[i]) && currentLayout[i] < 128) {  // (if they are playable)
        switch (lightMode) {                                             // and implementing the selected lighting pattern
          case 0:
            buttonPattern(i);  // Lights up the button pressed.
            break;
          case 1:
            notePattern(i);  // Lights up the same exact notes as played across the array.
            break;
          case 2:
            octavePattern(i);  // Lights up the same notes as played in all octaves across the array.
            break;
          case 3:
            splashPattern(i);  // Creates an expanding ring around the pressed button.
            break;
          case 4:
            starPattern(i);  // Creates a starburst around the pressed button.
            break;
          default:  // Just in case something goes wrong?
            buttonPattern(i);
            break;
        }
      }
    }
  }
}

int keyColor(byte note) {
    return ((note - key + transpose) % tones) * ((5006*12)/tones);
}

void buttonPattern(int i) {
  if (activeButtons[i] == 1) {  // If it's an active button...
    // ...then we light it up!
    strip.setPixelColor(i, strip.ColorHSV(keyColor(currentLayout[i]), 240, pressedBrightness));
  }
}

void notePattern(int i) {
  if (activeButtons[i] == 1) {                       // Check to see if the it's an active button.
    for (int m = 0; m < elementCount; m++) {         // Scanning through all the lights
      if (currentLayout[m] < 128) {                  // Only runs on lights in the playable area
        if (currentLayout[m] == currentLayout[i]) {  // If it's the same note as the active button...
          // ...then we light it up!
          strip.setPixelColor(m, strip.ColorHSV(keyColor(currentLayout[m]), 240, pressedBrightness));
        }
      }
    }
  }
}

void octavePattern(int i) {
  if (activeButtons[i] == 1) {                                 // Check to see if the it's an active button.
    for (int m = 0; m < elementCount; m++) {                   // Scanning through all the lights
      if (currentLayout[m] < 128) {                            // Only runs on lights in the playable area
        if (currentLayout[m] % tones == currentLayout[i] % tones) {  // If it's in different octaves as the active button...
          // ...then we light it up!
          strip.setPixelColor(m, strip.ColorHSV(keyColor(currentLayout[m]), 240, pressedBrightness));
        }
      }
    }
  }
}

/* Kinda inefficient - adds 3ms to the loop timer for every 4 buttons held, but does not affect playability yet.
If performance becomes an issue, this may be better handled by a lookup table or something like that.
Another thing I'm considering is having an array for the lights that is populated as it runs the loops and only does setPixelColor at the
end. This would allow me to have the brightness fade and only add to the array if the light is brighter than what's currently there.*/
void splashPattern(int i) {
  int x1 = i % 10;  // Calculate the coordinates of the pressed button
  int y1 = i / 10;
  if (animationStep[i] > 0) {                 // Oh boy, animation time! Might be overcomplicating this...
    for (int m = 0; m < elementCount; m++) {  // Scanning through all the lights
      if (currentLayout[m] < 128) {           // Only runs on lights in the playable area
        int x2 = m % 10;                      // Coordinates of lights
        int y2 = m / 10;
        int dx = x1 - x2;  // Difference between light and button pressed
        int dy = y1 - y2;
// Penalty int shifts the lights over depending on if they are on odd or even rows to correct for the staggered rows
#if ModelNumber == 1
        int penalty = (((y1 % 2 == 0) && (y2 % 2 != 0) && (x1 > x2)) || ((y2 % 2 == 0) && (y1 % 2 != 0) && (x2 > x1))) ? 1 : 0;
#elif ModelNumber == 2
        int penalty = (((y1 % 2 == 0) && (y2 % 2 != 0) && (x1 < x2)) || ((y2 % 2 == 0) && (y1 % 2 != 0) && (x2 < x1))) ? 1 : 0;
#endif
        // If the light is the correct distance from the button...
        if (max(abs(dy), abs(dx) + floor(abs(dy) / 2) + penalty) == animationStep[i]) {
          // light it up!
          strip.setPixelColor(m, strip.ColorHSV(keyColor(currentLayout[m]), 240, pressedBrightness));
          // or we could have it fade as it moves
          //strip.setPixelColor(m, strip.ColorHSV(keyColor(currentLayout[m]), 240, (pressedBrightness - animationStep[i]*12)));
        }
      }
    }
  }
  if (activeButtons[i] == 1) {  // Check to see if the it's an active button.
    // Then we light up the pressed button
    strip.setPixelColor(i, strip.ColorHSV(keyColor(currentLayout[i]), 240, pressedBrightness));
    if (animationStep[i] < 16) {
      animationStep[i]++;  // Increment the animation to the next step for next time.
    }
  } else {
    animationStep[i] = 0;  // Stop the animation if the key is released
  }
}

void starPattern(int i) { // This one is far more efficient with no noticeable performance hit when playing lots of notes at once.
  int x1 = i % 10;  // Calculate the coordinates of the pressed button
  int y1 = i / 10;
  // Define the relative offsets of neighboring buttons in the pattern
#if ModelNumber == 1
  int offsets[][2] = {
    { 0, 1 },                        // Left
    { 0, -1 },                       // Right
    { -1, (y1 % 2 == 0) ? 0 : -1 },  // Top Left (adjusted based on row parity)
    { -1, (y1 % 2 == 0) ? 1 : 0 },   // Top Right (adjusted based on row parity)
    { 1, (y1 % 2 == 0) ? 1 : 0 },    // Bottom Right (adjusted based on row parity)
    { 1, (y1 % 2 == 0) ? 0 : -1 }    // Bottom Left (adjusted based on row parity)
  };
#elif ModelNumber == 2
  int offsets[][2] = {
    { 0, -1 },                       // Left
    { 0, 1 },                        // Right
    { -1, (y1 % 2 == 0) ? 0 : 1 },  // Top Left (adjusted based on row parity)
    { -1, (y1 % 2 == 0) ? -1 : 0 },   // Top Right (adjusted based on row parity)
    { 1, (y1 % 2 == 0) ? -1 : 0 },    // Bottom Right (adjusted based on row parity)
    { 1, (y1 % 2 == 0) ? 0 : 1 }    // Bottom Left (adjusted based on row parity)
  };
#endif


  if (animationStep[i] > 0) {  // Oh boy, animation time!
    for (const auto& offset : offsets) {
      // Calculate the neighboring button coordinates
      int y2 = y1 + offset[0] * animationStep[i];
#if ModelNumber == 1
      int x2 = x1 + offset[1] * animationStep[i] + ((y1 % 2 == 0) ? -1 : 1) * ((y1 == y2) ? 0 : 1) * (animationStep[i] / 2);
#elif ModelNumber == 2
      int x2 = x1 + offset[1] * animationStep[i] + ((y1 % 2 == 0) ? 1 : -1) * ((y1 == y2) ? 0 : 1) * (animationStep[i] / 2);
#endif
      // Check if the neighboring button is within the layout boundaries
      if (y2 >= 0 && y2 < 14 && x2 >= 0 && x2 < 10) {
        // Calculate the index of the neighboring button
        int neighborIndex = y2 * 10 + x2;
        if (currentLayout[neighborIndex] < 128) {  // If it's in the playable area...
          // ...set the color for the neighboring button
          strip.setPixelColor(neighborIndex, strip.ColorHSV(keyColor(currentLayout[neighborIndex]), 240, pressedBrightness));
        }
      }
    }
  }


  if (activeButtons[i] == 1) {  // Check to see if the it's an active button.
    // Then we light up the pressed button
    strip.setPixelColor(i, strip.ColorHSV(keyColor(currentLayout[i]), 240, pressedBrightness));
    if (animationStep[i] < 16) {
      animationStep[i]++;  // Increment the animation to the next step for next time.
    }
  } else {
    animationStep[i] = 0;  // Stop the animation if the key is released
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
      if (currentLayout[i] < 128 && isNotePlayable(currentLayout[i])) {
        return (currentLayout[i] + transpose) % 128;
      }
    }
  }
  return 128;
}

void do_tone(byte pitch) {
    if (tones == 12) {
        tone(TONEPIN, pitches[pitch]);
    } else if (tones == 19) {
        tone(TONEPIN, pitches19[pitch]);
    } else if (tones == 24) {
        tone(TONEPIN, pitches24[pitch]);
    } else if (tones == 41) {
        tone(TONEPIN, pitches41[pitch]);
    }
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
    do_tone(pitch);
  }
}
// Send Note Off
void noteOff(byte channel, byte pitch, byte velocity) {
  MIDI.sendNoteOff(pitch, velocity, channel);
  noTone(TONEPIN);
  if (buzzer) {
    byte anotherPitch = getHeldNote();
    if (anotherPitch < 128) {
      do_tone(anotherPitch);
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
  if (scaleLock) {
    strip.setPixelColor(i, strip.ColorHSV(keyColor(currentLayout[i]), 255, 0));
  } else {
    strip.setPixelColor(i, strip.ColorHSV(keyColor(currentLayout[i]), 255, dimBrightness));
  }

  // Scale highlighting
  if (isNoteLit(currentLayout[i])) {
    strip.setPixelColor(i, strip.ColorHSV(keyColor(currentLayout[i]), 255, defaultBrightness));
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
  menuPageMain.addMenuItem(menuItemLighting);
  menuPageMain.addMenuItem(menuItemBuzzer);
  menuPageMain.addMenuItem(menuItemTesting);
  // Add menu items to Layout Select page
  menuPageLayout.addMenuItem(menuItemWickiHayden);
  menuPageLayout.addMenuItem(menuItemHarmonicTable);
  menuPageLayout.addMenuItem(menuItemGerhard);
  // Add menu items to Testing page
  menuPageTesting.addMenuItem(menuItemSequencer);
  menuPageTesting.addMenuItem(menuItemEzMajor);
  menuPageTesting.addMenuItem(menuItemVersion);
  menuPageTesting.addMenuItem(menuItemTones);
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
void ezMajor() {
  currentLayout = ezMajorLayout;
  setLayoutLEDs();
  if (ModelNumber != 1) {
    u8g2.setDisplayRotation(U8G2_R2);
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
