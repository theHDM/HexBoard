// ====== Hexperiment v1.2
  // Copyright 2022-2023 Jared DeCook and Zach DeCook
  // Licensed under the GNU GPL Version 3.
  // Hardware Information:
  // Generic RP2040 running at 133MHz with 16MB of flash
  // https://github.com/earlephilhower/arduino-pico
  // (Additional boards manager URL: https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json)
  // Tools > USB Stack > (Adafruit TinyUSB)
  // Sketch > Export Compiled Binary
  //
  // Brilliant resource for dealing with hexagonal coordinates. https://www.redblobgames.com/grids/hexagons/
  // Used this to get my hexagonal animations sorted. http://ondras.github.io/rot.js/manual/#hex/indexing
  //
  // Menu library documentation https://github.com/Spirik/GEM
  //
  // Arduino IDE setup:
  // Board = Generic RP2040 (use the following additional board manager repo:
  // https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json)
  //
  // Patches needed for U8G2, Rotary.h
  // ==============================================================================
  
  #include <Arduino.h>
  #include <Adafruit_TinyUSB.h>
  #include "LittleFS.h"
  #include <MIDI.h>
  #include <Adafruit_NeoPixel.h>
  #define GEM_DISABLE_GLCD
  #include <GEM_u8g2.h>
  #include <Wire.h>
  #include <Rotary.h>
  #include "hardware/pwm.h"
  #include "hardware/timer.h"
  #include "hardware/irq.h"
  #include <queue>              // std::queue construct to store open channels in microtonal mode
  #include <string>
  
  // hardware pins
    #define SDAPIN 16
    #define SCLPIN 17

  // USB MIDI object //
    Adafruit_USBD_MIDI usb_midi;
    // Create a new instance of the Arduino MIDI Library,
    // and attach usb_midi as the transport.
    MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);
    #define CONCERT_A_HZ 440.0
    int16_t channelBend[16];   // what's the current note bend on this channel
    byte channelPoly[16];      // how many notes are playing on this channel
    std::queue<byte> openChannelQueue;
    #define PITCH_BEND_SEMIS 2

  // LED SETUP //
    #define LED_PIN 22
    #define LED_COUNT 140
    Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

  // ENCODER SETUP //
    #define ROT_PIN_A 20
    #define ROT_PIN_B 21
    #define ROT_PIN_C 24
    Rotary rotary = Rotary(ROT_PIN_A, ROT_PIN_B);
    bool rotaryIsClicked = HIGH;          //
    bool rotaryWasClicked = HIGH;         //
    int8_t rotaryKnobTurns = 0;           //
    byte maxKnobTurns = 10;

  // Create an instance of the U8g2 graphics library.
    U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);

  // Create menu object of class GEM_u8g2. Supply its constructor with reference to u8g2 object we created earlier
    #define MENU_ITEM_HEIGHT 10
    #define MENU_PAGE_SCREEN_TOP_OFFSET 10
    #define MENU_VALUES_LEFT_OFFSET 78
    GEM_u8g2 menu(
      u8g2, GEM_POINTER_ROW, GEM_ITEMS_COUNT_AUTO, 
      MENU_ITEM_HEIGHT, MENU_PAGE_SCREEN_TOP_OFFSET, MENU_VALUES_LEFT_OFFSET
    );
    const byte defaultContrast = 63;           // GFX default contrast
    bool screenSaverOn = 0;                    //
    uint32_t screenTime = 0;                   // GFX timer to count if screensaver should go on
    const uint32_t screenSaverMillis = 10'000; //

  // DIAGNOSTICS //
    // 1 = Full button test (1 and 0)
    // 2 = Button test (button number)
    // 3 = MIDI output test
    // 4 = Loop timing readout in milliseconds
    const byte diagnostics = 0;

  // Global time variables
    uint32_t runTime = 0;                // Program loop consistent variable for time in milliseconds since power on
    uint32_t lapTime = 0;                // Used to keep track of how long each loop takes. Useful for rate-limiting.
    uint32_t loopTime = 0;               // Used to check speed of the loop in diagnostics mode 4
    byte animationFPS = 32; // actually frames per 2^10 seconds. close enough to 30fps
    int16_t rainbowDegreeTime = 64; // ms to go through 1/360 of rainbow.

  // Button matrix and LED locations (PROD unit only)
    #define MPLEX_1_PIN 4
    #define MPLEX_2_PIN 5
    #define MPLEX_4_PIN 2
    #define MPLEX_8_PIN 3
    const byte mPin[] = { 
      MPLEX_1_PIN, MPLEX_2_PIN, MPLEX_4_PIN, MPLEX_8_PIN 
    };
    #define COLUMN_PIN_0 6
    #define COLUMN_PIN_1 7
    #define COLUMN_PIN_2 8
    #define COLUMN_PIN_3 9
    #define COLUMN_PIN_4 10
    #define COLUMN_PIN_5 11
    #define COLUMN_PIN_6 12
    #define COLUMN_PIN_7 13
    #define COLUMN_PIN_8 14
    #define COLUMN_PIN_9 15
    const byte cPin[] = { 
      COLUMN_PIN_0, COLUMN_PIN_1, COLUMN_PIN_2, COLUMN_PIN_3,
      COLUMN_PIN_4, COLUMN_PIN_5, COLUMN_PIN_6, 
      COLUMN_PIN_7, COLUMN_PIN_8, COLUMN_PIN_9 
    };
    #define COLCOUNT 10
    #define ROWCOUNT 14
  
    // Since MIDI only uses 7 bits, we can give greater values special meanings.
    // (see commandPress)
    // start CMDB in a range that won't interfere with layouts.
    #define CMDB 192
    #define UNUSED_NOTE 255

    // LED addresses for CMD buttons. (consequencely, also the button address too)
    #define CMDBTN_0 0
    #define CMDBTN_1 20
    #define CMDBTN_2 40
    #define CMDBTN_3 60
    #define CMDBTN_4 80
    #define CMDBTN_5 100
    #define CMDBTN_6 120
    const byte assignCmd[] = { 
      CMDBTN_0, CMDBTN_1, CMDBTN_2, CMDBTN_3, 
      CMDBTN_4, CMDBTN_5, CMDBTN_6
    };
    #define CMDCOUNT 7

  // MIDI note layout tables overhauled procedure since v1.1
  // FIRST, some introductory declarations
    typedef struct {  // defines the hex-grid coordinates using a double-offset system
      int8_t row;
      int8_t col;
    } coordinates;    // probably could have done this as a std::pair, but was too lazy
    
    enum { 
      Right, UpRight, UpLeft, Left, DnLeft, DnRight 
    };        // the six cardinal directions on the hex grid are 0 thru 5, counter-clockwise

    enum {
      Twelve, Seventeen, Nineteen, TwentyTwo, 
      TwentyFour, ThirtyOne, FortyOne, FiftyThree, 
      SeventyTwo, BohlenPierce, 
      CarlosA, CarlosB, CarlosG
    };       // this is supposed to help with code legibility, weird as it looks.
            // tuning # 0 is 12-EDO, so refer to that index 0 as "Twelve"
            // then the next tuning #1 is 17-EDO so refer to index 1 as Seventeen, etc.

  // SECOND, each button is an object, of type "buttonDef"
    typedef struct {
      byte keyState = 0;            // binary 00 = off, 01 = just pressed, 10 = just released, 11 = held
      coordinates coords = {0,0};   // the hexagonal coordinates
      uint32_t timePressed = 0;     // timecode of last press
      uint32_t LEDcolorAnim = 0;    // calculate it once and store value, to make LED playback snappier 
      uint32_t LEDcolorPlay = 0;    // calculate it once and store value, to make LED playback snappier
      uint32_t LEDcolorOn = 0;      // calculate it once and store value, to make LED playback snappier
      uint32_t LEDcolorOff = 0;     // calculate it once and store value, to make LED playback snappier
      bool animate = 0;             // hex is flagged as part of the animation in this frame, helps make animations smoother
      int16_t steps = 0;            // number of steps from key center (semitones in 12EDO; microtones if >12EDO)
      bool isCmd = 0;               // 0 if it's a MIDI note; 1 if it's a MIDI control cmd
      bool inScale = 0;             // 0 if it's not in the selected scale; 1 if it is
      byte note = UNUSED_NOTE;      // MIDI note or control parameter corresponding to this hex
      int16_t bend;                 // in microtonal mode, the pitch bend for this note needed to be tuned correctly
      byte channel;                 // what MIDI channel this note is playing on
      float frequency;              // what frequency to ring on the buzzer
      void updateKeyState(bool keyPressed) {
        keyState = (((keyState << 1) + keyPressed) & 3);
        if (keyState == 1) {
          timePressed = millis();   // log the time
        };
      };
      uint32_t animFrame() {     
        if (timePressed) {          // 2^10 milliseconds is close enough to 1 second
          return 1 + (((runTime - timePressed) * animationFPS) >> 10);
        } else {
          return 0;
        };
      };
    } buttonDef;                    // a better C++ programmer than me would turn this into some
                                    // fancy class definition in a header. i'm not that programmer!

    buttonDef h[LED_COUNT];         // a collection of all the buttons from 0 to 139
                                    // h[i] refers to the button with the LED address = i.

  // THIRD, each layout can be built on the fly. used to be done
    // separately in the ./makeLayout.py script, but not anymore.
    // with the introduction of microtonal tunings, note that each tuning
    // has its own list of layouts that are useful in that tuning.

    typedef struct {
      std::string name;
      bool isPortrait;
      byte rootHex;        // instead of "what note is button 1", "what button is the middle"
      int8_t acrossSteps;  // defined this way to be compatible with original v1.1 firmare
      int8_t dnLeftSteps;  // defined this way to be compatible with original v1.1 firmare
      byte tuning;
    } layoutDef;

    layoutDef layoutOptions[] = {
      { "Wicki-Hayden",      1, 64,   2,  -7, Twelve       },
      { "Harmonic Table",    0, 75,  -7,   3, Twelve       },
      { "Janko",             0, 65,  -1,  -1, Twelve       },
      { "Gerhard",           0, 65,  -1,  -3, Twelve       },
      { "Accordion C-sys.",  1, 75,   2,  -3, Twelve       },
      { "Accordion B-sys.",  1, 64,   1,  -3, Twelve       },
      { "Full Layout",       1, 65,  -1,  -9, Twelve       },
      { "Bosanquet, 17",     0, 65,  -2,  -1, Seventeen    },
      { "Full Layout",       1, 65,  -1,  -9, Seventeen    },
      { "Bosanquet, 19",     0, 65,  -1,  -2, Nineteen     },
      { "Full Layout",       1, 65,  -1,  -9, Nineteen     },
      { "Bosanquet, 22",     0, 65,  -3,  -1, TwentyTwo    },
      { "Full Layout",       1, 65,  -1,  -9, TwentyTwo    },
      { "Bosanquet, 24",     0, 65,  -1,  -3, TwentyFour   },
      { "Full Layout",       1, 65,  -1,  -9, TwentyFour   },
      { "Bosanquet, 31",     0, 65,  -2,  -3, ThirtyOne    },
      { "Full Layout",       1, 65,  -1,  -9, ThirtyOne    },
      { "Bosanquet, 41",     0, 65,  -4,  -3, FortyOne     },  // forty-one #1
      { "Gerhard, 41",       0, 65,   3, -10, FortyOne     },  // forty-one #2
      { "Full Layout, 41",   0, 65,  -1,  -8, FortyOne     },  // forty-one #3
      { "Wicki-Hayden, 53",  1, 64,   9, -31, FiftyThree   },
      { "Harmonic Tbl, 53",  0, 75, -31,  14, FiftyThree   },
      { "Bosanquet, 53",     0, 65,  -5,  -4, FiftyThree   },
      { "Full Layout, 53",   0, 65,  -1,  -9, FiftyThree   },
      { "Full Layout, 72",   0, 65,  -1,  -9, SeventyTwo   },
      { "Full Layout",       1, 65,  -1,  -9, BohlenPierce },
      { "Full Layout",       1, 65,  -1,  -9, CarlosA      },
      { "Full Layout",       1, 65,  -1,  -9, CarlosB      },
      { "Full Layout",       1, 65,  -1,  -9, CarlosG      }
    };
    const byte layoutCount = sizeof(layoutOptions) / sizeof(layoutDef);

  // FOURTH, since we updated routine for the piezo buzzer
    // we no longer rely on the Arduino tone() function.
    // instead we wrote our own pulse generator using the
    // system clock, and can pass precise frequencies
    // up to ~12kHz. the exact frequency of each button
    // depends on the tuning system, defined in the struct below.

    typedef struct {
      std::string name;
      byte cycleLength; // steps before repeat
      float stepSize;   // in cents, 100 = "normal" semitone.
    } tuningDef;

    tuningDef tuningOptions[] = {
      // replaces the idea of const byte EDO[] = { 12, 17, 19, 22, 24, 31, 41, 53, 72 };
      { "12 EDO",           12,  100.0 },
      { "17 EDO",           17, 1200.0 / 17 },
      { "19 EDO",           19, 1200.0 / 19 },
      { "22 EDO",           22, 1200.0 / 22 },
      { "24 EDO",           24,   50.0 },
      { "31 EDO",           31, 1200.0 / 31 },
      { "41 EDO",           41, 1200.0 / 41 },
      { "53 EDO",           53, 1200.0 / 53 },
      { "72 EDO",           72,  100.0 / 6 },
      { "Bohlen-Pierce",    13, 1901.955 / 13 }, //
      { "Carlos Alpha",      9, 77.965 }, //
      { "Carlos Beta",      11, 63.833 }, //
      { "Carlos Gamma",     20, 35.099 }
    };
    const byte tuningCount = sizeof(tuningOptions) / sizeof(tuningDef);

    #define TONEPIN 23
    #define TONE_SL 3
    #define TONE_CH 1
    #define WAVE_RESOLUTION 16
    #define ALARM_NUM 2
    #define ALARM_IRQ TIMER_IRQ_2

    typedef struct {
      std::string name;
      byte lvl[WAVE_RESOLUTION];
    } waveDef;
    waveDef wf[] = { // from [0..129]
      {"Square",  {0,0,0,0,0,0,0,0,129,129,129,129,129,129,129,129}},
      {"Saw",  {0,9,17,26,34,43,52,60,69,77,86,95,103,112,120,129}},
      {"3iangle",  {0,16,32,48,65,81,97,113,129,113,97,81,65,48,32,16}},
      {"Sine",  {0,5,19,40,65,89,110,124,129,124,110,89,65,40,19,5}}
    };
    byte wfTick = 0;
    byte wfLvl = 0;

  // Tone and Arpeggiator variables
    uint32_t microSecondsPerCycle = 1000000;
    uint32_t microSecondsPerTick = microSecondsPerCycle / WAVE_RESOLUTION;
    byte currentHexBuzzing = 255;         // if this is 255, buzzer set to off (0% duty cycle)
    uint32_t currentBuzzTime = 0;         // Used to keep track of when this note started buzzin
    uint32_t arpeggiateLength = 60;       //

  // Pitch bend and mod wheel variables overhauled to use an internal emulation structure as follows
    const uint16_t ccMsgCoolDown = 32; // milliseconds between steps
    typedef struct {
      byte* topBtn;
      byte* midBtn;
      byte* botBtn;
      int16_t minValue;
      int16_t maxValue;
      uint16_t stepValue;
      int16_t defValue;
      int16_t curValue;
      int16_t targetValue;
      uint32_t timeLastChanged;
      void setTargetValue() {
        if (*midBtn >> 1) { // middle button toggles target (0) vs. step (1) mode
          int16_t temp = curValue;
              if (*topBtn == 1)     {temp += stepValue;};
              if (*botBtn == 1)     {temp -= stepValue;};
              if (temp > maxValue)  {temp  = maxValue;} 
          else if (temp <= minValue) {temp  = minValue;};
          targetValue = temp;
        } else {
          switch (((*topBtn >> 1) << 1) + (*botBtn >> 1)) {
            case 0b10:   targetValue = maxValue;     break;
            case 0b11:   targetValue = defValue;     break;
            case 0b01:   targetValue = minValue;     break;
            default:     targetValue = curValue;     break;
          };
        };
      };
      bool updateValue() {
        int16_t temp = targetValue - curValue;
        if (temp != 0) {
          if ((runTime - timeLastChanged) >= ccMsgCoolDown) {
            timeLastChanged = runTime;
            if (abs(temp) < stepValue) {
              curValue = targetValue;
            } else {
              curValue = curValue + (stepValue * (temp / abs(temp)));
            };
            return 1;
          } else {
            return 0;
          };
        } else {
          return 0;
        };
      };   
    } wheelDef;
    wheelDef modWheel = {
      &h[assignCmd[4]].keyState,
      &h[assignCmd[5]].keyState,
      &h[assignCmd[6]].keyState,
      0, 127, 8,
      0, 0, 0
    }; 
    wheelDef pbWheel =  {
      &h[assignCmd[4]].keyState,
      &h[assignCmd[5]].keyState,
      &h[assignCmd[6]].keyState,
      -8192, 8192, 1024,
      0, 0, 0
    };
    wheelDef velWheel = {
      &h[assignCmd[0]].keyState,
      &h[assignCmd[1]].keyState,
      &h[assignCmd[2]].keyState,
      0, 127, 8,  
      96, 96, 96
    };
    bool toggleWheel = 0; // 0 for mod, 1 for pb

  /* Sequencer mode has not yet been restored

    // Variables for sequencer mode
    // Sequencer mode probably needs some love before it will be useful/usable.
    // The device is held vertically, and two rows create a "lane".
    // the first 8 buttons from each row are the steps (giving you 4 measures with quarter-note precision)
    // The extra 3 (4?) buttons are for bank switching, muting, and solo-ing
    typedef struct {
      // The first 16 are for bank 0, and the second 16 are for bank 1.
      bool steps[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
      bool bank = 0;
      int state = 0;  // TODO: change to enum: normal, mute, solo, mute&solo
      int instrument = 0; // What midi note this lane will send to the computer.
    } Lane;
    #define STATE_MUTE 1
    #define STATE_SOLO 2

    #define NLANES 7
    Lane lanes[NLANES];

    int sequencerStep = 0;  // 0 - 31

    // You have to push a button to switch modes
    bool sequencerMode = 0;

    // THESE CAN BE USED TO RESET THE SEQUENCE POSITION
    // void handleStart(void);
    // void handleContinue(void);
    // void handleStop(void);

    // THIS WILL BE USED FOR THE SEQUENCER CLOCK (24 frames per quarter note)
    // void handleTimeCodeQuarterFrame(byte data);
    // We should be able to adjust the division in the menu to have different sequence speeds.

    void handleNoteOn(byte channel, byte pitch, byte velocity) {
      // Rosegarden sends its metronome this way. Using for testing...
      if (1 == sequencerMode && 10 == channel && 100 == pitch) {
        sequencerPlayNextNote();
      }
    }

    */

  // ====== initialize list of supported scales / modes / raga / maqam

    typedef struct {
      std::string name;
      byte tuning;
      byte step[16]; // 16 bytes = 128 bits, 1 = in scale; 0 = not
    } scaleDef;
    scaleDef scaleOptions[] = {
      { "None",              255,        { 255,        255,         255,255,255,255,255,255,255,255,255,255,255,255,255,255} },
      { "Major",             Twelve,     { 0b10101101, 0b0101'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Minor, natural",    Twelve,     { 0b10110101, 0b1010'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Minor, melodic",    Twelve,     { 0b10110101, 0b0101'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Minor, harmonic",   Twelve,     { 0b10110101, 0b1001'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Pentatonic, major", Twelve,     { 0b10101001, 0b0100'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Pentatonic, minor", Twelve,     { 0b10010101, 0b0010'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Blues",             Twelve,     { 0b10010111, 0b0010'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Double Harmonic",   Twelve,     { 0b11001101, 0b1001'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Phrygian",          Twelve,     { 0b11010101, 0b1010'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Phrygian Dominant", Twelve,     { 0b11001101, 0b1010'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Dorian",            Twelve,     { 0b10110101, 0b0110'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Lydian",            Twelve,     { 0b10101011, 0b0101'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Lydian Dominant",   Twelve,     { 0b10101011, 0b0110'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Mixolydian",        Twelve,     { 0b10101101, 0b0110'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Locrian",           Twelve,     { 0b11010110, 0b1010'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Whole tone",        Twelve,     { 0b10101010, 0b1010'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Octatonic",         Twelve,     { 0b10110110, 0b1101'0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Rast maqam",        TwentyFour, { 0b10001001, 0b00100010, 0b00101100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { "Rast makam",        FiftyThree, { 0b10000000, 0b01000000, 0b01000010, 0b00000001,
                                                  0b00000000, 0b10001000, 0b10000'000, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    };
    const byte scaleCount = sizeof(scaleOptions) / sizeof(scaleDef);

  // ====== initialize key notation and coloring routines

    enum { DARK = 0, VeryDIM = 1, DIM = 32, BRIGHT = 127, VeryBRIGHT = 255 };
    enum { GRAY = 0, DULL = 127, VIVID = 255 };
    enum colors       { W,    R,    O,    Y,    L,     G,     C,     B,     I,     P,     M,
                              r,    o,    y,    l,     g,     c,     b,     i,     p,     m     };
    float hueCode[] = { 0.0,  0.0,  36.0, 72.0, 108.0, 144.0, 180.0, 216.0, 252.0, 288.0, 324.0,
                              0.0,  36.0, 72.0, 108.0, 144.0, 180.0, 216.0, 252.0, 288.0, 324.0  };
    byte satCode[] =  { GRAY, VIVID,VIVID,VIVID,VIVID, VIVID, VIVID, VIVID, VIVID, VIVID, VIVID, 
                              DULL, DULL, DULL, DULL,  DULL,  DULL,  DULL,  DULL,  DULL,  DULL  };

    typedef struct {
      std::string name;
      byte tuning;
      int8_t offset;      // steps from constant A4 to that key class 
      colors tierColor;
    } keyDef;
    keyDef keyOptions[] = {
      // 12 EDO, whole tone = 2, #/b = 1
        { " C        (B#)", Twelve, -9, W },
        { " C# / Db",       Twelve, -8, i },
        { "      D",        Twelve, -7, W },
        { "      D# / Eb",  Twelve, -6, i },
        { "     (Fb)  E",   Twelve, -5, W },
        { "      F   (E#)", Twelve, -4, c },
        { " Gb / F#",       Twelve, -3, I },
        { " G",             Twelve, -2, c },
        { " G# / Ab",       Twelve, -1, I },
        { "      A",        Twelve,  0, c },
        { "      A# / Bb",  Twelve,  1, I },
        { "(Cb)       B",   Twelve,  2, c },
      // 17 EDO, whole tone = 3, #/b = 2, +/d = 1
        { " C        (B+)", Seventeen,  -13, W },
        { " C+ / Db / B#",  Seventeen,  -12, R },
        { " C# / Dd",       Seventeen,  -11, I },
        { "      D",        Seventeen,  -10, W },
        { "      D+ / Eb",  Seventeen,   -9, R },
        { " Fb / D# / Ed",  Seventeen,   -8, I },
        { "(Fd)       E",   Seventeen,   -7, W },
        { " F        (E+)", Seventeen,   -6, W },
        { " F+ / Gb / E#",  Seventeen,   -5, R },
        { " F# / Gd",       Seventeen,   -4, I },
        { "      G",        Seventeen,   -3, W },
        { "      G+ / Ab",  Seventeen,   -2, R },
        { "      G# / Ad",  Seventeen,   -1, I },
        { "           A",   Seventeen,    0, W },
        { "      Bb / A+",  Seventeen,    1, R },
        { " Cb / Bd / A#",  Seventeen,    2, I },
        { "(Cd)  B"      ,  Seventeen,    3, W },
      // 19 EDO, whole tone = 3, #/b = 1
        { " C",       Nineteen, -14, W },
        { " C#",      Nineteen, -13, R },
        { " Db",      Nineteen, -12, I },
        { " D",       Nineteen, -11, W },
        { " D#",      Nineteen, -10, R },
        { " Eb",      Nineteen,  -9, I },
        { " E",       Nineteen,  -8, W },
        { " E# / Fb", Nineteen,  -7, m },
        { "      F",  Nineteen,  -6, W },
        { "      F#", Nineteen,  -5, R },
        { "      Gb", Nineteen,  -4, I },
        { "      G",  Nineteen,  -3, W },
        { "      G#", Nineteen,  -2, R },
        { "      Ab", Nineteen,  -1, I },
        { "      A",  Nineteen,   0, W },
        { "      A#", Nineteen,   1, R },
        { "      Bb", Nineteen,   2, I },
        { "      B",  Nineteen,   3, W },
        { " Cb / B#", Nineteen,   4, m },
      // 22 EDO, whole tone = 4, #/b = 3, ^/v = 1
        { "  C         (^B)", TwentyTwo, -17, W },
        { " ^C  /  Db / vB#", TwentyTwo, -16, l },
        { " vC# / ^Db /  B#", TwentyTwo, -15, C },
        { "  C# / vD",        TwentyTwo, -14, i },
        { "        D",        TwentyTwo, -13, W },
        { "       ^D  /  Eb", TwentyTwo, -12, l },
        { "  Fb / vD# / ^Eb", TwentyTwo, -11, C },
        { " ^Fb /  D# / vE",  TwentyTwo, -10, i },
        { "(vF)          E",  TwentyTwo,  -9, W },
        { "  F         (^E)", TwentyTwo,  -8, W },
        { " ^F  /  Gb / vE#", TwentyTwo,  -7, l },
        { " vF# / ^Gb /  E#", TwentyTwo,  -6, C },
        { "  F# / vG",        TwentyTwo,  -5, i },
        { "        G",        TwentyTwo,  -4, W },
        { "       ^G  /  Ab", TwentyTwo,  -3, l },
        { "       vG# / ^Ab", TwentyTwo,  -2, C },
        { "        G# / vA",  TwentyTwo,  -1, i },
        { "              A",  TwentyTwo,   0, W },
        { "        Bb / ^A",  TwentyTwo,   1, l },
        { "  Cb / ^Bb / vA#", TwentyTwo,   2, C },
        { " ^Cb / vB  /  A#", TwentyTwo,   3, i },
        { "(vC)    B",        TwentyTwo,   4, W },
      // 24 EDO, whole tone = 4, #/b = 2, +/d = 1
        { " C  / B#", TwentyFour, -18, W },
        { " C+",      TwentyFour, -17, r },
        { " C# / Db", TwentyFour, -16, I },
        { "      Dd", TwentyFour, -15, g },
        { "      D",  TwentyFour, -14, W },
        { "      D+", TwentyFour, -13, r },
        { " Eb / D#", TwentyFour, -12, I },
        { " Ed",      TwentyFour, -11, g },
        { " E  / Fb", TwentyFour, -10, W },
        { " E+ / Fd", TwentyFour,  -9, y },
        { " E# / F",  TwentyFour,  -8, W },
        { "      F+", TwentyFour,  -7, r },
        { " Gb / F#", TwentyFour,  -6, I },
        { " Gd",      TwentyFour,  -5, g },
        { " G",       TwentyFour,  -4, W },
        { " G+",      TwentyFour,  -3, r },
        { " G# / Ab", TwentyFour,  -2, I },
        { "      Ad", TwentyFour,  -1, g },
        { "      A",  TwentyFour,   0, W },
        { "      A+", TwentyFour,   1, r },
        { " Bb / A#", TwentyFour,   2, I },
        { " Bd",      TwentyFour,   3, g },
        { " B  / Cb", TwentyFour,   4, W },
        { " B+ / Cd", TwentyFour,   5, y },
      // 31 EDO, whole tone = 5, #/b = 2, +/d = 1
        { " C",       ThirtyOne, -23, W },
        { " C+",      ThirtyOne, -22, R },
        { " C#",      ThirtyOne, -21, Y },
        { " Db",      ThirtyOne, -20, C },
        { " Dd",      ThirtyOne, -19, I },
        { " D",       ThirtyOne, -18, W },
        { " D+",      ThirtyOne, -17, R },
        { " D#",      ThirtyOne, -16, Y },
        { " Eb",      ThirtyOne, -15, C },
        { " Ed",      ThirtyOne, -14, I },
        { " E",       ThirtyOne, -13, W },
        { " E+ / Fb", ThirtyOne, -12, L },
        { " E# / Fd", ThirtyOne, -11, M },
        { "      F",  ThirtyOne, -10, W },
        { "      F+", ThirtyOne,  -9, R },
        { "      F#", ThirtyOne,  -8, Y },
        { "      Gb", ThirtyOne,  -7, C },
        { "      Gd", ThirtyOne,  -6, I },
        { "      G",  ThirtyOne,  -5, W },
        { "      G+", ThirtyOne,  -4, R },
        { "      G#", ThirtyOne,  -3, Y },
        { "      Ab", ThirtyOne,  -2, C },
        { "      Ad", ThirtyOne,  -1, I },
        { "      A",  ThirtyOne,   0, W },
        { "      A+", ThirtyOne,   1, R },
        { "      A#", ThirtyOne,   2, Y },
        { "      Bb", ThirtyOne,   3, C },
        { "      Bd", ThirtyOne,   4, I },
        { "      B",  ThirtyOne,   5, W },
        { " Cb / B+", ThirtyOne,   6, L },
        { " Cd / B#", ThirtyOne,   7, M },
      // 41 EDO, whole tone = 7, #/b = 4, +/d = 2, ^/v = 1
        { "  C         (vB#)", FortyOne, -31, W },
        { " ^C        /  B#",  FortyOne, -30, c },
        { "  C+ ",             FortyOne, -29, O },
        { " vC# /  Db",        FortyOne, -28, I },
        { "  C# / ^Db",        FortyOne, -27, R },
        { "        Dd",        FortyOne, -26, B },
        { "       vD",         FortyOne, -25, y },
        { "        D",         FortyOne, -24, W },
        { "       ^D",         FortyOne, -23, c },
        { "        D+",        FortyOne, -22, O },
        { "       vD# /  Eb",  FortyOne, -21, I },
        { "        D# / ^Eb",  FortyOne, -20, R },
        { "              Ed",  FortyOne, -19, B },
        { "             vE",   FortyOne, -18, y },
        { "      (^Fb)   E",   FortyOne, -17, W },
        { "        Fd / ^E",   FortyOne, -16, c },
        { "       vF  /  E+",  FortyOne, -15, y },
        { "        F   (vE#)", FortyOne, -14, W },
        { "       ^F  /  E#",  FortyOne, -13, c },
        { "        F+",        FortyOne, -12, O },
        { "  Gb / vF#",        FortyOne, -11, I },
        { " ^Gb /  F#",        FortyOne, -10, R },
        { "  Gd",              FortyOne,  -9, B },
        { " vG",               FortyOne,  -8, y },
        { "  G",               FortyOne,  -7, W },
        { " ^G",               FortyOne,  -6, c },
        { "  G+",              FortyOne,  -5, O },
        { " vG# /  Ab",        FortyOne,  -4, I },
        { "  G# / ^Ab",        FortyOne,  -3, R },
        { "        Ad",        FortyOne,  -2, B },
        { "       vA",         FortyOne,  -1, y },
        { "        A",         FortyOne,   0, W },
        { "       ^A",         FortyOne,   1, c },
        { "        A+",        FortyOne,   2, O },
        { "       vA# /  Bb",  FortyOne,   3, I },
        { "        A# / ^Bb",  FortyOne,   4, R },
        { "              Bd",  FortyOne,   5, B },
        { "             vB",   FortyOne,   6, y },
        { "      (^Cb)   B",   FortyOne,   7, W },
        { "        Cd / ^B",   FortyOne,   8, c },
        { "       vC  /  B+",  FortyOne,   9, y },
      // 53 EDO, whole tone = 9, #/b = 5, >/< = 2, ^/v = 1
        { "  C         (vB#)", FiftyThree, -40, W },
        { " ^C     /     B#",  FiftyThree, -39, c },
        { " >C  / <Db",        FiftyThree, -38, l },
        { " <C# / vDb",        FiftyThree, -37, O },
        { " vC# /  Db",        FiftyThree, -36, I },
        { "  C# / ^Db",        FiftyThree, -35, R },
        { " ^C# / >Db",        FiftyThree, -34, B },
        { " >C# / <D",         FiftyThree, -33, g },
        { "       vD",         FiftyThree, -32, y },
        { "        D",         FiftyThree, -31, W },
        { "       ^D",         FiftyThree, -30, c },
        { "       >D  / <Eb",  FiftyThree, -29, l },
        { "       <D# / vEb",  FiftyThree, -28, O },
        { "       vD# /  Eb",  FiftyThree, -27, I },
        { "        D# / ^Eb",  FiftyThree, -26, R },
        { "       ^D# / >Eb",  FiftyThree, -25, B },
        { "       >D# / <E",   FiftyThree, -24, g },
        { "  Fb    /    vE",   FiftyThree, -23, y },
        { "(^Fb)         E",   FiftyThree, -22, W },
        { "(>Fb)        ^E",   FiftyThree, -21, c },
        { " <F     /    >E",   FiftyThree, -20, G },
        { " vF         (<E#)", FiftyThree, -19, y },
        { "  F         (vE#)", FiftyThree, -18, W },
        { " ^F     /     E#",  FiftyThree, -17, c },
        { " >F  / <Gb",        FiftyThree, -16, l },
        { " <F# / vGb",        FiftyThree, -15, O },
        { " vF# /  Gb",        FiftyThree, -14, I },
        { "  F# / ^Gb",        FiftyThree, -13, R },
        { " ^F# / >Gb",        FiftyThree, -12, B },
        { " >F# / <G",         FiftyThree, -11, g },
        { "       vG",         FiftyThree, -10, y },
        { "        G",         FiftyThree,  -9, W },
        { "       ^G",         FiftyThree,  -8, c },
        { "       >G  / <Ab",  FiftyThree,  -7, l },
        { "       <G# / vAb",  FiftyThree,  -6, O },
        { "       vG# /  Ab",  FiftyThree,  -5, I },
        { "        G# / ^Ab",  FiftyThree,  -4, R },
        { "       ^G# / >Ab",  FiftyThree,  -3, B },
        { "       >G# / <A",   FiftyThree,  -2, g },
        { "             vA",   FiftyThree,  -1, y },
        { "              A",   FiftyThree,   0, W },
        { "             ^A",   FiftyThree,   1, c },
        { "       <Bb / >A",   FiftyThree,   2, l },
        { "       vBb / <A#",  FiftyThree,   3, O },
        { "        Bb / vA#",  FiftyThree,   4, I },
        { "       ^Bb /  A#",  FiftyThree,   5, R },
        { "       >Bb / ^A#",  FiftyThree,   6, B },
        { "       <B  / >A#",  FiftyThree,   7, g },
        { "  Cb / vB",         FiftyThree,   8, y },
        { "(^Cb)   B",         FiftyThree,   9, W },
        { "(>Cb)  ^B",         FiftyThree,  10, c },
        { " <C  / >B",         FiftyThree,  11, G },
        { " vC   (<B#)",       FiftyThree,  12, y },
      // 72 EDO, whole tone = 12, #/b = 6, +/d = 3, ^/v = 1
        { "  C    (B#)", SeventyTwo, -54, W },
        { " ^C",         SeventyTwo, -53, g },
        { " vC+",        SeventyTwo, -52, r },
        { "  C+",        SeventyTwo, -51, p },
        { " ^C+",        SeventyTwo, -50, b },
        { " vC#",        SeventyTwo, -49, y },
        { "  C# /  Db",  SeventyTwo, -48, I },
        { " ^C# / ^Db",  SeventyTwo, -47, g },
        { "       vDd",  SeventyTwo, -46, r },
        { "        Dd",  SeventyTwo, -45, p },
        { "       ^Dd",  SeventyTwo, -44, b },
        { "       vD",   SeventyTwo, -43, y },
        { "        D",   SeventyTwo, -42, W },
        { "       ^D",   SeventyTwo, -41, g },
        { "       vD+",  SeventyTwo, -40, r },
        { "        D+",  SeventyTwo, -39, p },
        { "       ^D+",  SeventyTwo, -38, b },
        { " vEb / vD#",  SeventyTwo, -37, y },
        { "  Eb /  D#",  SeventyTwo, -36, I },
        { " ^Eb / ^D#",  SeventyTwo, -35, g },
        { " vEd",        SeventyTwo, -34, r },
        { "  Ed",        SeventyTwo, -33, p },
        { " ^Ed",        SeventyTwo, -32, b },
        { " vE   (vFb)", SeventyTwo, -31, y },
        { "  E    (Fb)", SeventyTwo, -30, W },
        { " ^E   (^Fb)", SeventyTwo, -29, g },
        { " vE+ / vFd",  SeventyTwo, -28, r },
        { "  E+ /  Fd",  SeventyTwo, -27, p },
        { " ^E+ / ^Fd",  SeventyTwo, -26, b },
        { "(vE#)  vF",   SeventyTwo, -25, y },
        { " (E#)   F",   SeventyTwo, -24, W },
        { "(^E#)  ^F",   SeventyTwo, -23, g },
        { "       vF+",  SeventyTwo, -22, r },
        { "        F+",  SeventyTwo, -21, p },
        { "       ^F+",  SeventyTwo, -20, b },
        { " vGb / vF#",  SeventyTwo, -19, y },
        { "  Gb /  F#",  SeventyTwo, -18, I },
        { " ^Gb / ^F#",  SeventyTwo, -17, g },
        { " vGd",        SeventyTwo, -16, r },
        { "  Gd",        SeventyTwo, -15, p },
        { " ^Gd",        SeventyTwo, -14, b },
        { " vG",         SeventyTwo, -13, y },
        { "  G",         SeventyTwo, -12, W },
        { " ^G",         SeventyTwo, -11, g },
        { " vG+",        SeventyTwo, -10, r },
        { "  G+",        SeventyTwo,  -9, p },
        { " ^G+",        SeventyTwo,  -8, b },
        { " vG# / vAb",  SeventyTwo,  -7, y },
        { "  G# /  Ab",  SeventyTwo,  -6, I },
        { " ^G# / ^Ab",  SeventyTwo,  -5, g },
        { "       vAd",  SeventyTwo,  -4, r },
        { "        Ad",  SeventyTwo,  -3, p },
        { "       ^Ad",  SeventyTwo,  -2, b },
        { "       vA",   SeventyTwo,  -1, y },
        { "        A",   SeventyTwo,   0, W },
        { "       ^A",   SeventyTwo,   1, g },
        { "       vA+",  SeventyTwo,   2, r },
        { "        A+",  SeventyTwo,   3, p },
        { "       ^A+",  SeventyTwo,   4, b },
        { " vBb / vA#",  SeventyTwo,   5, y },
        { "  Bb /  A#",  SeventyTwo,   6, I },
        { " ^Bb / ^A#",  SeventyTwo,   7, g },
        { " vBd",        SeventyTwo,   8, r },
        { "  Bd",        SeventyTwo,   9, p },
        { " ^Bd",        SeventyTwo,  10, b },
        { " vB   (vCb)", SeventyTwo,  11, y },
        { "  B    (Cb)", SeventyTwo,  12, W },
        { " ^B   (^Cb)", SeventyTwo,  13, g },
        { " vB+ / vCd",  SeventyTwo,  14, r },
        { "  B+ /  Cd",  SeventyTwo,  15, p },
        { " ^B+ / ^Cd",  SeventyTwo,  16, b },
        { "(vB#)  vC",   SeventyTwo,  17, y },
      //
        { "Note 0",BohlenPierce,0,W},
        { "Note 1",BohlenPierce,1,Y},
        { "Note 2",BohlenPierce,2,L},
        { "Note 3",BohlenPierce,3,G},
        { "Note 4",BohlenPierce,4,C},
        { "Note 5",BohlenPierce,5,B},
        { "Note 6",BohlenPierce,6,I},
        { "Note 7",BohlenPierce,7,P},
        { "Note 8",BohlenPierce,8,M},
        { "Note 9",BohlenPierce,9,R},
        { "Note 10",BohlenPierce,10,O},
        { "Note 11",BohlenPierce,11,g},
        { "Note 12",BohlenPierce,12,b},
      //
        { "Note 0",CarlosA,0,W},
        { "Note 1",CarlosA,1,Y},
        { "Note 2",CarlosA,2,G},
        { "Note 3",CarlosA,3,B},
        { "Note 4",CarlosA,4,P},
        { "Note 5",CarlosA,5,R},
        { "Note 6",CarlosA,6,c},
        { "Note 7",CarlosA,7,l},
        { "Note 8",CarlosA,8,m},
      //
        { "Note 0",CarlosB,0,W},
        { "Note 1",CarlosB,1,Y},
        { "Note 2",CarlosB,2,L},
        { "Note 3",CarlosB,3,G},
        { "Note 4",CarlosB,4,C},
        { "Note 5",CarlosB,5,B},
        { "Note 6",CarlosB,6,I},
        { "Note 7",CarlosB,7,P},
        { "Note 8",CarlosB,8,M},
        { "Note 9",CarlosB,9,R},
        { "Note 10",CarlosB,10,O},
      //
        { "Note 0",CarlosG,0,Y},
        { "Note 1",CarlosG,1,y},
        { "Note 2",CarlosG,2,L},
        { "Note 3",CarlosG,3,l},
        { "Note 4",CarlosG,4,G},
        { "Note 5",CarlosG,5,g},
        { "Note 6",CarlosG,6,C},
        { "Note 7",CarlosG,7,c},
        { "Note 8",CarlosG,8,B},
        { "Note 9",CarlosG,9,b},
        { "Note 10",CarlosG,10,I},
        { "Note 11",CarlosG,11,i},
        { "Note 12",CarlosG,12,P},
        { "Note 13",CarlosG,13,p},
        { "Note 14",CarlosG,14,M},
        { "Note 15",CarlosG,15,m},
        { "Note 16",CarlosG,16,R},
        { "Note 17",CarlosG,17,r},
        { "Note 18",CarlosG,18,O},
        { "Note 19",CarlosG,19,o},

    };
    const int keyCount = sizeof(keyOptions) / sizeof(keyDef);

  // MENU SYSTEM SETUP //
    // Create menu page object of class GEMPage. Menu page holds menu items (GEMItem) and represents menu level.
    // Menu can have multiple menu pages (linked to each other) with multiple menu items each

    GEMPage  menuPageMain("HexBoard MIDI Controller");
    GEMPage  menuPageTuning("Tuning");
    GEMPage  menuPageLayout("Layout");
    GEMPage  menuPageScales("Scales");
    GEMPage  menuPageKeys("Keys");

    GEMItem  menuGotoTuning("Tuning", menuPageTuning);
    GEMItem* menuItemTuning[tuningCount]; // dynamically generate item based on tunings

    GEMItem  menuGotoLayout("Layout", menuPageLayout); 
    GEMItem* menuItemLayout[layoutCount]; // dynamically generate item based on presets

    GEMItem  menuGotoScales("Scales", menuPageScales); 
    GEMItem* menuItemScales[scaleCount];  // dynamically generate item based on presets and if allowed in given EDO tuning

    GEMItem  menuGotoKeys("Keys",     menuPageKeys);
    GEMItem* menuItemKeys[keyCount];   // dynamically generate item based on presets

    byte scaleLock = 0;
    byte perceptual = 1;
    void resetHexLEDs();
    byte enableMIDI = 1;
    byte MPE = 0; // microtonal mode. if zero then attempt to self-manage multiple channels. 
                  // if one then on certain synths that are MPE compatible will send in that mode.
    void prepMIDIforMicrotones();
    SelectOptionByte optionByteYesOrNo[] =  { { "No"     , 0 },       
                                              { "Yes"    , 1 } };
    GEMSelect selectYesOrNo( sizeof(optionByteYesOrNo)  / sizeof(SelectOptionByte), optionByteYesOrNo);
    GEMItem  menuItemScaleLock( "Scale lock?",   scaleLock,     selectYesOrNo);
    GEMItem  menuItemPercep(    "Fix color:",    perceptual,    selectYesOrNo, resetHexLEDs);
    GEMItem  menuItemMIDI(      "Enable MIDI:",  enableMIDI,    selectYesOrNo);
    GEMItem  menuItemMPE(       "MPE Mode:",     MPE,           selectYesOrNo, prepMIDIforMicrotones);

    byte playbackMode = 2;
    SelectOptionByte optionBytePlayback[] = { { "Off"    , 0 },
                                              { "Mono"   , 1 }, 
                                              { "Arp'gio", 2 } };
    GEMSelect selectPlayback(sizeof(optionBytePlayback) / sizeof(SelectOptionByte), optionBytePlayback);
    GEMItem  menuItemPlayback(  "Buzzer:",       playbackMode,  selectPlayback);

    byte colorMode = 1;
    SelectOptionByte optionByteColor[] =    { { "Rainbow", 0 },
                                              { "Tiered" , 1 } };
    GEMSelect selectColor(   sizeof(optionByteColor)    / sizeof(SelectOptionByte), optionByteColor);
    GEMItem  menuItemColor(     "Color mode:",   colorMode,     selectColor,   resetHexLEDs);

    enum { NoAnim, StarAnim, SplashAnim, OrbitAnim, OctaveAnim, NoteAnim };
    byte animationType = NoAnim;
    SelectOptionByte optionByteAnimate[] =  { { "None"   , NoAnim }, 
                                              { "Octave" , OctaveAnim},   
                                              { "By Note", NoteAnim},   
                                              { "Star"   , StarAnim},   
                                              { "Splash" , SplashAnim},   
                                              { "Orbit"  , OrbitAnim} };
    GEMSelect selectAnimate( sizeof(optionByteAnimate)  / sizeof(SelectOptionByte), optionByteAnimate);
    GEMItem  menuItemAnimate(   "Animation:",    animationType, selectAnimate);

    byte currWave = 0;
    SelectOptionByte optionByteWaveform[] = { { wf[0].name.c_str(), 0 },
                                              { wf[1].name.c_str(), 1 },
                                              { wf[2].name.c_str(), 2 },
                                              { wf[3].name.c_str(), 3 }  };
    GEMSelect selectWaveform(sizeof(optionByteWaveform) / sizeof(SelectOptionByte), optionByteWaveform);
    GEMItem  menuItemWaveform(  "Waveform:",     currWave,      selectWaveform);

  // put all user-selectable options into a class so that down the line these can be saved and loaded.
    typedef struct { 
      std::string presetName; 
      int tuningIndex;     // instead of using pointers, i chose to store index value of each option, to be saved to a .pref or .ini or something
      int layoutIndex;
      int scaleIndex;
      int keyIndex;
      int transpose;
      // define simple recall functions
      tuningDef tuning() {
        return tuningOptions[tuningIndex];
      };
      layoutDef layout() {
        return layoutOptions[layoutIndex];
      };
      scaleDef scale() {
        return scaleOptions[scaleIndex];
      };
      keyDef key() {
        return keyOptions[keyIndex];
      };
      int layoutsBegin() {
        if (tuningIndex == Twelve) {
          return 0;
        } else {
          int temp = 0;
          while (layoutOptions[temp].tuning < tuningIndex) {
            temp++;
          };
          return temp;
        };
      };
      int keysBegin() {
        if (tuningIndex == Twelve) {
          return 0;
        } else {
          int temp = 0;
          while (keyOptions[temp].tuning < tuningIndex) {
            temp++;
          };
          return temp;
        };
      };
      int findC() {
        return keyOptions[keysBegin()].offset;
      };
    } presetDef;
    presetDef current = {
      "Default", 
      Twelve,     // see the relevant enum{} statement
      0,          // default to the first layout, wicki hayden
      0,          // default to using no scale (chromatic)
      0,          // default to the key of C
      0           // default to no transposition
    };

// ====== functions
  byte byteLerp(byte xOne, byte xTwo, float yOne, float yTwo, float y) {
    float weight = (y - yOne) / (yTwo - yOne);
    int temp = xOne + ((xTwo - xOne) * weight);
    if (temp < xOne) {temp = xOne;};
    if (temp > xTwo) {temp = xTwo;};
    return temp;
  }
  int positiveMod(int n, int d) {
    return (((n % d) + d) % d);
  }
  coordinates indexToCoord(byte x) {
    coordinates temp;
    temp.row = (x / 10);
    temp.col = (2 * (x % 10)) + (temp.row & 1);
    return temp;
  }
  bool hexOOB(coordinates c) {
    return (c.row < 0) 
        || (c.row >= ROWCOUNT)
        || (c.col < 0) 
        || (c.col >= (2 * COLCOUNT))
        || ((c.col + c.row) & 1);
  }
  byte coordToIndex(coordinates c) {
    if (hexOOB(c)) {
      return 255;
    } else {
      return (10 * c.row) + (c.col / 2);
    };
  }
  coordinates hexVector(byte direction, byte distance) {
    coordinates temp;
    int8_t vertical[]   = {0,-1,-1, 0, 1,1};
    int8_t horizontal[] = {2, 1,-1,-2,-1,1}; 
    temp.row = vertical[direction] * distance;
    temp.col = horizontal[direction] * distance;
    return temp;
  }
  coordinates hexOffset(coordinates a, coordinates b) {
    coordinates temp;
    temp.row = a.row + b.row;
    temp.col = a.col + b.col;
    return temp;
  }
  coordinates hexDistance(coordinates origin, coordinates destination) {
    coordinates temp;
    temp.row = destination.row - origin.row;
    temp.col = destination.col - origin.col;
    return temp;
  }
  float freqToMIDI(float Hz) {             // formula to convert from Hz to MIDI note
    return 69.0 + 12.0 * log2f(Hz / 440.0);
  }
  float MIDItoFreq(float MIDI) {           // formula to convert from MIDI note to Hz
    return 440.0 * exp2((MIDI - 69.0) / 12.0);
  }
  float stepsToMIDI(int16_t stepsFromA) {  // return the MIDI pitch associated
    return freqToMIDI(CONCERT_A_HZ) + ((float)stepsFromA * (float)current.tuning().stepSize / 100.0);
  }

// ====== diagnostic wrapper

  void sendToLog(std::string msg) {
    if (diagnostics) {
      Serial.println(msg.c_str());
    };
  }

// ====== LED routines

  int16_t transformHue(float D) {
    if ((!perceptual) || (D > 360.0)) {
      return 65536 * (D / 360.0);
    } else {
      //                red             yellow                 green               blue
      int hueIn[] =  {    0,    9,   18,   90,  108,  126,  135,  150,  198,  243,  252,  261,  306,  333,  360};
      //          #ff0000            #ffff00           #00ff00       #00ffff     #0000ff     #ff00ff
      int hueOut[] = {    0, 3640, 5461,10922,12743,16384,21845,27306,32768,38229,43690,49152,54613,58254,65535};
      byte B = 0;
      while (D - hueIn[B] > 0) {
        B++;
      };
      return hueOut[B - 1] + (hueOut[B] - hueOut[B - 1]) * ((D - (float)hueIn[B - 1])/(float)(hueIn[B] - hueIn[B - 1]));
    };
  }
  void resetHexLEDs() { // calculate color codes for each hex, store for playback
    int16_t hue;
    float hueDegrees;
    byte sat;
    colors c;
    for (byte i = 0; i < LED_COUNT; i++) {
      if (!(h[i].isCmd)) {
        byte scaleDegree = positiveMod(h[i].steps + current.key().offset - current.findC(),current.tuning().cycleLength);
        switch (colorMode) {
          case 1:
            c = keyOptions[current.keysBegin() + scaleDegree].tierColor;
            hueDegrees = hueCode[c];
            sat = satCode[c];
            break;
          default:
            hueDegrees = 360.0 * ((float)scaleDegree / (float)current.tuning().cycleLength);
            sat = 255;
            break;
        };
        hue = transformHue(hueDegrees);
        h[i].LEDcolorPlay = strip.gamma32(strip.ColorHSV(hue,sat,VeryBRIGHT));
        h[i].LEDcolorOn = strip.gamma32(strip.ColorHSV(hue,sat,BRIGHT));
        h[i].LEDcolorOff = strip.gamma32(strip.ColorHSV(hue,sat,DIM));
        h[i].LEDcolorAnim = strip.ColorHSV(hue,0,255);
      } else {
        //
      };
    };
  }

// ====== layout routines

  void assignPitches() {     // run this if the layout, key, or transposition changes, but not if color or scale changes
    sendToLog("assignPitch was called:");
    for (byte i = 0; i < LED_COUNT; i++) {
      if (!(h[i].isCmd)) {
        float N = stepsToMIDI(h[i].steps + current.key().offset + current.transpose);
        if (N < 0 || N >= 128) {
          h[i].note = 255;
          h[i].bend = 0;
          h[i].frequency = 0.0;
        } else {
          h[i].note = ((N >= 127) ? 127 : round(N));
          h[i].bend = (ldexp(N - h[i].note, 13) / PITCH_BEND_SEMIS);
          h[i].frequency = MIDItoFreq(N);
        };
        sendToLog(
          "hex #" + std::to_string(i) + ", " +
          "steps=" + std::to_string(h[i].steps) + ", " +
          "isCmd? " + std::to_string(h[i].isCmd) + ", " +
          "note=" + std::to_string(h[i].note) + ", " + 
          "bend=" + std::to_string(h[i].bend) + ", " + 
          "freq=" + std::to_string(h[i].frequency) + ", " + 
          "inScale? " + std::to_string(h[i].inScale) + "."
        );
      };
    };
    sendToLog("assignPitches complete.");
  }
  void applyScale() {
    sendToLog("applyScale was called:");
    for (byte i = 0; i < LED_COUNT; i++) {
      if (!(h[i].isCmd)) {
        byte degree = positiveMod(h[i].steps, current.tuning().cycleLength);
        byte whichByte = degree / 8;
        byte bitShift = 7 - (degree - (whichByte << 3));
        byte digitMask = 1 << bitShift;
        h[i].inScale = (current.scale().step[whichByte] & digitMask) >> bitShift;
        sendToLog(
          "hex #" + std::to_string(i) + ", " +
          "steps=" + std::to_string(h[i].steps) + ", " +
          "isCmd? " + std::to_string(h[i].isCmd) + ", " +
          "note=" + std::to_string(h[i].note) + ", " + 
          "inScale? " + std::to_string(h[i].inScale) + "."
        );
      };
    };
    resetHexLEDs();
    sendToLog("applyScale complete.");
  }
  void applyLayout() {       // call this function when the layout changes
    sendToLog("buildLayout was called:");
    for (byte i = 0; i < LED_COUNT; i++) {
      if (!(h[i].isCmd)) {
        coordinates dist = hexDistance(h[current.layout().rootHex].coords, h[i].coords);
        h[i].steps = (
          (dist.col * current.layout().acrossSteps) + 
          (dist.row * (
            current.layout().acrossSteps + 
            (2 * current.layout().dnLeftSteps)
          ))
        ) / 2;
        sendToLog(
          "hex #" + std::to_string(i) + ", " +
          "steps=" + std::to_string(h[i].steps) + "."
        );
      };
    };
    applyScale();        // when layout changes, have to re-apply scale and re-apply LEDs
    assignPitches();     // same with pitches
    u8g2.setDisplayRotation(current.layout().isPortrait ? U8G2_R2 : U8G2_R1);     // and landscape / portrait rotation
    sendToLog("buildLayout complete.");
  }
// ====== buzzer routines

  // the piezo buzzer is an on/off switch that can buzz as fast as the processor clock (133MHz)
  // the processor is fast enough to emulate analog signals.
  // the RP2040 has pulse width modulation (PWM) built into the hardware.
  // it can output a %-on / %-off pattern at any percentage desired.
  // at high enough frequencies, it sounds the same as an analog signal at that % volume.
  // to emulate an 8-bit (0-255) analog sample, with phase-correction, we need a 9 bit (512) cycle.
  // we can safely sample up to 260kHz (133MHz / 512) this way.
  // the highest frequency note in MIDI is about 12.5kHz.
  // it is therefore possible to emulate waveforms with 4 bits resolution (260kHz / 12.5kHz, it's > 16 but < 32).
  // 1) set a constant PWM signal at F_CPU/512 (260kHz) to play on pin 23
  //    the PWM signal can emulate an analog value from 0 to 255.
  //    this is done in setup1().
  // 2) if a note is to be played on the buzzer, get the frequency, and express as a period in microseconds.
  //    this is done in buzz().
  // 3) divide the period into 16 subperiods. fractions of microsecond are distributed across the 16 ticks.
  // 4) every subperiod, change the level of the PWM output so that you emulate the next in the sequence of
  //    16 analog sample values. those values are based on the waveform shape chosen (square, sine, etc)
  // 5) this value is also scaled by the MIDI velocity wheel.
  // 6) hardware timers are used because they will interrupt and run even if other code is active.
  //    otherwise, the subperiod is essentially floored at the length of the main loop() which is
  //    thousands of microseconds long!
  // the implementation of 6) is to make a single timer that calls back an interrupt function called advanceTick().
  // the callback function then resets the interrupt flag and resets the timer alarm.
  // the timer is set to go off at the time of the last timer, plus the subperiod (stored based on the last frequency played).
  // after the timer is reset, the function then changes the level of the PWM based on 4) and 5) above.
  // example:
  // to buzz note 69 (A=440Hz) at velocity 96:
  //   period = 2273 microseconds
  //   subperiod = 142 microseconds for 15 ticks, 143 microseconds for 1 tick
  //   for a square wave play 8 periods at zero level, 8 periods at 96 * 129 / 64 = 203 level

  void advanceTick() {
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
    wfTick = ((wfTick + 1) & (WAVE_RESOLUTION - 1));
    microSecondsPerTick = (microSecondsPerCycle / WAVE_RESOLUTION)
       + ((microSecondsPerCycle % WAVE_RESOLUTION) < wfTick)
      ;
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + microSecondsPerTick;
    wfLvl = ((playbackMode && (currentHexBuzzing <= LED_COUNT)) ? (wf[currWave].lvl[wfTick] * velWheel.curValue) >> 6 : 0);
    pwm_set_chan_level(TONE_SL, TONE_CH, wfLvl);
  }
  void buzz() {
    if (playbackMode && (currentHexBuzzing <= LED_COUNT)) {
      microSecondsPerCycle = round(1000000 / (exp2(pbWheel.curValue * PITCH_BEND_SEMIS / 98304.0) * h[currentHexBuzzing].frequency));
    };
  }
  byte nextHeldNote() {
    byte n = 255;
    for (byte i = 1; i < LED_COUNT; i++) {
      byte j = positiveMod(currentHexBuzzing + i, LED_COUNT);
      if ((h[j].channel) && (!h[j].isCmd)) {
        n = j;
        break;
      };
    };
    return n;
  }
  void tryBuzzing(byte x) {        
    currentHexBuzzing = x;
    if ((h[x].isCmd) || (h[x].note >= 128)) {
      currentHexBuzzing = 255;  // send 128 or larger to turn off tone
    };
    buzz();
  }
// ====== MIDI routines

  void setPitchBendRange(byte Ch, byte semitones) {
    if (enableMIDI) {     // MIDI mode only
      MIDI.beginRpn(0, Ch);
      MIDI.sendRpnValue(semitones << 7, Ch);
      MIDI.endRpn(Ch);
      sendToLog(
        "set pitch bend range on ch " +
        std::to_string(Ch) + " to be " + std::to_string(semitones) + " semitones"
      );
    }
  }
  void setMPEzone(byte masterCh, byte sizeOfZone) {
    if (enableMIDI) {     // MIDI mode only
      MIDI.beginRpn(6, masterCh);
      MIDI.sendRpnValue(sizeOfZone << 7, masterCh);
      MIDI.endRpn(masterCh);
      sendToLog(
        "tried sending MIDI msg to set MPE zone, master ch " +
        std::to_string(masterCh) + ", zone of this size: " + std::to_string(sizeOfZone)
      );
    }
  }
  void prepMIDIforMicrotones() {
    bool makeZone = (MPE && (current.tuningIndex != Twelve)); // if MPE flag is on and tuning <> 12EDO
    setMPEzone(1, (8 * makeZone));   // MPE zone 1 = ch 2 thru 9 (or reset if not using MPE)
    delay(ccMsgCoolDown);
    setMPEzone(16, (5 * makeZone));  // MPE zone 16 = ch 11 thru 15 (or reset if not using MPE)
    delay(ccMsgCoolDown);
    for (byte i = 1; i <= 16; i++) {
      setPitchBendRange(i, PITCH_BEND_SEMIS);  // some synths try to set PB range to 48 semitones.
      delay(ccMsgCoolDown);                  // this forces it back to the expected range of 2 semitones.
      if ((i != 10) && ((!makeZone) || ((i > 1) && (i < 16)))) {
        openChannelQueue.push(i);
        sendToLog("pushed ch " + std::to_string(i) + " to the open channel queue");
      };
      channelBend[i - 1] = 0;
      channelPoly[i - 1] = 0;
    };
  }
  void chgModulation() {
    if (enableMIDI) {     // MIDI mode only
      if (current.tuningIndex == Twelve) {
        MIDI.sendControlChange(1, modWheel.curValue, 1);
        sendToLog("sent mod value " + std::to_string(modWheel.curValue) + " to ch 1");
      } else if (MPE) {
        MIDI.sendControlChange(1, modWheel.curValue, 1);
        sendToLog("sent mod value " + std::to_string(modWheel.curValue) + " to ch 1");
        MIDI.sendControlChange(1, modWheel.curValue, 16);
        sendToLog("sent mod value " + std::to_string(modWheel.curValue) + " to ch 16");
      } else {
        for (byte i = 0; i < 16; i++) {
          MIDI.sendControlChange(1, modWheel.curValue, i + 1);
          sendToLog("sent mod value " + std::to_string(modWheel.curValue) + " to ch " + std::to_string(i+1));
        };
      };
    };
  };
  void chgUniversalPB() {
    if (enableMIDI) {     // MIDI mode only
      if (current.tuningIndex == Twelve) {
        MIDI.sendPitchBend(pbWheel.curValue, 1);
        sendToLog("sent pb value " + std::to_string(pbWheel.curValue) + " to ch 1");
      } else if (MPE) {
        MIDI.sendPitchBend(pbWheel.curValue, 1);
        sendToLog("sent pb value " + std::to_string(pbWheel.curValue) + " to ch 1");
        MIDI.sendPitchBend(pbWheel.curValue, 16);
        sendToLog("sent pb value " + std::to_string(pbWheel.curValue) + " to ch 16");
      } else {
        for (byte i = 0; i < 16; i++) {
          MIDI.sendPitchBend(channelBend[i] + pbWheel.curValue, i + 1);
          sendToLog("sent pb value " + std::to_string(channelBend[i] + pbWheel.curValue) + " to ch " + std::to_string(i+1));
        };
      };
    };
  }
  byte assignChannel(byte x) {
    if (current.tuningIndex == Twelve) {
      return 1;
    } else {
      byte temp = 17;
      for (byte c = MPE; c < (16 - MPE); c++) {  // MPE - look at ch 2 thru 15 [c 1-14]; otherwise ch 1 thru 16 [c 0-15]
        if ((c + 1 != 10) && (h[x].bend == channelBend[c])) {  // not using drum channel ch 10 in either case
          temp = c + 1;
          sendToLog("found a matching channel: ch " + std::to_string(temp) + " has pitch bend " + std::to_string(channelBend[c]));
          break;
        };
      };
      if (temp = 17) {
        if (openChannelQueue.empty()) {
          sendToLog("channel queue was empty so we didn't send a note on");
        } else {
          temp = openChannelQueue.front();
          openChannelQueue.pop();
          sendToLog("popped " + std::to_string(temp) + " off the queue");
        };
      };
      return temp;
    };
  }

// ====== hex press routines

  void noteOn(byte x) {
    byte c = assignChannel(x);
    if (c <= 16) {
      h[x].channel = c;       // value is 1 - 16
      if (current.tuningIndex != Twelve) {
        channelPoly[c - 1]++;   // array is 0 - 15
      };    
      if (playbackMode) {
        tryBuzzing(x);
      } else {
        if (current.tuningIndex != Twelve) {
          MIDI.sendPitchBend(h[x].bend, c); // ch 1-16
        };
        MIDI.sendNoteOn(h[x].note, velWheel.curValue, c); // ch 1-16
        sendToLog(
          "sent note on: " + std::to_string(h[x].note) +
          " pb " + std::to_string(h[x].bend) +
          " vel " + std::to_string(velWheel.curValue) +
          " ch " + std::to_string(c)
        );
      };
    };
  }
  void noteOff(byte x) {
    byte c = h[x].channel;
    if (c) {
      h[x].channel = 0;
      if (current.tuningIndex != Twelve) {
        switch (channelPoly[c - 1]) {
          case 1:
            channelPoly[c - 1]--;
            openChannelQueue.push(c);
            break;
          case 0:
            break;
          default:
            channelPoly[c - 1]--;
            break;
        };
      };
      if (playbackMode) {
        tryBuzzing(nextHeldNote());
      } else {
        MIDI.sendNoteOff(h[x].note, velWheel.curValue, c);
        sendToLog(
          "sent note off: " + std::to_string(h[x].note) +
          " pb " + std::to_string(h[x].bend) +
          " vel " + std::to_string(velWheel.curValue) +
          " ch " + std::to_string(c)
        );
      };
    };
  }
  void cmdOn(byte x) {   // volume and mod wheel read all current buttons
    switch (h[x].note) {
      case CMDB + 3:
        toggleWheel = !toggleWheel;
        // recolorHex(x);
        break;
      default:
        // the rest should all be taken care of within the wheelDef structure
        break;
    };
  }
  void cmdOff(byte x) {   // pitch bend wheel only if buttons held.
    // nothing; should all be taken care of within the wheelDef structure
  }

// ====== animations

  void flagToAnimate(coordinates C) {
    if (!hexOOB(C)) {
      h[coordToIndex(C)].animate = 1;
    };
  }
  void animateMirror() {
    for (byte i = 0; i < LED_COUNT; i++) {                   // check every hex
      if ((!(h[i].isCmd)) && (h[i].channel)) {              // that is a held note     
        for (byte j = 0; j < LED_COUNT; j++) {               // compare to every hex
          if ((!(h[j].isCmd)) && (!(h[j].channel))) {       // that is a note not being played
            int16_t temp = h[i].steps - h[j].steps;         // look at difference between notes
            if (animationType == OctaveAnim) {              // set octave diff to zero if need be
              temp = positiveMod(temp, current.tuning().cycleLength);
            };
            if (temp == 0) {                                // highlight if diff is zero
              h[j].animate = 1;
            };
          };
        };  
      };
    };
  }
  void animateOrbit() {
    for (byte i = 0; i < LED_COUNT; i++) {                               // check every hex
      if ((!(h[i].isCmd)) && (h[i].channel)) {                          // that is a held note     
        flagToAnimate(hexOffset(h[i].coords,hexVector((h[i].animFrame() % 6),1)));       // different neighbor each frame
      };
    };
  }
  void animateRadial() {
    for (byte i = 0; i < LED_COUNT; i++) {                               // check every hex
      if (!(h[i].isCmd)) {                                               // that is a note
        uint32_t radius = h[i].animFrame();
        if ((radius > 0) && (radius < 16)) {                              // played in the last 16 frames
          byte steps = ((animationType == SplashAnim) ? radius : 1);    // star = 1 step to next corner; ring = 1 step per hex
          coordinates temp = hexOffset(h[i].coords,hexVector(DnLeft,radius));  // start at one corner of the ring
          for (byte dir = 0; dir < 6; dir++) {                          // walk along the ring in each of the 6 hex directions
            for (byte i = 0; i < steps; i++) {                          // # of steps to the next corner 
              flagToAnimate(temp);                                       // flag for animation
              temp = hexOffset(temp, hexVector(dir,radius / steps));    // then next step
            };
          };
        };
      };      
    };    
  }

// ====== menu routines

  void menuHome() {
    menu.setMenuPageCurrent(menuPageMain);
    menu.drawMenu();
  }
  void showOnlyValidLayoutChoices() { // re-run at setup and whenever tuning changes
    for (byte L = 0; L < layoutCount; L++) {
      menuItemLayout[L]->hide((layoutOptions[L].tuning != current.tuningIndex));
    };
    sendToLog("menu: Layout choices were updated.");
  }
  void showOnlyValidScaleChoices() { // re-run at setup and whenever tuning changes
    for (int S = 0; S < scaleCount; S++) {
      menuItemScales[S]->hide((scaleOptions[S].tuning != current.tuningIndex) && (scaleOptions[S].tuning != 255));
    };
    sendToLog("menu: Scale choices were updated.");
  }
  void showOnlyValidKeyChoices() { // re-run at setup and whenever tuning changes
    for (int K = 0; K < keyCount; K++) {
      menuItemKeys[K]->hide((keyOptions[K].tuning != current.tuningIndex));
    };
    sendToLog("menu: Key choices were updated.");
  }
  void changeLayout(GEMCallbackData callbackData) {  // when you change the layout via the menu
    byte selection = callbackData.valByte;
    if (selection != current.layoutIndex) {
      current.layoutIndex = selection;
      applyLayout();
    };
    menuHome();
  }
  void changeScale(GEMCallbackData callbackData) {   // when you change the scale via the menu
    int selection = callbackData.valInt;
    if (selection != current.scaleIndex) {
      current.scaleIndex = selection;
      applyScale();
    };
    menuHome();
  }
  void changeKey(GEMCallbackData callbackData) {     // when you change the key via the menu
    int selection = callbackData.valInt;
    if (selection != current.keyIndex) {
      current.keyIndex = selection;
      assignPitches();
    };
    menuHome();
  }
  void changeTuning(GEMCallbackData callbackData) { // not working yet
    byte selection = callbackData.valByte;
    if (selection != current.tuningIndex) {
      current.tuningIndex = selection;
      current.layoutIndex = current.layoutsBegin();
      current.scaleIndex = 0;
      current.keyIndex = current.keysBegin();
      showOnlyValidLayoutChoices();
      showOnlyValidScaleChoices();
      showOnlyValidKeyChoices();
      applyLayout();
      prepMIDIforMicrotones();
    };
    menuHome();
  }
  void buildMenu() {
    menuPageMain.addMenuItem(menuGotoTuning);
    for (byte T = 0; T < tuningCount; T++) { // create pointers to all tuning choices
      menuItemTuning[T] = new GEMItem(tuningOptions[T].name.c_str(), changeTuning, T);
      menuPageTuning.addMenuItem(*menuItemTuning[T]);
    };

    menuPageMain.addMenuItem(menuGotoLayout);
    for (byte L = 0; L < layoutCount; L++) { // create pointers to all layouts
      menuItemLayout[L] = new GEMItem(layoutOptions[L].name.c_str(), changeLayout, L);
      menuPageLayout.addMenuItem(*menuItemLayout[L]);
    };
    showOnlyValidLayoutChoices();

    menuPageMain.addMenuItem(menuGotoScales);
    for (int S = 0; S < scaleCount; S++) {  // create pointers to all scale items, filter them as you go
      menuItemScales[S] = new GEMItem(scaleOptions[S].name.c_str(), changeScale, S);
      menuPageScales.addMenuItem(*menuItemScales[S]);
    };
    showOnlyValidScaleChoices();

    menuPageMain.addMenuItem(menuGotoKeys);
    for (int K = 0; K < keyCount; K++) {
      menuItemKeys[K] = new GEMItem(keyOptions[K].name.c_str(), changeKey, K);
      menuPageKeys.addMenuItem(*menuItemKeys[K]);
    };
    showOnlyValidKeyChoices();

    menuPageMain.addMenuItem(menuItemScaleLock);
    menuPageMain.addMenuItem(menuItemColor);
    menuPageMain.addMenuItem(menuItemMIDI);
    menuPageMain.addMenuItem(menuItemPlayback);  
    menuPageMain.addMenuItem(menuItemWaveform);
    menuPageMain.addMenuItem(menuItemAnimate);
    menuPageMain.addMenuItem(menuItemPercep);
    menuPageMain.addMenuItem(menuItemMPE);
  }

// ====== setup routines

  void setupMIDI() {
    usb_midi.setStringDescriptor("HexBoard MIDI");  // Initialize MIDI, and listen to all MIDI channels
    MIDI.begin(MIDI_CHANNEL_OMNI);                  // This will also call usb_midi's begin()
  }
  void setupFileSystem() {
    Serial.begin(115200);     // Set serial to make uploads work without bootsel button
    LittleFSConfig cfg;       // Configure file system defaults
    cfg.setAutoFormat(true);  // Formats file system if it cannot be mounted.
    LittleFS.setConfig(cfg);
    LittleFS.begin();  // Mounts file system.
    if (!LittleFS.begin()) {
      sendToLog("An Error has occurred while mounting LittleFS");
    }
  }
  void setupPins() {
    for (byte p = 0; p < sizeof(cPin); p++)  // For each column pin...
    {
      pinMode(cPin[p], INPUT_PULLUP);  // set the pinMode to INPUT_PULLUP (+3.3V / HIGH).
    }
    for (byte p = 0; p < sizeof(mPin); p++)  // For each column pin...
    {
      pinMode(mPin[p], OUTPUT);  // Setting the row multiplexer pins to output.
    }
    Wire.setSDA(SDAPIN);
    Wire.setSCL(SCLPIN);
    pinMode(ROT_PIN_C, INPUT_PULLUP);
  }
  void setupGrid() {
    /*
    sendToLog("initializing hex grid..."));
    */
    for (byte i = 0; i < LED_COUNT; i++) {
      h[i].coords = indexToCoord(i);
      h[i].isCmd = 0;
      h[i].note = 255;
      h[i].keyState = 0;
    };
    for (byte c = 0; c < CMDCOUNT; c++) {
      h[assignCmd[c]].isCmd = 1;
      h[assignCmd[c]].note = CMDB + c;
    };
    applyLayout();
  }
  void setupLEDs() {  // need layout
    strip.begin();    // INITIALIZE NeoPixel strip object
    strip.show();     // Turn OFF all pixels ASAP
    resetHexLEDs();
  }
  void setupMenu() {  // need menu
    menu.setSplashDelay(0);
    menu.init();
    buildMenu();
    menuHome();
  }
  void setupGFX() {
    u8g2.begin();                       // Menu and graphics setup
    u8g2.setBusClock(1000000);          // Speed up display
    u8g2.setContrast(defaultContrast);  // Set contrast
  }
  void testDiagnostics() {
    /*
    sendToLog("theHDM was here"));
    */
  }
  void setupPiezo() {
    gpio_set_function(TONEPIN, GPIO_FUNC_PWM);
    pwm_set_phase_correct(TONE_SL, true);
    pwm_set_wrap(TONE_SL, 254);
    pwm_set_clkdiv(TONE_SL, 1.0f);
    pwm_set_chan_level(TONE_SL, TONE_CH, 0);
    pwm_set_enabled(TONE_SL, true);
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);  // initialize the timer
    irq_set_exclusive_handler(ALARM_IRQ, advanceTick);  // function to run every interrupt
    irq_set_enabled(ALARM_IRQ, true);
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + microSecondsPerTick;
  }

// ====== loop routines

  void timeTracker() {
    lapTime = runTime - loopTime;
    // sendToLog(lapTime));  // Print out the time it takes to run each loop
    loopTime = runTime;  // Update previousTime variable to give us a reference point for next loop
    runTime = millis();   // Store the current time in a uniform variable for this program loop
  }
  void screenSaver() {
    if (screenTime <= screenSaverMillis) {
      screenTime = screenTime + lapTime;
      if (screenSaverOn) {
        screenSaverOn = 0;
        u8g2.setContrast(defaultContrast);
      }
    } else {
      if (!screenSaverOn) {
        screenSaverOn = 1;
        u8g2.setContrast(1);
      }
    }
  }
  void readHexes() {
    for (byte r = 0; r < ROWCOUNT; r++) {  // Iterate through each of the row pins on the multiplexing chip.
      for (byte d = 0; d < 4; d++) {
        digitalWrite(mPin[d], (r >> d) & 1);
      }
      for (byte c = 0; c < COLCOUNT; c++) {   // Now iterate through each of the column pins that are connected to the current row pin.
        byte p = cPin[c];               // Hold the currently selected column pin in a variable.
        pinMode(p, INPUT_PULLUP);             // Set that row pin to INPUT_PULLUP mode (+3.3V / HIGH).
        delayMicroseconds(10);                // Delay to give the pin modes time to change state (false readings are caused otherwise).
        bool didYouPressHex = (digitalRead(p) == LOW);  // hex is pressed if it returns LOW. else not pressed
        h[c + (r * COLCOUNT)].updateKeyState(didYouPressHex);
        pinMode(p, INPUT);  // Set the selected column pin back to INPUT mode (0V / LOW).
       }
    }
  }
  void actionHexes() { 
    for (byte i = 0; i < LED_COUNT; i++) {   // For all buttons in the deck
      switch (h[i].keyState) {
        case 1: // just pressed
          if (h[i].isCmd) {
            cmdOn(i);
          } else if (h[i].inScale || (!scaleLock)) {
            noteOn(i);
          };
          break;
        case 2: // just released
          if (h[i].isCmd) {
            cmdOff(i);
          } else if (h[i].inScale || (!scaleLock)) {
            noteOff(i);
          };
          break;
        case 3: // held
          break;
        default: // inactive
          break;
      };
    };
  }
  void arpeggiate() {
    if (playbackMode > 1) {
      if (runTime - currentBuzzTime > arpeggiateLength) {
        currentBuzzTime = millis();
        byte n = nextHeldNote();
        if (n != 255) {
          tryBuzzing(nextHeldNote());
        };
      };
    };
  }
  void updateWheels() {  
    velWheel.setTargetValue();
    bool upd = velWheel.updateValue();
    if (upd) {
      buzz(); // update the volume live
      sendToLog("vel became " + std::to_string(velWheel.curValue));
    }
    if (toggleWheel) {
      pbWheel.setTargetValue();
      upd = pbWheel.updateValue();
      if (upd) {
        buzz();
        chgUniversalPB();
      };
    } else {
      modWheel.setTargetValue();
      upd = modWheel.updateValue();
      if (upd) {
        chgModulation();
      };
    };
  }
  void animateLEDs() {    // TBD  
    for (byte i = 0; i < LED_COUNT; i++) {      
      h[i].animate = 0;
    };
    if (animationType) {
      switch (animationType) { 
        case StarAnim: case SplashAnim:
          animateRadial();
          break;
        case OrbitAnim:
          animateOrbit();
          break;
        case OctaveAnim: case NoteAnim:
          animateMirror();
          break;  
        default:
          break;
      };
    };
  }
  void lightUpLEDs() {   
    for (byte i = 0; i < LED_COUNT; i++) {      
      if (!(h[i].isCmd)) {
        if (h[i].animate) {
          strip.setPixelColor(i,h[i].LEDcolorAnim);
        } else if (h[i].channel) {
          strip.setPixelColor(i,h[i].LEDcolorPlay);
        } else if (h[i].inScale) {
          strip.setPixelColor(i,h[i].LEDcolorOn);
        } else {
          strip.setPixelColor(i,h[i].LEDcolorOff);
        };
      };
    };
    int16_t hueV = transformHue((runTime / rainbowDegreeTime) % 360);
    strip.setPixelColor(assignCmd[0],strip.gamma32(strip.ColorHSV(
      hueV,192,byteLerp(0,255,85,127,velWheel.curValue)
    )));
    strip.setPixelColor(assignCmd[1],strip.gamma32(strip.ColorHSV(
      hueV,192,byteLerp(0,255,42,85,velWheel.curValue)
    )));
    strip.setPixelColor(assignCmd[2],strip.gamma32(strip.ColorHSV(
      hueV,192,byteLerp(0,255,0,42,velWheel.curValue)
    )));
    if (toggleWheel) {
      // pb red / green
      int16_t hueP = transformHue((pbWheel.curValue > 0) ? 0 : 180);
      byte satP = byteLerp(0,255,0,8192,abs(pbWheel.curValue));
      strip.setPixelColor(assignCmd[3],strip.gamma32(strip.ColorHSV(
        0,0,64
      )));
      strip.setPixelColor(assignCmd[4],strip.gamma32(strip.ColorHSV(
        transformHue(0),satP * (pbWheel.curValue > 0),satP * (pbWheel.curValue > 0)
      )));
      strip.setPixelColor(assignCmd[5],strip.gamma32(strip.ColorHSV(
        hueP,satP,255
      )));
      strip.setPixelColor(assignCmd[6],strip.gamma32(strip.ColorHSV(
        transformHue(180),satP * (pbWheel.curValue < 0),satP * (pbWheel.curValue < 0)
      )));
    } else {
      // mod blue / yellow
      int16_t hueM = transformHue((modWheel.curValue > 63) ? 90 : 270);
      byte satM = byteLerp(0,255,0,64,abs(modWheel.curValue - 63));
      strip.setPixelColor(assignCmd[3],strip.gamma32(strip.ColorHSV(0,0,128)));
      strip.setPixelColor(assignCmd[4],strip.gamma32(strip.ColorHSV(
        hueM,satM,((modWheel.curValue > 63) ? satM : 0)
      )));
      strip.setPixelColor(assignCmd[5],strip.gamma32(strip.ColorHSV(
        hueM,satM,((modWheel.curValue > 63) ? 127 + (satM / 2) : 127 - (satM / 2))
      )));
      strip.setPixelColor(assignCmd[6],strip.gamma32(strip.ColorHSV(
        hueM,satM,127 + (satM / 2)
      )));
    };
    strip.show();
  }
  void dealWithRotary() {
    if (menu.readyForKey()) {
      rotaryIsClicked = digitalRead(ROT_PIN_C);
      if (rotaryIsClicked > rotaryWasClicked) {
        menu.registerKeyPress(GEM_KEY_OK);
        screenTime = 0;
      }
      rotaryWasClicked = rotaryIsClicked;
      if (rotaryKnobTurns != 0) {
        for (byte i = 0; i < abs(rotaryKnobTurns); i++) {
          menu.registerKeyPress(rotaryKnobTurns < 0 ? GEM_KEY_UP : GEM_KEY_DOWN);
        }
        rotaryKnobTurns = 0;
        screenTime = 0;
      }
    }
  }
  void readMIDI() {
    MIDI.read();
  }
  void keepTrackOfRotaryKnobTurns() {
    switch (rotary.process()) {
      case DIR_CW:
        rotaryKnobTurns++;
        break;
      case DIR_CCW:
        rotaryKnobTurns--;
        break;
    }
    rotaryKnobTurns = (
      rotaryKnobTurns > maxKnobTurns 
      ? maxKnobTurns 
      : (
        rotaryKnobTurns < -maxKnobTurns 
        ? -maxKnobTurns 
        : rotaryKnobTurns
      )
    );
  }

// ====== setup() and loop()

  void setup() {
    #if (defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040))
    TinyUSB_Device_Init(0);  // Manual begin() is required on core without built-in support for TinyUSB such as mbed rp2040
    #endif
    setupMIDI();
    setupFileSystem();
    setupPins();
    setupGrid();
    setupLEDs();
    setupGFX();
    setupMenu();
    for (byte i = 0; i < 5 && !TinyUSBDevice.mounted(); i++) {
      delay(1);  // wait until device mounted, maybe
    };
    testDiagnostics();  // Print diagnostic troubleshooting information to serial monitor
  }
  void setup1() {  // set up on second core
    setupPiezo();
  };
  void loop() {   // run on first core
    timeTracker();  // Time tracking functions
    screenSaver();  // Reduces wear-and-tear on OLED panel
    readHexes();       // Read and store the digital button states of the scanning matrix
    actionHexes();       // actions on hexes
    arpeggiate();      // arpeggiate the buzzer
    updateWheels();   // deal with the pitch/mod wheel
    animateLEDs();     // deal with animations
    lightUpLEDs();      // refresh LEDs
    dealWithRotary();  // deal with menu
    readMIDI();
  }
  void loop1() {  // run on second core
    keepTrackOfRotaryKnobTurns();
  }