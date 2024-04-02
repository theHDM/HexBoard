// ====== Hexperiment
  // Sketch to program the hexBoard to handle microtones
  // March 2024, theHDM / Nicholas Fox
  // major thanks to Zach and Jared!
  // Arduino IDE setup:
  // Board = Generic RP2040 (use the following additional board manager repo:
  // https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json)
  // Patches needed for U8G2, Rotary.h
  // ==============================================================================
  // list of things remaining to do:
  // -- program the wheel -- OK!
  // -- put back the animations -- OK!
  // -- test MPE working on iPad garageband -- works on pianoteq without MPE; need to get powered connection to iOS.
  // -- volume control test on buzzer
  // -- save and load presets
  // -- sequencer restore
  // ==============================================================================
  //
  #include <Arduino.h>
  #include <Wire.h>
  #include <LittleFS.h>
  #include <queue>              // std::queue construct to store open channels in microtonal mode
  const byte diagnostics = 1;
// ====== initialize timers

  uint32_t runTime = 0;                 // Program loop consistent variable for time in milliseconds since power on
  uint32_t lapTime = 0;                 // Used to keep track of how long each loop takes. Useful for rate-limiting.
  uint32_t loopTime = 0;               // Used to check speed of the loop in diagnostics mode 4

// ====== initialize SDA and SCL pins for hardware I/O

  const byte lightPinSDA = 16;
  const byte lightPinSCL = 17;

// ====== initialize MIDI

  #include <Adafruit_TinyUSB.h>
  #include <MIDI.h>
  Adafruit_USBD_MIDI usb_midi;
  MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);
  float concertA = 440.0;               // tuning of A4 in Hz
  byte MPE = 0; // microtonal mode. if zero then attempt to self-manage multiple channels.
              // if one then on certain synths that are MPE compatible will send in that mode.
  int16_t channelBend[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };  // what's the current note bend on this channel
  byte channelPoly[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };      // how many notes are playing on this channel
  std::queue<byte> openChannelQueue;
  const byte defaultPBRange = 2;

// ====== initialize LEDs

  #include <Adafruit_NeoPixel.h>
  const byte multiplexPins[] = { 4, 5, 2, 3 };  // m1p, m2p, m4p, m8p
  const byte rowCount = 14;                     // The number of rows in the matrix
  const byte columnPins[] = { 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
  const byte colCount = sizeof(columnPins);  // The number of columns in the matrix
  const byte hexCount = colCount * rowCount;  // The number of elements in the matrix
  const byte LEDPin = 22;
  Adafruit_NeoPixel strip(hexCount, LEDPin, NEO_GRB + NEO_KHZ800);
  enum { NoAnim, StarAnim, SplashAnim, OrbitAnim, OctaveAnim, NoteAnim };
  byte animationType = 0;
  byte animationFPS = 32; // actually frames per 2^10 seconds. close enough to 30fps
  int16_t rainbowDegreeTime = 64; // ms to go through 1/360 of rainbow.
// ====== initialize hex state object

  enum { Right, UpRight, UpLeft, Left, DnLeft, DnRight };
  typedef struct {
    int8_t row;
    int8_t col;
  } coordinates;
  typedef struct {
    byte keyState = 0;          // binary 00 = off, 01 = just pressed, 10 = just released, 11 = held
    coordinates coords = {0,0};
    uint32_t timePressed = 0;   // timecode of last press
    uint32_t LEDcolorAnim = 0;      //
    uint32_t LEDcolorPlay = 0;      //
    uint32_t LEDcolorOn = 0;   //
    uint32_t LEDcolorOff = 0;  //
    bool animate = 0;           // hex is flagged as part of the animation in this frame
    int16_t steps = 0;          // number of steps from key center (semitones in 12EDO; microtones if >12EDO)
    bool isCmd = 0;             // 0 if it's a MIDI note; 1 if it's a MIDI control cmd
    bool inScale = 0;           // 0 if it's not in the selected scale; 1 if it is
    byte note = 255;            // MIDI note or control parameter corresponding to this hex
    int16_t bend;               // in microtonal mode, the pitch bend for this note needed to be tuned correctly
    byte channel;               // what MIDI channel this note is playing on
    float frequency;            // what frequency to ring on the buzzer
    void updateKeyState(bool keyPressed) {
      keyState = (((keyState << 1) + keyPressed) & 3);
      if (keyState == 1) {
        timePressed = millis();  // log the time
      };
    };
    uint32_t animFrame() {
      if (timePressed) {    // 2^10 milliseconds is close enough to 1 second
        return 1 + (((runTime - timePressed) * animationFPS) >> 10);
      } else {
        return 0;
      };
    };
  } buttonDef;
  buttonDef h[hexCount]; // array of hex objects from 0 to 139
  const byte assignCmd[] = { 0, 20, 40, 60, 80, 100, 120 };
  const byte cmdCount = 7;
  const byte cmdCode = 192;

// ====== initialize wheel emulation

  const uint16_t ccMsgCoolDown = 20; // milliseconds between steps
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

// ====== initialize rotary knob

  #include <Rotary.h>
  const byte rotaryPinA = 20;
  const byte rotaryPinB = 21;
  const byte rotaryPinC = 24;
  Rotary rotary = Rotary(rotaryPinA, rotaryPinB);
  bool rotaryIsClicked = HIGH;          //
  bool rotaryWasClicked = HIGH;         //
  int8_t rotaryKnobTurns = 0;           //

// ====== initialize GFX display

  #include <GEM_u8g2.h>
  #define GEM_DISABLE_GLCD
  U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);
  GEM_u8g2 menu(u8g2, GEM_POINTER_ROW, GEM_ITEMS_COUNT_AUTO, 10, 10,
78); // menu item height; page screen top offset; menu values left offset
  const byte defaultContrast = 63;           // GFX default contrast
  bool screenSaverOn = 0;                    //
  uint32_t screenTime = 0;                   // GFX timer to count if screensaver should go on
  const uint32_t screenSaverMillis = 10000; //

// ====== initialize piezo buzzer

  //#include "RP2040_Volume.h"    // simulated volume control on buzzer
  const byte tonePin = 23;
  //RP2040_Volume piezoBuzzer(tonePin, tonePin);
  byte buzzer = 0;                      // buzzer state
  byte currentBuzzNote = 255;           // need to work on this
  uint32_t currentBuzzTime = 0;         // Used to keep track of when this note started buzzin
  uint32_t arpeggiateLength = 10;       //

// ====== initialize tuning (microtonal) presets

  typedef struct {
    char* name;
    byte cycleLength; // steps before repeat
    float stepSize;   // in cents, 100 = "normal" semitone.
  } tuningDef;
  enum {
    Twelve, Seventeen, Nineteen, TwentyTwo,
    TwentyFour, ThirtyOne, FortyOne, FiftyThree,
    SeventyTwo, BohlenPierce,
    CarlosA, CarlosB, CarlosG
  };
  tuningDef tuningOptions[] = {
    // replaces the idea of const byte EDO[] = { 12, 17, 19, 22, 24, 31, 41, 53, 72 };
    { (char*)"12 EDO",           12,  100.0 },
    { (char*)"17 EDO",           17, 1200.0 / 17 },
    { (char*)"19 EDO",           19, 1200.0 / 19 },
    { (char*)"22 EDO",           22, 1200.0 / 22 },
    { (char*)"24 EDO",           24,   50.0 },
    { (char*)"31 EDO",           31, 1200.0 / 31 },
    { (char*)"41 EDO",           41, 1200.0 / 41 },
    { (char*)"53 EDO",           53, 1200.0 / 53 },
    { (char*)"72 EDO",           72,  100.0 / 6 },
    { (char*)"Bohlen-Pierce",    13, 1901.955 / 13 }, //
    { (char*)"Carlos Alpha",      9, 77.965 }, //
    { (char*)"Carlos Beta",      11, 63.833 }, //
    { (char*)"Carlos Gamma",     20, 35.099 }
  };
  const byte tuningCount = sizeof(tuningOptions) / sizeof(tuningDef);

// ====== initialize layout patterns

  typedef struct {
    char* name;
    bool isPortrait;
    byte rootHex;
    int8_t acrossSteps;
    int8_t dnLeftSteps;
    byte tuning;
  } layoutDef;
  layoutDef layoutOptions[] = {
    { (char*)"Wicki-Hayden",      1, 64,   2,  -7, Twelve     },
    { (char*)"Harmonic Table",    0, 75,  -7,   3, Twelve     },
    { (char*)"Janko",             0, 65,  -1,  -1, Twelve     },
    { (char*)"Gerhard",           0, 65,  -1,  -3, Twelve     },
    { (char*)"Accordion C-sys.",  1, 75,   2,  -3, Twelve     },
    { (char*)"Accordion B-sys.",  1, 64,   1,  -3, Twelve     },
    { (char*)"Full Layout",       1, 65,  -1,  -9, Twelve     },
    { (char*)"Bosanquet, 17",     0, 65,  -2,  -1, Seventeen  },
    { (char*)"Full Layout",       1, 65,  -1,  -9, Seventeen  },
    { (char*)"Bosanquet, 19",     0, 65,  -1,  -2, Nineteen   },
    { (char*)"Full Layout",       1, 65,  -1,  -9, Nineteen   },
    { (char*)"Bosanquet, 22",     0, 65,  -3,  -1, TwentyTwo  },
    { (char*)"Full Layout",       1, 65,  -1,  -9, TwentyTwo  },
    { (char*)"Bosanquet, 24",     0, 65,  -1,  -3, TwentyFour },
    { (char*)"Full Layout",       1, 65,  -1,  -9, TwentyFour },
    { (char*)"Bosanquet, 31",     0, 65,  -2,  -3, ThirtyOne  },
    { (char*)"Full Layout",       1, 65,  -1,  -9, ThirtyOne  },
    { (char*)"Bosanquet, 41",     0, 65,  -4,  -3, FortyOne   },  // forty-one #1
    { (char*)"Gerhard, 41",       0, 65,   3, -10, FortyOne   },  // forty-one #2
    { (char*)"Full Layout, 41",   0, 65,  -1,  -8, FortyOne   },  // forty-one #3
    { (char*)"Wicki-Hayden, 53",  1, 64,   9, -31, FiftyThree },
    { (char*)"Harmonic Tbl, 53",  0, 75, -31,  14, FiftyThree },
    { (char*)"Bosanquet, 53",     0, 65,  -5,  -4, FiftyThree },
    { (char*)"Full Layout, 53",   0, 65,  -1,  -9, FiftyThree },
    { (char*)"Full Layout, 72",   0, 65,  -1,  -9, SeventyTwo },
    { (char*)"Full Layout",       1, 65,  -1,  -9, BohlenPierce },
    { (char*)"Full Layout",       1, 65,  -1,  -9, CarlosA    },
    { (char*)"Full Layout",       1, 65,  -1,  -9, CarlosB    },
    { (char*)"Full Layout",       1, 65,  -1,  -9, CarlosG    }
  };
  const byte layoutCount = sizeof(layoutOptions) / sizeof(layoutDef);

// ====== initialize list of supported scales / modes / raga / maqam

  typedef struct {
    char* name;
    byte tuning;
    byte step[16]; // 16 bytes = 128 bits, 1 = in scale; 0 = not
  } scaleDef;
  scaleDef scaleOptions[] = {
    { (char*)"None",              255,        { 255,        255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255} },
    { (char*)"Major",             Twelve,     { 0b10101101, 0b01010000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Minor, natural",    Twelve,     { 0b10110101, 0b10100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Minor, melodic",    Twelve,     { 0b10110101, 0b01010000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Minor, harmonic",   Twelve,     { 0b10110101, 0b10010000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Pentatonic, major", Twelve,     { 0b10101001, 0b01000000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Pentatonic, minor", Twelve,     { 0b10010101, 0b00100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Blues",             Twelve,     { 0b10010111, 0b00100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Double Harmonic",   Twelve,     { 0b11001101, 0b10010000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Phrygian",          Twelve,     { 0b11010101, 0b10100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Phrygian Dominant", Twelve,     { 0b11001101, 0b10100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Dorian",            Twelve,     { 0b10110101, 0b01100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Lydian",            Twelve,     { 0b10101011, 0b01010000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Lydian Dominant",   Twelve,     { 0b10101011, 0b01100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Mixolydian",        Twelve,     { 0b10101101, 0b01100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Locrian",           Twelve,     { 0b11010110, 0b10100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Whole tone",        Twelve,     { 0b10101010, 0b10100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Octatonic",         Twelve,     { 0b10110110, 0b11010000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Rast maqam",        TwentyFour, { 0b10001001, 0b00100010, 0b00101100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { (char*)"Rast makam",        FiftyThree, { 0b10000000, 0b01000000, 0b01000010, 0b00000001,
                                                0b00000000, 0b10001000, 0b10000000, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
  };
  const byte scaleCount = sizeof(scaleOptions) / sizeof(scaleDef);
  byte scaleLock = 0;    // menu wants this to be an int, not a bool

// ====== initialize key coloring routines

  enum colors      { W,    R,    O,    Y,    L,    G,    C,    B,    I,    P,    M,
                          r,    o,    y,    l,    g,    c,    b,    i,   p,    m     };
  enum { DARK = 0, VeryDIM = 1, DIM = 32, BRIGHT = 127, VeryBRIGHT = 255 };
  enum { GRAY = 0, DULL = 127, VIVID = 255 };
  float hueCode[] = { 0.0, 0.0, 36.0, 72.0, 108.0, 144.0, 180.0, 216.0, 252.0, 288.0, 324.0,
                           0.0, 36.0, 72.0, 108.0, 144.0, 180.0, 216.0, 252.0, 288.0, 324.0  };
  byte satCode[] = { GRAY, VIVID,VIVID,VIVID,VIVID,VIVID,VIVID,VIVID,VIVID,VIVID,VIVID,
                          DULL, DULL, DULL, DULL, DULL, DULL, DULL, DULL, DULL, DULL  };
  byte colorMode = 0;
  byte perceptual = 1;
  enum {assignDefault = -1}; // auto-determine this component of color

// ====== initialize note labels in each tuning, also used for key signature

  typedef struct {
    char* name;
    byte tuning;
    int8_t offset;      // steps from constant A4 to that key class
    colors tierColor;
  } keyDef;
  keyDef keyOptions[] = {
    // 12 EDO, whole tone = 2, #/b = 1
      { (char*)" C        (B#)", Twelve, -9, W },
      { (char*)" C# / Db",       Twelve, -8, I },
      { (char*)"      D",        Twelve, -7, W },
      { (char*)"      D# / Eb",  Twelve, -6, I },
      { (char*)"     (Fb)  E",   Twelve, -5, W },
      { (char*)"      F   (E#)", Twelve, -4, W },
      { (char*)" Gb / F#",       Twelve, -3, I },
      { (char*)" G",             Twelve, -2, W },
      { (char*)" G# / Ab",       Twelve, -1, I },
      { (char*)"      A",        Twelve,  0, W },
      { (char*)"      A# / Bb",  Twelve,  1, I },
      { (char*)"(Cb)       B",   Twelve,  2, W },
    // 17 EDO, whole tone = 3, #/b = 2, +/d = 1
      { (char*)" C        (B+)", Seventeen,  -13, W },
      { (char*)" C+ / Db / B#",  Seventeen,  -12, R },
      { (char*)" C# / Dd",       Seventeen,  -11, I },
      { (char*)"      D",        Seventeen,  -10, W },
      { (char*)"      D+ / Eb",  Seventeen,   -9, R },
      { (char*)" Fb / D# / Ed",  Seventeen,   -8, I },
      { (char*)"(Fd)       E",   Seventeen,   -7, W },
      { (char*)" F        (E+)", Seventeen,   -6, W },
      { (char*)" F+ / Gb / E#",  Seventeen,   -5, R },
      { (char*)" F# / Gd",       Seventeen,   -4, I },
      { (char*)"      G",        Seventeen,   -3, W },
      { (char*)"      G+ / Ab",  Seventeen,   -2, R },
      { (char*)"      G# / Ad",  Seventeen,   -1, I },
      { (char*)"           A",   Seventeen,    0, W },
      { (char*)"      Bb / A+",  Seventeen,    1, R },
      { (char*)" Cb / Bd / A#",  Seventeen,    2, I },
      { (char*)"(Cd)  B"      ,  Seventeen,    3, W },
    // 19 EDO, whole tone = 3, #/b = 1
      { (char*)" C",       Nineteen, -14, W },
      { (char*)" C#",      Nineteen, -13, R },
      { (char*)" Db",      Nineteen, -12, I },
      { (char*)" D",       Nineteen, -11, W },
      { (char*)" D#",      Nineteen, -10, R },
      { (char*)" Eb",      Nineteen,  -9, I },
      { (char*)" E",       Nineteen,  -8, W },
      { (char*)" E# / Fb", Nineteen,  -7, m },
      { (char*)"      F",  Nineteen,  -6, W },
      { (char*)"      F#", Nineteen,  -5, R },
      { (char*)"      Gb", Nineteen,  -4, I },
      { (char*)"      G",  Nineteen,  -3, W },
      { (char*)"      G#", Nineteen,  -2, R },
      { (char*)"      Ab", Nineteen,  -1, I },
      { (char*)"      A",  Nineteen,   0, W },
      { (char*)"      A#", Nineteen,   1, R },
      { (char*)"      Bb", Nineteen,   2, I },
      { (char*)"      B",  Nineteen,   3, W },
      { (char*)" Cb / B#", Nineteen,   4, m },
    // 22 EDO, whole tone = 4, #/b = 3, ^/v = 1
      { (char*)"  C         (^B)", TwentyTwo, -17, W },
      { (char*)" ^C  /  Db / vB#", TwentyTwo, -16, l },
      { (char*)" vC# / ^Db /  B#", TwentyTwo, -15, C },
      { (char*)"  C# / vD",        TwentyTwo, -14, i },
      { (char*)"        D",        TwentyTwo, -13, W },
      { (char*)"       ^D  /  Eb", TwentyTwo, -12, l },
      { (char*)"  Fb / vD# / ^Eb", TwentyTwo, -11, C },
      { (char*)" ^Fb /  D# / vE",  TwentyTwo, -10, i },
      { (char*)"(vF)          E",  TwentyTwo,  -9, W },
      { (char*)"  F         (^E)", TwentyTwo,  -8, W },
      { (char*)" ^F  /  Gb / vE#", TwentyTwo,  -7, l },
      { (char*)" vF# / ^Gb /  E#", TwentyTwo,  -6, C },
      { (char*)"  F# / vG",        TwentyTwo,  -5, i },
      { (char*)"        G",        TwentyTwo,  -4, W },
      { (char*)"       ^G  /  Ab", TwentyTwo,  -3, l },
      { (char*)"       vG# / ^Ab", TwentyTwo,  -2, C },
      { (char*)"        G# / vA",  TwentyTwo,  -1, i },
      { (char*)"              A",  TwentyTwo,   0, W },
      { (char*)"        Bb / ^A",  TwentyTwo,   1, l },
      { (char*)"  Cb / ^Bb / vA#", TwentyTwo,   2, C },
      { (char*)" ^Cb / vB  /  A#", TwentyTwo,   3, i },
      { (char*)"(vC)    B",        TwentyTwo,   4, W },
    // 24 EDO, whole tone = 4, #/b = 2, +/d = 1
      { (char*)" C  / B#", TwentyFour, -18, W },
      { (char*)" C+",      TwentyFour, -17, r },
      { (char*)" C# / Db", TwentyFour, -16, I },
      { (char*)"      Dd", TwentyFour, -15, g },
      { (char*)"      D",  TwentyFour, -14, W },
      { (char*)"      D+", TwentyFour, -13, r },
      { (char*)" Eb / D#", TwentyFour, -12, I },
      { (char*)" Ed",      TwentyFour, -11, g },
      { (char*)" E  / Fb", TwentyFour, -10, W },
      { (char*)" E+ / Fd", TwentyFour,  -9, y },
      { (char*)" E# / F",  TwentyFour,  -8, W },
      { (char*)"      F+", TwentyFour,  -7, r },
      { (char*)" Gb / F#", TwentyFour,  -6, I },
      { (char*)" Gd",      TwentyFour,  -5, g },
      { (char*)" G",       TwentyFour,  -4, W },
      { (char*)" G+",      TwentyFour,  -3, r },
      { (char*)" G# / Ab", TwentyFour,  -2, I },
      { (char*)"      Ad", TwentyFour,  -1, g },
      { (char*)"      A",  TwentyFour,   0, W },
      { (char*)"      A+", TwentyFour,   1, r },
      { (char*)" Bb / A#", TwentyFour,   2, I },
      { (char*)" Bd",      TwentyFour,   3, g },
      { (char*)" B  / Cb", TwentyFour,   4, W },
      { (char*)" B+ / Cd", TwentyFour,   5, y },
    // 31 EDO, whole tone = 5, #/b = 2, +/d = 1
      { (char*)" C",       ThirtyOne, -23, W },
      { (char*)" C+",      ThirtyOne, -22, R },
      { (char*)" C#",      ThirtyOne, -21, Y },
      { (char*)" Db",      ThirtyOne, -20, C },
      { (char*)" Dd",      ThirtyOne, -19, I },
      { (char*)" D",       ThirtyOne, -18, W },
      { (char*)" D+",      ThirtyOne, -17, R },
      { (char*)" D#",      ThirtyOne, -16, Y },
      { (char*)" Eb",      ThirtyOne, -15, C },
      { (char*)" Ed",      ThirtyOne, -14, I },
      { (char*)" E",       ThirtyOne, -13, W },
      { (char*)" E+ / Fb", ThirtyOne, -12, L },
      { (char*)" E# / Fd", ThirtyOne, -11, M },
      { (char*)"      F",  ThirtyOne, -10, W },
      { (char*)"      F+", ThirtyOne,  -9, R },
      { (char*)"      F#", ThirtyOne,  -8, Y },
      { (char*)"      Gb", ThirtyOne,  -7, C },
      { (char*)"      Gd", ThirtyOne,  -6, I },
      { (char*)"      G",  ThirtyOne,  -5, W },
      { (char*)"      G+", ThirtyOne,  -4, R },
      { (char*)"      G#", ThirtyOne,  -3, Y },
      { (char*)"      Ab", ThirtyOne,  -2, C },
      { (char*)"      Ad", ThirtyOne,  -1, I },
      { (char*)"      A",  ThirtyOne,   0, W },
      { (char*)"      A+", ThirtyOne,   1, R },
      { (char*)"      A#", ThirtyOne,   2, Y },
      { (char*)"      Bb", ThirtyOne,   3, C },
      { (char*)"      Bd", ThirtyOne,   4, I },
      { (char*)"      B",  ThirtyOne,   5, W },
      { (char*)" Cb / B+", ThirtyOne,   6, L },
      { (char*)" Cd / B#", ThirtyOne,   7, M },
    // 41 EDO, whole tone = 7, #/b = 4, +/d = 2, ^/v = 1
      { (char*)"  C         (vB#)", FortyOne, -31, W },
      { (char*)" ^C        /  B#",  FortyOne, -30, c },
      { (char*)"  C+ ",             FortyOne, -29, O },
      { (char*)" vC# /  Db",        FortyOne, -28, I },
      { (char*)"  C# / ^Db",        FortyOne, -27, R },
      { (char*)"        Dd",        FortyOne, -26, B },
      { (char*)"       vD",         FortyOne, -25, y },
      { (char*)"        D",         FortyOne, -24, W },
      { (char*)"       ^D",         FortyOne, -23, c },
      { (char*)"        D+",        FortyOne, -22, O },
      { (char*)"       vD# /  Eb",  FortyOne, -21, I },
      { (char*)"        D# / ^Eb",  FortyOne, -20, R },
      { (char*)"              Ed",  FortyOne, -19, B },
      { (char*)"             vE",   FortyOne, -18, y },
      { (char*)"      (^Fb)   E",   FortyOne, -17, W },
      { (char*)"        Fd / ^E",   FortyOne, -16, c },
      { (char*)"       vF  /  E+",  FortyOne, -15, y },
      { (char*)"        F   (vE#)", FortyOne, -14, W },
      { (char*)"       ^F  /  E#",  FortyOne, -13, c },
      { (char*)"        F+",        FortyOne, -12, O },
      { (char*)"  Gb / vF#",        FortyOne, -11, I },
      { (char*)" ^Gb /  F#",        FortyOne, -10, R },
      { (char*)"  Gd",              FortyOne,  -9, B },
      { (char*)" vG",               FortyOne,  -8, y },
      { (char*)"  G",               FortyOne,  -7, W },
      { (char*)" ^G",               FortyOne,  -6, c },
      { (char*)"  G+",              FortyOne,  -5, O },
      { (char*)" vG# /  Ab",        FortyOne,  -4, I },
      { (char*)"  G# / ^Ab",        FortyOne,  -3, R },
      { (char*)"        Ad",        FortyOne,  -2, B },
      { (char*)"       vA",         FortyOne,  -1, y },
      { (char*)"        A",         FortyOne,   0, W },
      { (char*)"       ^A",         FortyOne,   1, c },
      { (char*)"        A+",        FortyOne,   2, O },
      { (char*)"       vA# /  Bb",  FortyOne,   3, I },
      { (char*)"        A# / ^Bb",  FortyOne,   4, R },
      { (char*)"              Bd",  FortyOne,   5, B },
      { (char*)"             vB",   FortyOne,   6, y },
      { (char*)"      (^Cb)   B",   FortyOne,   7, W },
      { (char*)"        Cd / ^B",   FortyOne,   8, c },
      { (char*)"       vC  /  B+",  FortyOne,   9, y },
    // 53 EDO, whole tone = 9, #/b = 5, >/< = 2, ^/v = 1
      { (char*)"  C         (vB#)", FiftyThree, -40, W },
      { (char*)" ^C     /     B#",  FiftyThree, -39, c },
      { (char*)" >C  / <Db",        FiftyThree, -38, l },
      { (char*)" <C# / vDb",        FiftyThree, -37, O },
      { (char*)" vC# /  Db",        FiftyThree, -36, I },
      { (char*)"  C# / ^Db",        FiftyThree, -35, R },
      { (char*)" ^C# / >Db",        FiftyThree, -34, B },
      { (char*)" >C# / <D",         FiftyThree, -33, g },
      { (char*)"       vD",         FiftyThree, -32, y },
      { (char*)"        D",         FiftyThree, -31, W },
      { (char*)"       ^D",         FiftyThree, -30, c },
      { (char*)"       >D  / <Eb",  FiftyThree, -29, l },
      { (char*)"       <D# / vEb",  FiftyThree, -28, O },
      { (char*)"       vD# /  Eb",  FiftyThree, -27, I },
      { (char*)"        D# / ^Eb",  FiftyThree, -26, R },
      { (char*)"       ^D# / >Eb",  FiftyThree, -25, B },
      { (char*)"       >D# / <E",   FiftyThree, -24, g },
      { (char*)"  Fb    /    vE",   FiftyThree, -23, y },
      { (char*)"(^Fb)         E",   FiftyThree, -22, W },
      { (char*)"(>Fb)        ^E",   FiftyThree, -21, c },
      { (char*)" <F     /    >E",   FiftyThree, -20, G },
      { (char*)" vF         (<E#)", FiftyThree, -19, y },
      { (char*)"  F         (vE#)", FiftyThree, -18, W },
      { (char*)" ^F     /     E#",  FiftyThree, -17, c },
      { (char*)" >F  / <Gb",        FiftyThree, -16, l },
      { (char*)" <F# / vGb",        FiftyThree, -15, O },
      { (char*)" vF# /  Gb",        FiftyThree, -14, I },
      { (char*)"  F# / ^Gb",        FiftyThree, -13, R },
      { (char*)" ^F# / >Gb",        FiftyThree, -12, B },
      { (char*)" >F# / <G",         FiftyThree, -11, g },
      { (char*)"       vG",         FiftyThree, -10, y },
      { (char*)"        G",         FiftyThree,  -9, W },
      { (char*)"       ^G",         FiftyThree,  -8, c },
      { (char*)"       >G  / <Ab",  FiftyThree,  -7, l },
      { (char*)"       <G# / vAb",  FiftyThree,  -6, O },
      { (char*)"       vG# /  Ab",  FiftyThree,  -5, I },
      { (char*)"        G# / ^Ab",  FiftyThree,  -4, R },
      { (char*)"       ^G# / >Ab",  FiftyThree,  -3, B },
      { (char*)"       >G# / <A",   FiftyThree,  -2, g },
      { (char*)"             vA",   FiftyThree,  -1, y },
      { (char*)"              A",   FiftyThree,   0, W },
      { (char*)"             ^A",   FiftyThree,   1, c },
      { (char*)"       <Bb / >A",   FiftyThree,   2, l },
      { (char*)"       vBb / <A#",  FiftyThree,   3, O },
      { (char*)"        Bb / vA#",  FiftyThree,   4, I },
      { (char*)"       ^Bb /  A#",  FiftyThree,   5, R },
      { (char*)"       >Bb / ^A#",  FiftyThree,   6, B },
      { (char*)"       <B  / >A#",  FiftyThree,   7, g },
      { (char*)"  Cb / vB",         FiftyThree,   8, y },
      { (char*)"(^Cb)   B",         FiftyThree,   9, W },
      { (char*)"(>Cb)  ^B",         FiftyThree,  10, c },
      { (char*)" <C  / >B",         FiftyThree,  11, G },
      { (char*)" vC   (<B#)",       FiftyThree,  12, y },
    // 72 EDO, whole tone = 12, #/b = 6, +/d = 3, ^/v = 1
      { (char*)"  C    (B#)", SeventyTwo, -54, W },
      { (char*)" ^C",         SeventyTwo, -53, g },
      { (char*)" vC+",        SeventyTwo, -52, r },
      { (char*)"  C+",        SeventyTwo, -51, p },
      { (char*)" ^C+",        SeventyTwo, -50, b },
      { (char*)" vC#",        SeventyTwo, -49, y },
      { (char*)"  C# /  Db",  SeventyTwo, -48, I },
      { (char*)" ^C# / ^Db",  SeventyTwo, -47, g },
      { (char*)"       vDd",  SeventyTwo, -46, r },
      { (char*)"        Dd",  SeventyTwo, -45, p },
      { (char*)"       ^Dd",  SeventyTwo, -44, b },
      { (char*)"       vD",   SeventyTwo, -43, y },
      { (char*)"        D",   SeventyTwo, -42, W },
      { (char*)"       ^D",   SeventyTwo, -41, g },
      { (char*)"       vD+",  SeventyTwo, -40, r },
      { (char*)"        D+",  SeventyTwo, -39, p },
      { (char*)"       ^D+",  SeventyTwo, -38, b },
      { (char*)" vEb / vD#",  SeventyTwo, -37, y },
      { (char*)"  Eb /  D#",  SeventyTwo, -36, I },
      { (char*)" ^Eb / ^D#",  SeventyTwo, -35, g },
      { (char*)" vEd",        SeventyTwo, -34, r },
      { (char*)"  Ed",        SeventyTwo, -33, p },
      { (char*)" ^Ed",        SeventyTwo, -32, b },
      { (char*)" vE   (vFb)", SeventyTwo, -31, y },
      { (char*)"  E    (Fb)", SeventyTwo, -30, W },
      { (char*)" ^E   (^Fb)", SeventyTwo, -29, g },
      { (char*)" vE+ / vFd",  SeventyTwo, -28, r },
      { (char*)"  E+ /  Fd",  SeventyTwo, -27, p },
      { (char*)" ^E+ / ^Fd",  SeventyTwo, -26, b },
      { (char*)"(vE#)  vF",   SeventyTwo, -25, y },
      { (char*)" (E#)   F",   SeventyTwo, -24, W },
      { (char*)"(^E#)  ^F",   SeventyTwo, -23, g },
      { (char*)"       vF+",  SeventyTwo, -22, r },
      { (char*)"        F+",  SeventyTwo, -21, p },
      { (char*)"       ^F+",  SeventyTwo, -20, b },
      { (char*)" vGb / vF#",  SeventyTwo, -19, y },
      { (char*)"  Gb /  F#",  SeventyTwo, -18, I },
      { (char*)" ^Gb / ^F#",  SeventyTwo, -17, g },
      { (char*)" vGd",        SeventyTwo, -16, r },
      { (char*)"  Gd",        SeventyTwo, -15, p },
      { (char*)" ^Gd",        SeventyTwo, -14, b },
      { (char*)" vG",         SeventyTwo, -13, y },
      { (char*)"  G",         SeventyTwo, -12, W },
      { (char*)" ^G",         SeventyTwo, -11, g },
      { (char*)" vG+",        SeventyTwo, -10, r },
      { (char*)"  G+",        SeventyTwo,  -9, p },
      { (char*)" ^G+",        SeventyTwo,  -8, b },
      { (char*)" vG# / vAb",  SeventyTwo,  -7, y },
      { (char*)"  G# /  Ab",  SeventyTwo,  -6, I },
      { (char*)" ^G# / ^Ab",  SeventyTwo,  -5, g },
      { (char*)"       vAd",  SeventyTwo,  -4, r },
      { (char*)"        Ad",  SeventyTwo,  -3, p },
      { (char*)"       ^Ad",  SeventyTwo,  -2, b },
      { (char*)"       vA",   SeventyTwo,  -1, y },
      { (char*)"        A",   SeventyTwo,   0, W },
      { (char*)"       ^A",   SeventyTwo,   1, g },
      { (char*)"       vA+",  SeventyTwo,   2, r },
      { (char*)"        A+",  SeventyTwo,   3, p },
      { (char*)"       ^A+",  SeventyTwo,   4, b },
      { (char*)" vBb / vA#",  SeventyTwo,   5, y },
      { (char*)"  Bb /  A#",  SeventyTwo,   6, I },
      { (char*)" ^Bb / ^A#",  SeventyTwo,   7, g },
      { (char*)" vBd",        SeventyTwo,   8, r },
      { (char*)"  Bd",        SeventyTwo,   9, p },
      { (char*)" ^Bd",        SeventyTwo,  10, b },
      { (char*)" vB   (vCb)", SeventyTwo,  11, y },
      { (char*)"  B    (Cb)", SeventyTwo,  12, W },
      { (char*)" ^B   (^Cb)", SeventyTwo,  13, g },
      { (char*)" vB+ / vCd",  SeventyTwo,  14, r },
      { (char*)"  B+ /  Cd",  SeventyTwo,  15, p },
      { (char*)" ^B+ / ^Cd",  SeventyTwo,  16, b },
      { (char*)"(vB#)  vC",   SeventyTwo,  17, y },
    //
      { (char*)"n/a",BohlenPierce,0,W},
      { (char*)"n/a",BohlenPierce,0,W},
      { (char*)"n/a",BohlenPierce,0,W},
      { (char*)"n/a",BohlenPierce,0,W},
      { (char*)"n/a",BohlenPierce,0,W},
      { (char*)"n/a",BohlenPierce,0,W},
      { (char*)"n/a",BohlenPierce,0,W},
      { (char*)"n/a",BohlenPierce,0,W},
      { (char*)"n/a",BohlenPierce,0,W},
      { (char*)"n/a",BohlenPierce,0,W},
      { (char*)"n/a",BohlenPierce,0,W},
      { (char*)"n/a",BohlenPierce,0,W},
      { (char*)"n/a",BohlenPierce,0,W},
    //
      { (char*)"n/a",CarlosA,0,W},
      { (char*)"n/a",CarlosA,0,W},
      { (char*)"n/a",CarlosA,0,W},
      { (char*)"n/a",CarlosA,0,W},
      { (char*)"n/a",CarlosA,0,W},
      { (char*)"n/a",CarlosA,0,W},
      { (char*)"n/a",CarlosA,0,W},
      { (char*)"n/a",CarlosA,0,W},
      { (char*)"n/a",CarlosA,0,W},
    //
      { (char*)"n/a",CarlosB,0,W},
      { (char*)"n/a",CarlosB,0,W},
      { (char*)"n/a",CarlosB,0,W},
      { (char*)"n/a",CarlosB,0,W},
      { (char*)"n/a",CarlosB,0,W},
      { (char*)"n/a",CarlosB,0,W},
      { (char*)"n/a",CarlosB,0,W},
      { (char*)"n/a",CarlosB,0,W},
      { (char*)"n/a",CarlosB,0,W},
      { (char*)"n/a",CarlosB,0,W},
      { (char*)"n/a",CarlosB,0,W},
    //
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},
      { (char*)"n/a",CarlosG,0,W},

  };
  const int keyCount = sizeof(keyOptions) / sizeof(keyDef);
// ====== initialize structure to store and recall user preferences

  typedef struct { // put all user-selectable options into a class so that down the line these can be saved and loaded.
    char* presetName;
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
    (char*)"Default",
    Twelve,     // see the relevant enum{} statement
    0,          // default to the first layout, wicki hayden
    0,          // default to using no scale (chromatic)
    0,          // default to the key of C
    0           // default to no transposition
  };
// ====== functions

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
        || (c.row >= rowCount)
        || (c.col < 0)
        || (c.col >= (2 * colCount))
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
    return freqToMIDI(concertA) + ((float)stepsFromA * (float)current.tuning().stepSize / 100.0);
  }

// ====== diagnostic wrapper

  void sendToLog(String msg) {
    if (diagnostics) {
      Serial.println(msg);
    };
  }

// ====== LED routines

  int16_t transformHue(float D) {
    if ((!perceptual) || (D > 360.0)) {
      return 65536 * (D / 360.0);
    } else {
      //                red             yellow                 green            blue
      int hueIn[] =  {    0,    9,   18,   90,  108,  126,  135,  150,  198,  243,  252,  261,  306,  333,  360};
      //          #ff0000            #ffff00           #00ff00 #00ffff     #0000ff     #ff00ff
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
    for (byte i = 0; i < hexCount; i++) {
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
    for (byte i = 0; i < hexCount; i++) {
      if (!(h[i].isCmd)) {
        float N = stepsToMIDI(h[i].steps + current.key().offset + current.transpose);
        if (N < 0 || N >= 128) {
          h[i].note = 255;
          h[i].bend = 0;
          h[i].frequency = 0.0;
        } else {
          h[i].note = ((N >= 127) ? 127 : round(N));
          h[i].bend = (ldexp(N - h[i].note, 13) / defaultPBRange);
          h[i].frequency = MIDItoFreq(N);
        };
        sendToLog(String(
          "hex #" + String(i) + ", " +
          "steps=" + String(h[i].steps) + ", " +
          "isCmd? " + String(h[i].isCmd) + ", " +
          "note=" + String(h[i].note) + ", " +
          "bend=" + String(h[i].bend) + ", " +
          "freq=" + String(h[i].frequency) + ", " +
          "inScale? " + String(h[i].inScale) + "."
        ));
      };
    };
    sendToLog("assignPitches complete.");
  }
  void applyScale() {
    sendToLog("applyScale was called:");
    for (byte i = 0; i < hexCount; i++) {
      if (!(h[i].isCmd)) {
        byte degree = positiveMod(h[i].steps, current.tuning().cycleLength);
        byte whichByte = degree / 8;
        byte bitShift = 7 - (degree - (whichByte << 3));
        byte digitMask = 1 << bitShift;
        h[i].inScale = (current.scale().step[whichByte] & digitMask)
>> bitShift;
        sendToLog(String(
          "hex #" + String(i) + ", " +
          "steps=" + String(h[i].steps) + ", " +
          "isCmd? " + String(h[i].isCmd) + ", " +
          "note=" + String(h[i].note) + ", " +
          "inScale? " + String(h[i].inScale) + "."
        ));
      };
    };
    resetHexLEDs();
    sendToLog("applyScale complete.");
  }
  void applyLayout() {       // call this function when the layout changes
    sendToLog("buildLayout was called:");
    for (byte i = 0; i < hexCount; i++) {
      if (!(h[i].isCmd)) {
        coordinates dist = hexDistance(h[current.layout().rootHex].coords, h[i].coords);
        h[i].steps = (
          (dist.col * current.layout().acrossSteps) +
          (dist.row * (
            current.layout().acrossSteps +
            (2 * current.layout().dnLeftSteps)
          ))
        ) / 2;
        sendToLog(String(
          "hex #" + String(i) + ", " +
          "steps=" + String(h[i].steps) + "."
        ));
      };
    };
    applyScale();        // when layout changes, have to re-apply scale and re-apply LEDs
    assignPitches();     // same with pitches
    u8g2.setDisplayRotation(current.layout().isPortrait ? U8G2_R2 : U8G2_R1);     // and landscape / portrait rotation
    sendToLog("buildLayout complete.");
  }
// ====== buzzer routines

  byte nextHeldNote() {
    byte n = 255;
    for (byte i = 1; i < hexCount; i++) {
      byte checkNote = positiveMod(currentBuzzNote + i, hexCount);
      if ((h[checkNote].channel) && (!h[checkNote].isCmd)) {
        n = checkNote;
        break;
      };
    };
    return n;
  }
  void buzz(byte x) {        // send 128 or larger to turn off tone
    currentBuzzNote = x;
    if ((!(h[x].isCmd)) && (h[x].note < 128) && (h[x].frequency < 32767)) {
      //piezoBuzzer.tone(h[x].frequency, (float)velWheel.curValue * (100.0 / 128.0), 16384, TIME_MS);
      tone(tonePin, h[x].frequency);     // stock TONE library, but frequency changed to float
    } else {
      //piezoBuzzer.stop_tone();
      noTone(tonePin);                     // stock TONE library
    };
  }
// ====== MIDI routines

  void setPitchBendRange(byte Ch, byte semitones) {
    MIDI.beginRpn(0, Ch);
    MIDI.sendRpnValue(semitones << 7, Ch);
    MIDI.endRpn(Ch);
    sendToLog(String(
      "set pitch bend range on ch " +
      String(Ch) + " to be " + String(semitones) + " semitones"
    ));
  }
  void setMPEzone(byte masterCh, byte sizeOfZone) {
    MIDI.beginRpn(6, masterCh);
    MIDI.sendRpnValue(sizeOfZone << 7, masterCh);
    MIDI.endRpn(masterCh);
    sendToLog(String(
      "tried sending MIDI msg to set MPE zone, master ch " +
      String(masterCh) + ", zone of this size: " + String(sizeOfZone)
    ));
  }
  void prepMIDIforMicrotones() {
    bool makeZone = (MPE && (current.tuningIndex != Twelve)); // if MPE flag is on and tuning <> 12EDO
    setMPEzone(1, (8 * makeZone));   // MPE zone 1 = ch 2 thru 9 (or reset if not using MPE)
    delay(ccMsgCoolDown);
    setMPEzone(16, (5 * makeZone));  // MPE zone 16 = ch 11 thru 15 (or reset if not using MPE)
    delay(ccMsgCoolDown);
    for (byte i = 1; i <= 16; i++) {
      setPitchBendRange(i, defaultPBRange);  // some synths try to set PB range to 48 semitones.
      delay(ccMsgCoolDown);                  // this forces it back to the expected range of 2 semitones.
      if ((i != 10) && ((!makeZone) || ((i > 1) && (i < 16)))) {
        openChannelQueue.push(i);
        sendToLog(String("pushed ch " + String(i) + " to the open channel queue"));
      };
      channelBend[i - 1] = 0;
      channelPoly[i - 1] = 0;
    };
  }
  void chgModulation() {
  if (current.tuningIndex == Twelve) {
      MIDI.sendControlChange(1, modWheel.curValue, 1);
      sendToLog(String("sent mod value " + String(modWheel.curValue) + " to ch 1"));
    } else if (MPE) {
      MIDI.sendControlChange(1, modWheel.curValue, 1);
      sendToLog(String("sent mod value " + String(modWheel.curValue) + " to ch 1"));
      MIDI.sendControlChange(1, modWheel.curValue, 16);
      sendToLog(String("sent mod value " + String(modWheel.curValue) + " to ch 16"));
    } else {
      for (byte i = 0; i < 16; i++) {
        MIDI.sendControlChange(1, modWheel.curValue, i + 1);
        sendToLog(String("sent mod value " + String(modWheel.curValue) + " to ch " + String(i+1)));
      };
    };
  };
  void chgUniversalPB() {
    if (current.tuningIndex == Twelve) {
      MIDI.sendPitchBend(pbWheel.curValue, 1);
      sendToLog(String("sent pb value " + String(pbWheel.curValue) + " to ch 1"));
    } else if (MPE) {
      MIDI.sendPitchBend(pbWheel.curValue, 1);
      sendToLog(String("sent pb value " + String(pbWheel.curValue) + " to ch 1"));
      MIDI.sendPitchBend(pbWheel.curValue, 16);
      sendToLog(String("sent pb value " + String(pbWheel.curValue) + " to ch 16"));
    } else {
      for (byte i = 0; i < 16; i++) {
        MIDI.sendPitchBend(channelBend[i] + pbWheel.curValue, i + 1);
        sendToLog(String("sent pb value " + String(channelBend[i] + pbWheel.curValue) + " to ch " + String(i+1)));
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
          sendToLog(String("found a matching channel: ch " + String(temp) + " has pitch bend " + String(channelBend[c])));
          break;
        };
      };
      if (temp = 17) {
        if (openChannelQueue.empty()) {
          sendToLog(String("channel queue was empty so we didn't send a note on"));
        } else {
          temp = openChannelQueue.front();
          openChannelQueue.pop();
          sendToLog(String("popped " + String(temp) + " off the queue"));
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
        MIDI.sendPitchBend(h[x].bend, c); // ch 1-16
      };
      MIDI.sendNoteOn(h[x].note, velWheel.curValue, c); // ch 1-16
      sendToLog(String(
        "sent note on: " + String(h[x].note) +
        " pb " + String(h[x].bend) +
        " vel " + String(velWheel.curValue) +
        " ch " + String(c)
      ));
      if (current.tuningIndex != Twelve) {
        channelPoly[c - 1]++;   // array is 0 - 15
      };
      if (buzzer) {
        buzz(x);
      };
    };
  }
  void noteOff(byte x) {
    byte c = h[x].channel;
    if (c) {
      h[x].channel = 0;
      MIDI.sendNoteOff(h[x].note, velWheel.curValue, c);
      sendToLog(String(
        "sent note off: " + String(h[x].note) +
        " pb " + String(h[x].bend) +
        " vel " + String(velWheel.curValue) +
        " ch " + String(c)
      ));
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
      if (buzzer) {
        buzz(nextHeldNote());
      };
    };
  }
  void cmdOn(byte x) {   // volume and mod wheel read all current buttons
    switch (h[x].note) {
      case cmdCode + 3:
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
    for (byte i = 0; i < hexCount; i++) {                   // check every hex
      if ((!(h[i].isCmd)) && (h[i].channel)) {              // that is a held note
        for (byte j = 0; j < hexCount; j++) {               // compare to every hex
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
    for (byte i = 0; i < hexCount; i++) { // check every hex
      if ((!(h[i].isCmd)) && (h[i].channel)) { // that is a held note
        flagToAnimate(hexOffset(h[i].coords,hexVector((h[i].animFrame() % 6),1))); // different neighbor each frame
      };
    };
  }
  void animateRadial() {
    for (byte i = 0; i < hexCount; i++) {
 // check every hex
      if (!(h[i].isCmd)) {
  // that is a note
        uint32_t radius = h[i].animFrame();
        if ((radius > 0) && (radius < 16)) {
   // played in the last 16 frames
          byte steps = ((animationType == SplashAnim) ? radius : 1);
 // star = 1 step to next corner; ring = 1 step per hex
          coordinates temp =
hexOffset(h[i].coords,hexVector(DnLeft,radius));  // start at one corner of the ring
          for (byte dir = 0; dir < 6; dir++) {
 // walk along the ring in each of the 6 hex directions
            for (byte i = 0; i < steps; i++) {
 // # of steps to the next corner
              flagToAnimate(temp);
  // flag for animation
              temp = hexOffset(temp, hexVector(dir,radius / steps));
 // then next step
            };
          };
        };
      };
    };
  }

// ====== menu variables and routines

  // must declare these variables globally for some reason
  // doing so down here so we don't have to forward declare callback functions
  //
  SelectOptionByte optionByteYesOrNo[] = { { "No"     , 0 },
                                           { "Yes"    , 1 } };
  SelectOptionByte optionByteBuzzer[] =  { { "Off"    , 0 },
                                           { "Mono"   , 1 },
                                           { "Arp'gio", 2 } };
  SelectOptionByte optionByteColor[] =   { { "Rainbow", 0 },
                                           { "Tiered" , 1 } };
  SelectOptionByte optionByteAnimate[] = { { "None"   , NoAnim },
                                           { "Octave" , OctaveAnim},
                                           { "By Note", NoteAnim},
                                           { "Star"   , StarAnim},
                                           { "Splash" , SplashAnim},
                                           { "Orbit"  , OrbitAnim} };

  GEMSelect selectYesOrNo(sizeof(optionByteYesOrNo) / sizeof(SelectOptionByte), optionByteYesOrNo);
  GEMSelect selectBuzzer( sizeof(optionByteBuzzer)  / sizeof(SelectOptionByte), optionByteBuzzer);
  GEMSelect selectColor(  sizeof(optionByteColor)   / sizeof(SelectOptionByte), optionByteColor);
  GEMSelect selectAnimate(sizeof(optionByteAnimate) / sizeof(SelectOptionByte), optionByteAnimate);

  GEMPage  menuPageMain("HexBoard MIDI Controller");

  GEMPage  menuPageTuning("Tuning");
  GEMItem  menuGotoTuning("Tuning", menuPageTuning);
  GEMItem* menuItemTuning[tuningCount]; // dynamically generate item based on tunings

  GEMPage  menuPageLayout("Layout");
  GEMItem  menuGotoLayout("Layout", menuPageLayout);
  GEMItem* menuItemLayout[layoutCount]; // dynamically generate item based on presets

  GEMPage  menuPageScales("Scales");
  GEMItem  menuGotoScales("Scales", menuPageScales);
  GEMItem* menuItemScales[scaleCount];  // dynamically generate item based on presets and if allowed in given EDO tuning

  GEMPage  menuPageKeys("Keys");
  GEMItem  menuGotoKeys("Keys",     menuPageKeys);
  GEMItem* menuItemKeys[keyCount];   // dynamically generate item based on presets

  GEMItem  menuItemScaleLock( "Scale lock?",   scaleLock,     selectYesOrNo);
  GEMItem  menuItemMPE(       "MPE Mode:",     MPE,
selectYesOrNo, prepMIDIforMicrotones);
  GEMItem  menuItemBuzzer(    "Buzzer:",       buzzer,        selectBuzzer);
  GEMItem  menuItemColor(     "Color mode:",   colorMode,
selectColor,   resetHexLEDs);
  GEMItem  menuItemPercep(    "Adjust color:", perceptual,
selectYesOrNo, resetHexLEDs);
  GEMItem  menuItemAnimate(   "Animation:",    animationType, selectAnimate);

  void menuHome() {
    menu.setMenuPageCurrent(menuPageMain);
    menu.drawMenu();
  }
  void showOnlyValidLayoutChoices() { // re-run at setup and whenever tuning changes
    for (byte L = 0; L < layoutCount; L++) {
      menuItemLayout[L]->hide((layoutOptions[L].tuning != current.tuningIndex));
    };
    sendToLog(String("menu: Layout choices were updated."));
  }
  void showOnlyValidScaleChoices() { // re-run at setup and whenever tuning changes
    for (int S = 0; S < scaleCount; S++) {
      menuItemScales[S]->hide((scaleOptions[S].tuning != current.tuningIndex) && (scaleOptions[S].tuning != 255));
    };
    sendToLog(String("menu: Scale choices were updated."));
  }
  void showOnlyValidKeyChoices() { // re-run at setup and whenever tuning changes
    for (int K = 0; K < keyCount; K++) {
      menuItemKeys[K]->hide((keyOptions[K].tuning != current.tuningIndex));
    };
    sendToLog(String("menu: Key choices were updated."));
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
      menuItemTuning[T] = new GEMItem(tuningOptions[T].name, changeTuning, T);
      menuPageTuning.addMenuItem(*menuItemTuning[T]);
    };

    menuPageMain.addMenuItem(menuGotoLayout);
    for (byte L = 0; L < layoutCount; L++) { // create pointers to all layouts
      menuItemLayout[L] = new GEMItem(layoutOptions[L].name, changeLayout, L);
      menuPageLayout.addMenuItem(*menuItemLayout[L]);
    };
    showOnlyValidLayoutChoices();

    menuPageMain.addMenuItem(menuGotoScales);
    for (int S = 0; S < scaleCount; S++) {  // create pointers to all scale items, filter them as you go
      menuItemScales[S] = new GEMItem(scaleOptions[S].name, changeScale, S);
      menuPageScales.addMenuItem(*menuItemScales[S]);
    };
    showOnlyValidScaleChoices();

    menuPageMain.addMenuItem(menuGotoKeys);
    for (int K = 0; K < keyCount; K++) {
      menuItemKeys[K] = new GEMItem(keyOptions[K].name, changeKey, K);
      menuPageKeys.addMenuItem(*menuItemKeys[K]);
    };

    showOnlyValidKeyChoices();

    menuPageMain.addMenuItem(menuItemScaleLock);
    menuPageMain.addMenuItem(menuItemColor);
    menuPageMain.addMenuItem(menuItemBuzzer);
    menuPageMain.addMenuItem(menuItemAnimate);

    menuPageMain.addMenuItem(menuItemMPE);
    menuPageMain.addMenuItem(menuItemPercep);

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
      Serial.println("An Error has occurred while mounting LittleFS");
    }
  }
  void setupPins() {
    for (byte p = 0; p < sizeof(columnPins); p++)  // For each column pin...
    {
      pinMode(columnPins[p], INPUT_PULLUP);  // set the pinMode to INPUT_PULLUP (+3.3V / HIGH).
    }
    for (byte p = 0; p < sizeof(multiplexPins); p++)  // For each column pin...
    {
      pinMode(multiplexPins[p], OUTPUT);  // Setting the row multiplexer pins to output.
    }
    Wire.setSDA(lightPinSDA);
    Wire.setSCL(lightPinSCL);
    pinMode(rotaryPinC, INPUT_PULLUP);
  }
  void setupGrid() {
    sendToLog(String("initializing hex grid..."));
    for (byte i = 0; i < hexCount; i++) {
      h[i].coords = indexToCoord(i);
      h[i].isCmd = 0;
      h[i].note = 255;
      h[i].keyState = 0;
    };
    for (byte c = 0; c < cmdCount; c++) {
      h[assignCmd[c]].isCmd = 1;
      h[assignCmd[c]].note = cmdCode + c;
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
    sendToLog(String("theHDM was here"));
  }

// ====== loop routines

  void timeTracker() {
    lapTime = runTime - loopTime;
    // sendToLog(String(lapTime));  // Print out the time it takes to run each loop
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
    for (byte r = 0; r < rowCount; r++) {  // Iterate through each of the row pins on the multiplexing chip.
      for (byte d = 0; d < 4; d++) {
        digitalWrite(multiplexPins[d], (r >> d) & 1);
      }
      for (byte c = 0; c < colCount; c++) {   // Now iterate through each of the column pins that are connected to the current row pin.
        byte p = columnPins[c];               // Hold the currently selected column pin in a variable.
        pinMode(p, INPUT_PULLUP);             // Set that row pin to INPUT_PULLUP mode (+3.3V / HIGH).
        delayMicroseconds(10);                // Delay to give the pin modes time to change state (false readings are caused otherwise).
        bool didYouPressHex = (digitalRead(p) == LOW);  // hex is pressed if it returns LOW. else not pressed
        h[c + (r * colCount)].updateKeyState(didYouPressHex);
        pinMode(p, INPUT);  // Set the selected column pin back to INPUT mode (0V / LOW).
       }
    }
  }
  void actionHexes() {
    for (byte i = 0; i < hexCount; i++) {   // For all buttons in the deck
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
    if (buzzer > 1) {
      if (runTime - currentBuzzTime > arpeggiateLength) {
        currentBuzzTime = millis();
        byte nextNoteToBuzz = nextHeldNote();
        if (nextNoteToBuzz < cmdCode) {
          buzz(nextNoteToBuzz);
        };
      };
    };
  }
  void updateWheels() {
    velWheel.setTargetValue();
    bool upd = velWheel.updateValue(); // this function returns a boolean, gotta put it somewhere even if it isn't being used
    if (upd) {
      sendToLog(String("vel became " + String(velWheel.curValue)));
    }
    if (toggleWheel) {
      pbWheel.setTargetValue();
      upd = pbWheel.updateValue();
      if (upd) {
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
    for (byte i = 0; i < hexCount; i++) {
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
  byte byteLerp(byte xOne, byte xTwo, float yOne, float yTwo, float y) {
    float weight = (y - yOne) / (yTwo - yOne);
    int temp = xOne + ((xTwo - xOne) * weight);
    if (temp < xOne) {temp = xOne;};
    if (temp > xTwo) {temp = xTwo;};
    return temp;
  }
  void lightUpLEDs() {
    for (byte i = 0; i < hexCount; i++) {
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
        transformHue(0),satP * (pbWheel.curValue > 0),satP *
(pbWheel.curValue > 0)
      )));
      strip.setPixelColor(assignCmd[5],strip.gamma32(strip.ColorHSV(
        hueP,satP,255
      )));
      strip.setPixelColor(assignCmd[6],strip.gamma32(strip.ColorHSV(
        transformHue(180),satP * (pbWheel.curValue < 0),satP *
(pbWheel.curValue < 0)
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
        hueM,satM,((modWheel.curValue > 63) ? 127 + (satM / 2) : 127 -
(satM / 2))
      )));
      strip.setPixelColor(assignCmd[6],strip.gamma32(strip.ColorHSV(
        hueM,satM,127 + (satM / 2)
      )));
    };
    strip.show();
  }
  void dealWithRotary() {
    if (menu.readyForKey()) {
      rotaryIsClicked = digitalRead(rotaryPinC);
      if (rotaryIsClicked > rotaryWasClicked) {
        menu.registerKeyPress(GEM_KEY_OK);
        screenTime = 0;
      }
      rotaryWasClicked = rotaryIsClicked;
      if (rotaryKnobTurns != 0) {
        for (byte i = 0; i < abs(rotaryKnobTurns); i++) {
          menu.registerKeyPress(rotaryKnobTurns < 0 ? GEM_KEY_UP :
GEM_KEY_DOWN);
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
  }

// ====== setup() and loop()

  void setup() {
  #if (defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040))
    TinyUSB_Device_Init(0);  // Manual begin() is required on core
without built-in support for TinyUSB such as mbed rp2040
  #endif
    setupMIDI();
    setupFileSystem();
    setupPins();
    testDiagnostics();  // Print diagnostic troubleshooting information to serial monitor
    setupGrid();
    setupLEDs();
    setupGFX();
    setupMenu();
    for (byte i = 0; i < 5 && !TinyUSBDevice.mounted(); i++) {
      delay(1);  // wait until device mounted, maybe
    };
  }
  void setup1() {
    //
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
