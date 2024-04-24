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
  
  #include "Constants.h"       // preprocessor constants / macros
  #include "Classes.h"         // type definitions
  #include "Presets.h"         // pre-load tuning, scale, palette, layout definitions

  // ====== useful math functions
    int positiveMod(int n, int d) {
      return (((n % d) + d) % d);
    }
        
    byte byteLerp(byte xOne, byte xTwo, float yOne, float yTwo, float y) {
      float weight = (y - yOne) / (yTwo - yOne);
      int temp = xOne + ((xTwo - xOne) * weight);
      if (temp < xOne) {temp = xOne;};
      if (temp > xTwo) {temp = xTwo;};
      return temp;
    }

    Adafruit_USBD_MIDI usb_midi;
    // Create a new instance of the Arduino MIDI Library,
    // and attach usb_midi as the transport.
    MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

    Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

    Rotary rotary = Rotary(ROT_PIN_A, ROT_PIN_B);
    bool rotaryIsClicked = HIGH;          //
    bool rotaryWasClicked = HIGH;         //
    int8_t rotaryKnobTurns = 0;           //
    byte maxKnobTurns = 3;

  // Create an instance of the U8g2 graphics library.
    U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);

  // Create menu object of class GEM_u8g2. Supply its constructor with reference to u8g2 object we created earlier
    GEM_u8g2 menu(
      u8g2, GEM_POINTER_ROW, GEM_ITEMS_COUNT_AUTO, 
      MENU_ITEM_HEIGHT, MENU_PAGE_SCREEN_TOP_OFFSET, MENU_VALUES_LEFT_OFFSET
    );
    const byte defaultContrast = 63;                // GFX default contrast
    bool screenSaverOn = 0;                         //
    uint64_t screenTime = 0;                        // GFX timer to count if screensaver should go on
    const uint64_t screenSaverTimeout = (1u << 23); // 2^23 microseconds ~ 8 seconds

    const byte diagnostics = DIAGNOSTIC_ON;

  // Global time variables
    uint64_t runTime = 0;                // Program loop consistent variable for time in microseconds since power on
    uint64_t lapTime = 0;                // Used to keep track of how long each loop takes. Useful for rate-limiting.
    uint64_t loopTime = 0;               // Used to check speed of the loop in diagnostics mode 4

  // animation variables    E NE NW  W SW SE
    int8_t vertical[] =   { 0,-1,-1, 0, 1, 1};
    int8_t horizontal[] = { 2, 1,-1,-2,-1, 1};

    byte animationFPS = 32; // actually frames per 2^20 microseconds. close enough to 30fps
    int32_t rainbowDegreeTime = 65'536; // microseconds to go through 1/360 of rainbow

  // Button matrix and LED locations (PROD unit only)
    const byte mPin[] = { 
      MPLEX_1_PIN, MPLEX_2_PIN, MPLEX_4_PIN, MPLEX_8_PIN 
    };
    const byte cPin[] = { 
      COLUMN_PIN_0, COLUMN_PIN_1, COLUMN_PIN_2, COLUMN_PIN_3,
      COLUMN_PIN_4, COLUMN_PIN_5, COLUMN_PIN_6, 
      COLUMN_PIN_7, COLUMN_PIN_8, COLUMN_PIN_9 
    };
    const byte assignCmd[] = { 
      CMDBTN_0, CMDBTN_1, CMDBTN_2, CMDBTN_3, 
      CMDBTN_4, CMDBTN_5, CMDBTN_6
    };

  // MIDI note layout tables overhauled procedure since v1.1

    buttonDef h[LED_COUNT];         // a collection of all the buttons from 0 to 139
                                    // h[i] refers to the button with the LED address = i.
    byte enableMIDI = 1;
    const byte layoutCount = sizeof(layoutOptions) / sizeof(layoutDef);
    const byte scaleCount = sizeof(scaleOptions) / sizeof(scaleDef);

  // Tone and Arpeggiator variables
    oscillator synth[POLYPHONY_LIMIT]; // maximum polyphony
    byte poly = 0; // current polyphony
    std::queue<byte> openChannelQueue;
    const byte attenuation[] = {67,67,48,39,34,30,28,26,24,23,22,21,20,19,18,17};

    byte arpeggiatingNow = UNUSED_NOTE;         // if this is 255, buzzer set to off (0% duty cycle)
    uint64_t arpeggiateTime = 0;         // Used to keep track of when this note started buzzin
    uint32_t arpeggiateLength = 65'536;   // in microseconds

    byte scaleLock = 0;
    byte perceptual = 1;

    int velWheelSpeed = 8;
    int modWheelSpeed = 8;
    int pbWheelSpeed = 1024;

    wheelDef modWheel = { false, false, // standard mode, not sticky
      &h[assignCmd[4]].btnState, &h[assignCmd[5]].btnState, &h[assignCmd[6]].btnState,
      0, 127, &modWheelSpeed, 0, 0, 0, 0
    }; 
    wheelDef pbWheel =  { false, false, // standard mode, not sticky
      &h[assignCmd[4]].btnState, &h[assignCmd[5]].btnState, &h[assignCmd[6]].btnState,
      -8192, 8191, &pbWheelSpeed, 0, 0, 0, 0
    };
    wheelDef velWheel = { false, true, // standard mode, sticky
      &h[assignCmd[0]].btnState, &h[assignCmd[1]].btnState, &h[assignCmd[2]].btnState,
      0, 127, &velWheelSpeed, 96, 96, 96, 0
    };
    bool toggleWheel = 0; // 0 for mod, 1 for pb

  // MENU SYSTEM SETUP //
    // Create menu page object of class GEMPage. Menu page holds menu items (GEMItem) and represents menu level.
    // Menu can have multiple menu pages (linked to each other) with multiple menu items each

    GEMPage  menuPageMain("HexBoard MIDI Controller");
    GEMPage  menuPageTuning("Tuning");
    GEMItem  menuTuningBack("<< Back", menuPageMain);
    GEMItem  menuGotoTuning("Tuning", menuPageTuning);
    GEMPage  menuPageLayout("Layout");
    GEMItem  menuGotoLayout("Layout", menuPageLayout); 
    GEMItem  menuLayoutBack("<< Back", menuPageMain);
    GEMPage  menuPageScales("Scales");
    GEMItem  menuGotoScales("Scales", menuPageScales); 
    GEMItem  menuScalesBack("<< Back", menuPageMain);
    GEMPage  menuPageControl("Control wheel");
    GEMItem  menuGotoControl("Control wheel", menuPageControl);
    GEMItem  menuControlBack("<< Back", menuPageMain);
    
    // the following get initialized in the setup() routine.
    GEMItem* menuItemTuning[TUNINGCOUNT];       
    GEMItem* menuItemLayout[layoutCount];  
    GEMItem* menuItemScales[scaleCount];       
    GEMSelect* selectKey[TUNINGCOUNT];         
    GEMItem* menuItemKeys[TUNINGCOUNT];       

    void resetHexLEDs(); // forward-declaration
    SelectOptionByte optionByteYesOrNo[] =  { { "No", 0 }, { "Yes" , 1 } };
    GEMSelect selectYesOrNo( sizeof(optionByteYesOrNo)  / sizeof(SelectOptionByte), optionByteYesOrNo);
    GEMItem  menuItemScaleLock( "Scale lock?", scaleLock, selectYesOrNo);
    GEMItem  menuItemPercep( "Fix color:", perceptual, selectYesOrNo, resetHexLEDs);
    
    byte playbackMode = BUZZ_POLY;
    SelectOptionByte optionBytePlayback[] = { { "Off", BUZZ_OFF }, { "Mono", BUZZ_MONO }, { "Arp'gio", BUZZ_ARPEGGIO }, { "Poly", BUZZ_POLY } };
    GEMSelect selectPlayback(sizeof(optionBytePlayback) / sizeof(SelectOptionByte), optionBytePlayback);
    GEMItem  menuItemPlayback(  "Buzzer:",       playbackMode,  selectPlayback);

    void changeTranspose(); // forward-declaration
    int transposeSteps = 0;
    // doing this long-hand because the STRUCT has problems accepting string conversions of numbers for some reason
    SelectOptionInt optionIntTransposeSteps[] = {
      {"-127",-127},{"-126",-126},{"-125",-125},{"-124",-124},{"-123",-123},{"-122",-122},{"-121",-121},{"-120",-120},{"-119",-119},{"-118",-118},{"-117",-117},{"-116",-116},{"-115",-115},{"-114",-114},{"-113",-113},
      {"-112",-112},{"-111",-111},{"-110",-110},{"-109",-109},{"-108",-108},{"-107",-107},{"-106",-106},{"-105",-105},{"-104",-104},{"-103",-103},{"-102",-102},{"-101",-101},{"-100",-100},{"- 99",- 99},{"- 98",- 98},
      {"- 97",- 97},{"- 96",- 96},{"- 95",- 95},{"- 94",- 94},{"- 93",- 93},{"- 92",- 92},{"- 91",- 91},{"- 90",- 90},{"- 89",- 89},{"- 88",- 88},{"- 87",- 87},{"- 86",- 86},{"- 85",- 85},{"- 84",- 84},{"- 83",- 83},
      {"- 82",- 82},{"- 81",- 81},{"- 80",- 80},{"- 79",- 79},{"- 78",- 78},{"- 77",- 77},{"- 76",- 76},{"- 75",- 75},{"- 74",- 74},{"- 73",- 73},{"- 72",- 72},{"- 71",- 71},{"- 70",- 70},{"- 69",- 69},{"- 68",- 68},
      {"- 67",- 67},{"- 66",- 66},{"- 65",- 65},{"- 64",- 64},{"- 63",- 63},{"- 62",- 62},{"- 61",- 61},{"- 60",- 60},{"- 59",- 59},{"- 58",- 58},{"- 57",- 57},{"- 56",- 56},{"- 55",- 55},{"- 54",- 54},{"- 53",- 53},
      {"- 52",- 52},{"- 51",- 51},{"- 50",- 50},{"- 49",- 49},{"- 48",- 48},{"- 47",- 47},{"- 46",- 46},{"- 45",- 45},{"- 44",- 44},{"- 43",- 43},{"- 42",- 42},{"- 41",- 41},{"- 40",- 40},{"- 39",- 39},{"- 38",- 38},
      {"- 37",- 37},{"- 36",- 36},{"- 35",- 35},{"- 34",- 34},{"- 33",- 33},{"- 32",- 32},{"- 31",- 31},{"- 30",- 30},{"- 29",- 29},{"- 28",- 28},{"- 27",- 27},{"- 26",- 26},{"- 25",- 25},{"- 24",- 24},{"- 23",- 23},
      {"- 22",- 22},{"- 21",- 21},{"- 20",- 20},{"- 19",- 19},{"- 18",- 18},{"- 17",- 17},{"- 16",- 16},{"- 15",- 15},{"- 14",- 14},{"- 13",- 13},{"- 12",- 12},{"- 11",- 11},{"- 10",- 10},{"-  9",-  9},{"-  8",-  8},
      {"-  7",-  7},{"-  6",-  6},{"-  5",-  5},{"-  4",-  4},{"-  3",-  3},{"-  2",-  2},{"-  1",-  1},{"+/-0",   0},{"+  1",   1},{"+  2",   2},{"+  3",   3},{"+  4",   4},{"+  5",   5},{"+  6",   6},{"+  7",   7},
      {"+  8",   8},{"+  9",   9},{"+ 10",  10},{"+ 11",  11},{"+ 12",  12},{"+ 13",  13},{"+ 14",  14},{"+ 15",  15},{"+ 16",  16},{"+ 17",  17},{"+ 18",  18},{"+ 19",  19},{"+ 20",  20},{"+ 21",  21},{"+ 22",  22},
      {"+ 23",  23},{"+ 24",  24},{"+ 25",  25},{"+ 26",  26},{"+ 27",  27},{"+ 28",  28},{"+ 29",  29},{"+ 30",  30},{"+ 31",  31},{"+ 32",  32},{"+ 33",  33},{"+ 34",  34},{"+ 35",  35},{"+ 36",  36},{"+ 37",  37},
      {"+ 38",  38},{"+ 39",  39},{"+ 40",  40},{"+ 41",  41},{"+ 42",  42},{"+ 43",  43},{"+ 44",  44},{"+ 45",  45},{"+ 46",  46},{"+ 47",  47},{"+ 48",  48},{"+ 49",  49},{"+ 50",  50},{"+ 51",  51},{"+ 52",  52},
      {"+ 53",  53},{"+ 54",  54},{"+ 55",  55},{"+ 56",  56},{"+ 57",  57},{"+ 58",  58},{"+ 59",  59},{"+ 60",  60},{"+ 61",  61},{"+ 62",  62},{"+ 63",  63},{"+ 64",  64},{"+ 65",  65},{"+ 66",  66},{"+ 67",  67},
      {"+ 68",  68},{"+ 69",  69},{"+ 70",  70},{"+ 71",  71},{"+ 72",  72},{"+ 73",  73},{"+ 74",  74},{"+ 75",  75},{"+ 76",  76},{"+ 77",  77},{"+ 78",  78},{"+ 79",  79},{"+ 80",  80},{"+ 81",  81},{"+ 82",  82},
      {"+ 83",  83},{"+ 84",  84},{"+ 85",  85},{"+ 86",  86},{"+ 87",  87},{"+ 88",  88},{"+ 89",  89},{"+ 90",  90},{"+ 91",  91},{"+ 92",  92},{"+ 93",  93},{"+ 94",  94},{"+ 95",  95},{"+ 96",  96},{"+ 97",  97},
      {"+ 98",  98},{"+ 99",  99},{"+100", 100},{"+101", 101},{"+102", 102},{"+103", 103},{"+104", 104},{"+105", 105},{"+106", 106},{"+107", 107},{"+108", 108},{"+109", 109},{"+110", 110},{"+111", 111},{"+112", 112},
      {"+113", 113},{"+114", 114},{"+115", 115},{"+116", 116},{"+117", 117},{"+118", 118},{"+119", 119},{"+120", 120},{"+121", 121},{"+122", 122},{"+123", 123},{"+124", 124},{"+125", 125},{"+126", 126},{"+127", 127}
    };
    GEMSelect selectTransposeSteps( 255, optionIntTransposeSteps);
    GEMItem  menuItemTransposeSteps( "Transpose:",   transposeSteps,  selectTransposeSteps, changeTranspose);
    
    byte colorMode = TIERED_COLOR_MODE;
    SelectOptionByte optionByteColor[] =    { { "Rainbow", RAINBOW_MODE }, { "Tiered" , TIERED_COLOR_MODE } };
    GEMSelect selectColor( sizeof(optionByteColor) / sizeof(SelectOptionByte), optionByteColor);
    GEMItem  menuItemColor( "Color mode:", colorMode, selectColor, resetHexLEDs);

    byte animationType = ANIMATE_NONE;
    SelectOptionByte optionByteAnimate[] =  { { "None" , ANIMATE_NONE }, { "Octave", ANIMATE_OCTAVE },
      { "By Note", ANIMATE_BY_NOTE }, { "Star", ANIMATE_STAR }, { "Splash" , ANIMATE_SPLASH }, { "Orbit", ANIMATE_ORBIT } };
    GEMSelect selectAnimate( sizeof(optionByteAnimate)  / sizeof(SelectOptionByte), optionByteAnimate);
    GEMItem  menuItemAnimate( "Animation:", animationType, selectAnimate);

    byte currWave = WAVEFORM_SAW;
    SelectOptionByte optionByteWaveform[] = { { "Square", WAVEFORM_SQUARE }, { "Saw", WAVEFORM_SAW } };
    GEMSelect selectWaveform(sizeof(optionByteWaveform) / sizeof(SelectOptionByte), optionByteWaveform);
    GEMItem  menuItemWaveform( "Waveform:", currWave, selectWaveform);

    SelectOptionInt optionIntModWheel[] = { { "too slo", 1 }, { "Turtle", 2 }, { "Slow", 4 }, 
      { "Medium",    8 }, { "Fast",     16 }, { "Cheetah",  32 }, { "Instant", 127 } };
    GEMSelect selectModSpeed(sizeof(optionIntModWheel) / sizeof(SelectOptionInt), optionIntModWheel);
    GEMItem  menuItemModSpeed( "Mod wheel:", modWheelSpeed, selectModSpeed);
    GEMItem  menuItemVelSpeed( "Vel wheel:", velWheelSpeed, selectModSpeed);

    SelectOptionInt optionIntPBWheel[] =  { { "too slo", 128 }, { "Turtle", 256 }, { "Slow", 512 },  
      { "Medium", 1024 }, { "Fast", 2048 }, { "Cheetah", 4096 },  { "Instant", 16384 } };
    GEMSelect selectPBSpeed(sizeof(optionIntPBWheel) / sizeof(SelectOptionInt), optionIntPBWheel);
    GEMItem  menuItemPBSpeed( "PB wheel:", pbWheelSpeed, selectPBSpeed);

  // put all user-selectable options into a class so that down the line these can be saved and loaded.
    class presetDef { 
    public:
      std::string presetName; 
      int tuningIndex;     // instead of using pointers, i chose to store index value of each option, to be saved to a .pref or .ini or something
      int layoutIndex;
      int scaleIndex;
      int keyStepsFromA; // what key the scale is in, where zero equals A.
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
      int layoutsBegin() {
        if (tuningIndex == TUNING_12EDO) {
          return 0;
        } else {
          int temp = 0;
          while (layoutOptions[temp].tuning < tuningIndex) {
            temp++;
          };
          return temp;
        };
      };
      int keyStepsFromC() {
        return -(tuning().spanCtoA() + keyStepsFromA);
      };
      int pitchRelToA4(int givenStepsFromC) {
        return givenStepsFromC - tuning().spanCtoA() + transpose;
      };
      int keyDegree(int givenStepsFromC) {
        return positiveMod(givenStepsFromC + keyStepsFromC(), tuning().cycleLength);
      };
    };

    presetDef current = {
      "Default",      // name
      TUNING_12EDO,      // tuning
      0,              // default to the first layout, wicki hayden
      0,              // default to using no scale (chromatic)
      0,              // default to the key of C, which in 12EDO is -9 steps from A.
      0               // default to no transposition
    };

// ====== diagnostic wrapper

  void sendToLog(std::string msg) {
    if (diagnostics) {
      Serial.println(msg.c_str());
    };
  }

// ====== LED routines

  int16_t transformHue(float h) {
    //                red             yellow                 green               blue
    int hueIn[] =  {    0,    9,   18,   90,  108,  126,  135,  150,  198,  243,  252,  261,  306,  333,  360};
    //          #ff0000            #ffff00           #00ff00       #00ffff     #0000ff     #ff00ff
    int hueOut[] = {    0, 3640, 5461,10922,12743,16384,21845,27306,32768,38229,43690,49152,54613,58254,65535};
    float D = fmod(h,360);
    byte B = 0;
    while (D - hueIn[B] > 0) {
      B++;
    };
    float T = (D - hueIn[B - 1]) / (float)(hueIn[B] - hueIn[B - 1]);
    return (hueOut[B - 1] * (1 - T)) + (hueOut[B] * T);
  }

  uint32_t getLEDcode(colorDef c) {
    return strip.gamma32(strip.ColorHSV(transformHue(c.hue),c.sat,c.val));
  }

  void resetHexLEDs() { // calculate color codes for each hex, store for playback
    for (byte i = 0; i < LED_COUNT; i++) {
      if (!(h[i].isCmd)) {
        colorDef setColor;
        byte paletteIndex = positiveMod(h[i].stepsFromC,current.tuning().cycleLength);
        switch (colorMode) {
          case TIERED_COLOR_MODE:
            setColor = palette[current.tuningIndex].getColor(paletteIndex);
            break;
          default:
            setColor = 
              { 360.0 * ((float)paletteIndex / (float)current.tuning().cycleLength)
              , SAT_VIVID
              , VALUE_NORMAL
              };
            break;
        };
        h[i].LEDcolorOn   = getLEDcode(setColor);
        h[i].LEDcolorPlay = getLEDcode(setColor.mixWithWhite()); // "mix with white"
        setColor = {HUE_NONE,SAT_BW,VALUE_BLACK};
        h[i].LEDcolorOff  = getLEDcode(setColor);                // turn off entirely
        h[i].LEDcolorAnim = h[i].LEDcolorPlay;
      };
    };
    sendToLog("LED codes re-calculated.");
  }

  void resetVelocityLEDs() {
    colorDef tempColor = 
      { (runTime % (rainbowDegreeTime * 360)) / rainbowDegreeTime
      , SAT_MODERATE
      , byteLerp(0,255,85,127,velWheel.curValue)
      };
    strip.setPixelColor(assignCmd[0], getLEDcode(tempColor));

    tempColor.val = byteLerp(0,255,42,85,velWheel.curValue);
    strip.setPixelColor(assignCmd[1], getLEDcode(tempColor));
    
    tempColor.val = byteLerp(0,255,0,42,velWheel.curValue);
    strip.setPixelColor(assignCmd[2], getLEDcode(tempColor));
  }
  void resetWheelLEDs() {
    // middle button
    int tempSat = SAT_BW;
    colorDef tempColor = {HUE_NONE, tempSat, (toggleWheel ? VALUE_SHADE : VALUE_LOW)};
    strip.setPixelColor(assignCmd[3], getLEDcode(tempColor));
    if (toggleWheel) {
      // pb red / green
      tempSat = byteLerp(SAT_BW,SAT_VIVID,0,8192,abs(pbWheel.curValue));
      tempColor = {((pbWheel.curValue > 0) ? HUE_RED : HUE_CYAN), tempSat, VALUE_FULL};
      strip.setPixelColor(assignCmd[5], getLEDcode(tempColor));

      tempColor.val = tempSat * (pbWheel.curValue > 0);
      strip.setPixelColor(assignCmd[4], getLEDcode(tempColor));

      tempColor.val = tempSat * (pbWheel.curValue < 0);
      strip.setPixelColor(assignCmd[6], getLEDcode(tempColor));
    } else {
      // mod blue / yellow
      tempSat = byteLerp(SAT_BW,SAT_VIVID,0,64,abs(modWheel.curValue - 63));
      tempColor = {((modWheel.curValue > 63) ? HUE_YELLOW : HUE_INDIGO), tempSat, 127 + (tempSat / 2)};
      strip.setPixelColor(assignCmd[6], getLEDcode(tempColor));

      if (modWheel.curValue <= 63) {
        tempColor.val = 127 - (tempSat / 2);
      }
      strip.setPixelColor(assignCmd[5], getLEDcode(tempColor));
      
      tempColor.val = tempSat * (modWheel.curValue > 63);
      strip.setPixelColor(assignCmd[4], getLEDcode(tempColor));
    };
  }
  uint32_t applyNotePixelColor(byte x){
           if (h[x].animate) {     return h[x].LEDcolorAnim;
    } else if (h[x].channel) {     return h[x].LEDcolorPlay;
    } else if (h[x].inScale) {     return h[x].LEDcolorOn;
    } else {                       return h[x].LEDcolorOff;
    };
  }

// ====== layout routines

  float freqToMIDI(float Hz) {             // formula to convert from Hz to MIDI note
    return 69.0 + 12.0 * log2f(Hz / 440.0);
  }
  float MIDItoFreq(float MIDI) {           // formula to convert from MIDI note to Hz
    return 440.0 * exp2((MIDI - 69.0) / 12.0);
  }
  float stepsToMIDI(int16_t stepsFromA) {  // return the MIDI pitch associated
    return freqToMIDI(CONCERT_A_HZ) + ((float)stepsFromA * (float)current.tuning().stepSize / 100.0);
  }

  void assignPitches() {     // run this if the layout, key, or transposition changes, but not if color or scale changes
    sendToLog("assignPitch was called:");
    for (byte i = 0; i < LED_COUNT; i++) {
      if (!(h[i].isCmd)) {
        // steps is the distance from C
        // the stepsToMIDI function needs distance from A4
        // it also needs to reflect any transposition, but
        // NOT the key of the scale.
        float N = stepsToMIDI(current.pitchRelToA4(h[i].stepsFromC));
        if (N < 0 || N >= 128) {
          h[i].note = UNUSED_NOTE;
          h[i].bend = 0;
          h[i].frequency = 0.0;
        } else {
          h[i].note = ((N >= 127) ? 127 : round(N));
          h[i].bend = (ldexp(N - h[i].note, 13) / PITCH_BEND_SEMIS);
          h[i].frequency = MIDItoFreq(N);
        };
        sendToLog(
          "hex #" + std::to_string(i) + ", " +
          "steps=" + std::to_string(h[i].stepsFromC) + ", " +
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
        if (current.scale().tuning == ALL_TUNINGS) {
          h[i].inScale = 1;
        } else {
          byte degree = current.keyDegree(h[i].stepsFromC); 
          if (degree == 0) {
            h[i].inScale = 1;    // the root is always in the scale
          } else {
            byte tempSum = 0;
            byte iterator = 0;
            while (degree > tempSum) {
              tempSum += current.scale().pattern[iterator];
              iterator++;
            };  // add the steps in the scale, and you're in scale
            h[i].inScale = (tempSum == degree);   // if the note lands on one of those sums
          };
        };
        sendToLog(
          "hex #" + std::to_string(i) + ", " +
          "steps=" + std::to_string(h[i].stepsFromC) + ", " +
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
        int8_t distCol = h[i].coordCol - h[current.layout().hexMiddleC].coordCol;
        int8_t distRow = h[i].coordRow - h[current.layout().hexMiddleC].coordRow;
        h[i].stepsFromC = (
          (distCol * current.layout().acrossSteps) + 
          (distRow * (
            current.layout().acrossSteps + 
            (2 * current.layout().dnLeftSteps)
          ))
        ) / 2;  
        sendToLog(
          "hex #" + std::to_string(i) + ", " +
          "steps from C4=" + std::to_string(h[i].stepsFromC) + "."
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
  // it is theoretically possible to emulate waveforms with 4 bits resolution (260kHz / 12.5kHz)
  // but we are limited by calculation time.
  // the macro POLLING_INTERVAL_IN_MICROSECONDS is set to a value that is long enough
  // that the audio output is accurate, but short enough to allow as much resolution as possible.
  // currently, 32 microseconds appears to be sufficient (about 500 CPU cycles).
  //
  // 1) set a constant PWM signal at F_CPU/512 (260kHz) to play on pin 23
  //    the PWM signal can emulate an analog value from 0 to 255.
  //    this is done in setup1().
  // 2) if a note is to be played on the buzzer, assign a channel (same as MPE mode for MIDI)
  //    and calculate the frequency. this might include pitch bends.
  //    this is done in buzz().
  // 3) the frequency is expressed as "amount you'd increment a counter every polling interval
  //    so that you roll over a 16-bit (65536) value at that frequency.
  // example: 440Hz note, 32microS polling
  //    65536 x 440/s x .000032s = an increment of 923 per poll
  //    this is done in buzz().
  // 4) the object called synth[] stores the increment and counter for each channel (0-14)=MIDI(2 thru 16)
  //    at every poll, each counter is incremented (will roll over since the type is 16-bit unsigned integer)
  //    and depending on the waveform, the 8-bit analog level is calculated.
  //    example: square waves return 0 if the counter is 0-32767, 255 if 32768-65535.
  //             saw waves return (counter / 256).
  // 5) the analog levels are mixed. i use an attenuation function, basically (# of simultaneous notes) ^ -0.5,
  //    so the perceived volume is consistent. the velocity wheel is also multiplied in.
  // 6) hardware timers are used because they will interrupt and run even if other code is active.
  //    otherwise, the subperiod is essentially floored at the length of the main loop() which is
  //    thousands of microseconds long!
  //    further, we can run this process on the 2nd core so it doesn't interrupt the user experience
  // the implementation of 6) is to make a single timer that calls back an interrupt function called poll().
  // the callback function then resets the interrupt flag and resets the timer alarm.
  // the timer is set to go off at the time of the last timer + the polling interval


  // RUN ON CORE 2
  void poll() {
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + POLL_INTERVAL_IN_MICROSECONDS;
    uint16_t lvl = 0;
    byte p;
    for (byte i = 0; i < POLYPHONY_LIMIT; i++) {
      synth[i].counter += synth[i].increment; // should loop from 65536 -> 0
      switch (currWave) {
        case WAVEFORM_SQUARE:
          p = 0 - ((synth[i].counter & 0x8000) >> 15);   // grab first bit -> 0 or -1 (255)
          break;
        case WAVEFORM_SAW:
          p = (synth[i].counter >> 8);  // 0 thru 255
          break;
        default:
          p = 0;
          break;
      };
      lvl += p;  // for polyphony=15, cap=255*15=3825
    };
    lvl = (lvl * attenuation[poly]) >> 8; // cap = 3825 * 17 / 256 = 254
    lvl = (lvl * velWheel.curValue) >> 7;
    pwm_set_chan_level(TONE_SL, TONE_CH, lvl);
  }
  // RUN ON CORE 1
  void buzz(byte x, bool p) {
    byte ch = h[x].channel - 2;
    synth[ch].counter = 0;
    if (p) {
      synth[ch].increment = h[x].frequency                     // note frequency
        * exp2(pbWheel.curValue * PITCH_BEND_SEMIS / 98304.0)  // adjusted for global pitch bend
        * ((POLL_INTERVAL_IN_MICROSECONDS << 16) / 1000000);   // cycle 0-65535 at resultant frequency
    } else {
      synth[ch].increment = 0;                                 // zero effectively silences the channel
    };
  }

// ====== MIDI routines
  void setAllNotesOff(byte Ch) {
    if (enableMIDI) {
      MIDI.sendControlChange(123, 0, Ch);
    }
  }
  void setPitchBendRange(byte Ch, byte semitones) {
    if (enableMIDI) {     // MIDI mode only
      MIDI.beginRpn(0, Ch);
      MIDI.sendRpnValue(semitones << 7, Ch);
      MIDI.endRpn(Ch);
      sendToLog(
        "set pitch bend range on ch " +
        std::to_string(Ch) + " to be " + 
        std::to_string(semitones) + " semitones"
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
  void applyMPEmode() {
    while (!openChannelQueue.empty()) {         // empty the channel queue
      openChannelQueue.pop();
    };
    for (byte i = 1; i <= 16; i++) {
      setAllNotesOff(i);                        // turn off all notes
      setPitchBendRange(i, PITCH_BEND_SEMIS);   // force pitch bend back to the expected range of 2 semitones.
    };
    setMPEzone(1, POLYPHONY_LIMIT);   // MPE zone 1 = ch 2 thru 16
    for (byte i = 0; i < POLYPHONY_LIMIT; i++) {
      openChannelQueue.push(i + 2);
      sendToLog("pushed ch " + std::to_string(i + 2) + " to the open channel queue");
    };
  }
  void chgModulation() {
    if (enableMIDI) {     // MIDI mode only
      MIDI.sendControlChange(1, modWheel.curValue, 1);
      sendToLog("sent mod value " + std::to_string(modWheel.curValue) + " to ch 1");
    };
  }
  void chgUniversalPB() {
    if (enableMIDI) {     // MIDI mode only
      MIDI.sendPitchBend(pbWheel.curValue, 1);
      for (byte i = 0; i < LED_COUNT; i++) {
        if (!(h[i].isCmd)) {
          if (h[i].channel) {
            buzz(i,true);           // rebuzz all notes if the pitch bend changes
          };
        };
      };
      sendToLog("sent pb wheel value " + std::to_string(pbWheel.curValue) + " to ch 1");
    };
  }
  
// ====== hex press routines

  void playNote(byte x) {
    // this gets called on any non-command hex
    // that is not scale-locked.
    if (!(h[x].channel)) {    // but just in case, check
      if (openChannelQueue.empty()) {   // if there aren't any open channels
        sendToLog("channel queue was empty so did not play");
      } else {
        h[x].channel = openChannelQueue.front();   // value in MIDI terms (1-16)
        openChannelQueue.pop();
        sendToLog("popped " + std::to_string(h[x].channel) + " off the queue");
        if (!(playbackMode == BUZZ_OFF)) {
          buzz(x, true);
        };
        if (enableMIDI) {
          MIDI.sendPitchBend(h[x].bend, h[x].channel); // ch 1-16
          MIDI.sendNoteOn(h[x].note, velWheel.curValue, h[x].channel); // ch 1-16
          sendToLog(
            "sent MIDI noteOn: " + std::to_string(h[x].note) +
            " pb "  + std::to_string(h[x].bend) +
            " vel " + std::to_string(velWheel.curValue) +
            " ch "  + std::to_string(h[x].channel)
          );
        };
      };
    };
  }
  void stopNote(byte x) {
    // this gets called on any non-command hex
    // that is not scale-locked.
    if (h[x].channel) {    // but just in case, check
      openChannelQueue.push(h[x].channel);
      sendToLog("pushed " + std::to_string(h[x].channel) + " on the queue");
      if (!(playbackMode == BUZZ_OFF)) {
        buzz(x, false);
      };
      if (playbackMode == BUZZ_ARPEGGIO) {
        arpeggiateTime = 0;    // trigger arpeggiate function early if any note changes
      };
      if (enableMIDI) {
        MIDI.sendNoteOff(h[x].note, velWheel.curValue, h[x].channel);
        sendToLog(
          "sent note off: " + std::to_string(h[x].note) +
          " pb " + std::to_string(h[x].bend) +
          " vel " + std::to_string(velWheel.curValue) +
          " ch " + std::to_string(h[x].channel)
        );
      };
      h[x].channel = 0;
    };
  }
  void cmdOn(byte x) {   // volume and mod wheel read all current buttons
    switch (h[x].note) {
      case CMDB + 3:
        toggleWheel = !toggleWheel;
        break;
      default:
        // the rest should all be taken care of within the wheelDef structure
        break;
    };
  }
  void cmdOff(byte x) {   // pitch bend wheel only if buttons held.
    switch (h[x].note) {
      default:
        break;  // nothing; should all be taken care of within the wheelDef structure
    };
  }

// ====== animations
  uint64_t animFrame(byte x) {     
    if (h[x].timePressed) {          // 2^20 microseconds is close enough to 1 second
      return 1 + (((runTime - h[x].timePressed) * animationFPS) >> 20);
    } else {
      return 0;
    };
  }
  void flagToAnimate(int8_t r, int8_t c) {
    if (! 
      (    ( r < 0 ) || ( r >= ROWCOUNT )
        || ( c < 0 ) || ( c >= (2 * COLCOUNT) )
        || ( ( c + r ) & 1 )
      )
    ) {
      h[(10 * r) + (c / 2)].animate = 1;
    };
  }
  void animateMirror() {
    for (byte i = 0; i < LED_COUNT; i++) {                   // check every hex
      if ((!(h[i].isCmd)) && (h[i].channel)) {              // that is a held note     
        for (byte j = 0; j < LED_COUNT; j++) {               // compare to every hex
          if ((!(h[j].isCmd)) && (!(h[j].channel))) {       // that is a note not being played
            int16_t temp = h[i].stepsFromC - h[j].stepsFromC;         // look at difference between notes
            if (animationType == ANIMATE_OCTAVE) {              // set octave diff to zero if need be
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
      if ((!(h[i].isCmd)) && (h[i].channel) && ((h[i].inScale) || (!scaleLock))) {    // that is a held note
        byte tempDir = (animFrame(i) % 6);
        flagToAnimate(h[i].coordRow + vertical[tempDir], h[i].coordCol + horizontal[tempDir]);       // different neighbor each frame
      };
    };
  }

  void animateRadial() {
    for (byte i = 0; i < LED_COUNT; i++) {                              // check every hex
      if (!(h[i].isCmd) && ((h[i].inScale) || (!scaleLock))) {                                              // that is a note
        uint64_t radius = animFrame(i);
        if ((radius > 0) && (radius < 16)) {                            // played in the last 16 frames
          byte steps = ((animationType == ANIMATE_SPLASH) ? radius : 1);    // star = 1 step to next corner; ring = 1 step per hex
          int8_t turtleRow = h[i].coordRow + (radius * vertical[HEX_DIRECTION_SW]);
          int8_t turtleCol = h[i].coordCol + (radius * horizontal[HEX_DIRECTION_SW]);
          for (byte dir = HEX_DIRECTION_EAST; dir < 6; dir++) {        // walk along the ring in each of the 6 hex directions
            for (byte i = 0; i < steps; i++) {                          // # of steps to the next corner 
              flagToAnimate(turtleRow,turtleCol);                     // flag for animation
              turtleRow += (vertical[dir] * (radius / steps));
              turtleCol += (horizontal[dir] * (radius / steps));
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
      menuItemScales[S]->hide((scaleOptions[S].tuning != current.tuningIndex) && (scaleOptions[S].tuning != ALL_TUNINGS));
    };
    sendToLog("menu: Scale choices were updated.");
  }
  void showOnlyValidKeyChoices() { // re-run at setup and whenever tuning changes
    for (int T = 0; T < TUNINGCOUNT; T++) {
      menuItemKeys[T]->hide((T != current.tuningIndex));
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
  void changeKey() {     // when you change the key via the menu
    applyScale();
  }
  void changeTranspose() {     // when you change the transpose via the menu
    current.transpose = transposeSteps;
    assignPitches();
  }
  void changeTuning(GEMCallbackData callbackData) { // not working yet
    byte selection = callbackData.valByte;
    if (selection != current.tuningIndex) {
      current.tuningIndex = selection;
      current.layoutIndex = current.layoutsBegin();        // reset layout to first in list
      current.scaleIndex = 0;                              // reset scale to "no scale"
      current.keyStepsFromA = current.tuning().spanCtoA(); // reset key to C
      showOnlyValidLayoutChoices();                        // change list of choices in GEM Menu
      showOnlyValidScaleChoices();                         // change list of choices in GEM Menu
      showOnlyValidKeyChoices();                           // change list of choices in GEM Menu
      applyLayout();   // apply changes above
      applyMPEmode();  // clear out MIDI queue
    };
    menuHome();
  }
  void createTuningMenuItems() {
    for (byte T = 0; T < TUNINGCOUNT; T++) {
      menuItemTuning[T] = new GEMItem(tuningOptions[T].name.c_str(), changeTuning, T);
      menuPageTuning.addMenuItem(*menuItemTuning[T]);
    };
  }
  void createLayoutMenuItems() {
    for (byte L = 0; L < layoutCount; L++) { // create pointers to all layouts
      menuItemLayout[L] = new GEMItem(layoutOptions[L].name.c_str(), changeLayout, L);
      menuPageLayout.addMenuItem(*menuItemLayout[L]);
    };
    showOnlyValidLayoutChoices();
  }
  void createKeyMenuItems() {
    for (byte T = 0; T < TUNINGCOUNT; T++) {
      selectKey[T] = new GEMSelect(tuningOptions[T].cycleLength, tuningOptions[T].keyChoices);
      menuItemKeys[T] = new GEMItem("Key:", current.keyStepsFromA, *selectKey[T], changeKey);
      menuPageScales.addMenuItem(*menuItemKeys[T]);
    };
    showOnlyValidKeyChoices();
  }
  void createScaleMenuItems() {
    for (int S = 0; S < scaleCount; S++) {  // create pointers to all scale items, filter them as you go
      menuItemScales[S] = new GEMItem(scaleOptions[S].name.c_str(), changeScale, S);
      menuPageScales.addMenuItem(*menuItemScales[S]);
    };
    showOnlyValidScaleChoices();
  }

// ====== setup routines
  void testDiagnostics() {
    sendToLog("theHDM was here");
  }
  void setupMIDI() {
    usb_midi.setStringDescriptor("HexBoard MIDI");  // Initialize MIDI, and listen to all MIDI channels
    MIDI.begin(MIDI_CHANNEL_OMNI);                  // This will also call usb_midi's begin()
    applyMPEmode();
    sendToLog("setupMIDI okay");
  }
  void setupFileSystem() {
    Serial.begin(115200);     // Set serial to make uploads work without bootsel button
    LittleFSConfig cfg;       // Configure file system defaults
    cfg.setAutoFormat(true);  // Formats file system if it cannot be mounted.
    LittleFS.setConfig(cfg);
    LittleFS.begin();  // Mounts file system.
    if (!LittleFS.begin()) {
      sendToLog("An Error has occurred while mounting LittleFS");
    } else {
      sendToLog("LittleFS mounted OK");
    }
  }
  void setupPins() {
    for (byte p = 0; p < sizeof(cPin); p++) { // For each column pin...
      pinMode(cPin[p], INPUT_PULLUP);  // set the pinMode to INPUT_PULLUP (+3.3V / HIGH).
    }
    for (byte p = 0; p < sizeof(mPin); p++) { // For each column pin...
      pinMode(mPin[p], OUTPUT);  // Setting the row multiplexer pins to output.
    }
    Wire.setSDA(SDAPIN);
    Wire.setSCL(SCLPIN);
    pinMode(ROT_PIN_C, INPUT_PULLUP);
    sendToLog("Pins mounted");
  }
  void setupGrid() {
    for (byte i = 0; i < LED_COUNT; i++) {
      h[i].coordRow = (i / 10);
      h[i].coordCol = (2 * (i % 10)) + (h[i].coordRow & 1);
      h[i].isCmd = 0;
      h[i].note = UNUSED_NOTE;
      h[i].btnState = 0;
    };
    for (byte c = 0; c < CMDCOUNT; c++) {
      h[assignCmd[c]].isCmd = 1;
      h[assignCmd[c]].note = CMDB + c;
    };
    sendToLog("initializing hex grid...");
    applyLayout();
  }
  void setupLEDs() { 
    strip.begin();    // INITIALIZE NeoPixel strip object
    strip.show();     // Turn OFF all pixels ASAP
    sendToLog("LEDs started..."); 
    resetHexLEDs();
  }
  void setupMenu() { 
    menu.setSplashDelay(0);
    menu.init();
    menuPageMain.addMenuItem(menuGotoTuning);
      createTuningMenuItems();
      menuPageTuning.addMenuItem(menuTuningBack);
    menuPageMain.addMenuItem(menuGotoLayout);
      createLayoutMenuItems();
      menuPageLayout.addMenuItem(menuLayoutBack);
    menuPageMain.addMenuItem(menuGotoScales);
      createKeyMenuItems();
      menuPageScales.addMenuItem(menuItemScaleLock);
      createScaleMenuItems();
      menuPageScales.addMenuItem(menuScalesBack);
    menuPageMain.addMenuItem(menuGotoControl);
      menuPageControl.addMenuItem(menuItemPBSpeed);
      menuPageControl.addMenuItem(menuItemModSpeed);
      menuPageControl.addMenuItem(menuItemVelSpeed);
      menuPageControl.addMenuItem(menuControlBack);
    menuPageMain.addMenuItem(menuItemTransposeSteps);
    menuPageMain.addMenuItem(menuItemColor);
    menuPageMain.addMenuItem(menuItemPlayback);  
    menuPageMain.addMenuItem(menuItemWaveform);
    menuPageMain.addMenuItem(menuItemAnimate);
    menuHome();
  }
  void setupGFX() {
    u8g2.begin();                       // Menu and graphics setup
    u8g2.setBusClock(1000000);          // Speed up display
    u8g2.setContrast(defaultContrast);  // Set contrast
    sendToLog("U8G2 graphics initialized.");
  }
  void setupPiezo() {
    gpio_set_function(TONEPIN, GPIO_FUNC_PWM);    // set that pin as PWM
    pwm_set_phase_correct(TONE_SL, true);         // phase correct sounds better
    pwm_set_wrap(TONE_SL, 254);                   // 0 - 254 allows 0 - 255 level
    pwm_set_clkdiv(TONE_SL, 1.0f);                // run at full clock speed
    pwm_set_chan_level(TONE_SL, TONE_CH, 0);      // initialize at zero to prevent whining sound
    pwm_set_enabled(TONE_SL, true);               // ENGAGE!
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);  // initialize the timer
    irq_set_exclusive_handler(ALARM_IRQ, poll);  // function to run every interrupt
    irq_set_enabled(ALARM_IRQ, true);             // ENGAGE!
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + POLL_INTERVAL_IN_MICROSECONDS;
    sendToLog("buzzer is ready.");
  }

// ====== loop routines
  void timeTracker() {
    lapTime = runTime - loopTime;
    loopTime = runTime;                                 // Update previousTime variable to give us a reference point for next loop
    runTime = timer_hw->timerawh;
    runTime = (runTime << 32) + (timer_hw->timerawl);   // Store the current time in a uniform variable for this program loop
  }
  void screenSaver() {
    if (screenTime <= screenSaverTimeout) {
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
    for (byte r = 0; r < ROWCOUNT; r++) {      // Iterate through each of the row pins on the multiplexing chip.
      for (byte d = 0; d < 4; d++) {
        digitalWrite(mPin[d], (r >> d) & 1);
      }
      for (byte c = 0; c < COLCOUNT; c++) {    // Now iterate through each of the column pins that are connected to the current row pin.
        byte p = cPin[c];                      // Hold the currently selected column pin in a variable.
        pinMode(p, INPUT_PULLUP);              // Set that row pin to INPUT_PULLUP mode (+3.3V / HIGH).
        byte i = c + (r * COLCOUNT);
        delayMicroseconds(6);                  // delay while column pin mode
        bool didYouPressHex = (digitalRead(p) == LOW);  // hex is pressed if it returns LOW. else not pressed
        h[i].interpBtnPress(didYouPressHex);
        if (h[i].btnState == 1) {
          h[i].timePressed = runTime;          // log the time
        };
        pinMode(p, INPUT);                     // Set the selected column pin back to INPUT mode (0V / LOW).
       }
    }
  }
  void actionHexes() { 
    for (byte i = 0; i < LED_COUNT; i++) {   // For all buttons in the deck
      switch (h[i].btnState) {
        case 1: // just pressed
          if (h[i].isCmd) {
            cmdOn(i);
          } else if (h[i].inScale || (!scaleLock)) {
            playNote(i);
          };
          break;
        case 2: // just released
          if (h[i].isCmd) {
            cmdOff(i);
          } else if (h[i].inScale || (!scaleLock)) {
            stopNote(i);
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
    if (playbackMode == BUZZ_ARPEGGIO) {
      if (runTime - arpeggiateTime > arpeggiateLength) {
        arpeggiateTime = runTime;
        byte n = UNUSED_NOTE;
        for (byte i = 1; i < LED_COUNT; i++) {
          byte j = positiveMod(arpeggiatingNow + i, LED_COUNT);
          if ((h[j].channel) && (!h[j].isCmd)) {
            n = j;
            break;
          };
        };
        arpeggiatingNow = n;
        if (n != UNUSED_NOTE) {
          buzz(n, true);
        };
      };
    };
  }
  void updateWheels() {  
    velWheel.setTargetValue();
    bool upd = velWheel.updateValue(runTime);
    if (upd) {
      sendToLog("vel became " + std::to_string(velWheel.curValue));
    }
    if (toggleWheel) {
      pbWheel.setTargetValue();
      upd = pbWheel.updateValue(runTime);
      if (upd) {
        chgUniversalPB();
      };
    } else {
      modWheel.setTargetValue();
      upd = modWheel.updateValue(runTime);
      if (upd) {
        chgModulation();
      };
    };
  }
  
  void animateLEDs() {  
    for (byte i = 0; i < LED_COUNT; i++) {      
      h[i].animate = 0;
    };
    if (animationType) {
      switch (animationType) { 
        case ANIMATE_STAR: case ANIMATE_SPLASH:
          animateRadial();
          break;
        case ANIMATE_ORBIT:
          animateOrbit();
          break;
        case ANIMATE_OCTAVE: case ANIMATE_BY_NOTE:
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
        strip.setPixelColor(i,applyNotePixelColor(i));
      }
    };
    resetVelocityLEDs();
    resetWheelLEDs();
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
      case DIR_CW:      rotaryKnobTurns++;   break;
      case DIR_CCW:     rotaryKnobTurns--;   break;
    }
    rotaryKnobTurns = (
      (rotaryKnobTurns > maxKnobTurns) ? maxKnobTurns : (
        (rotaryKnobTurns < -maxKnobTurns) ? -maxKnobTurns : rotaryKnobTurns 
      )
    );
  }

// ====== setup() and loop()

  void setup() {
    testDiagnostics();  // Print diagnostic troubleshooting information to serial monitor
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
  }
  void setup1() {  // set up on second core
    setupPiezo();
  }
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