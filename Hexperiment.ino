// ====== Hexperiment v1.2
  // Copyright 2022-2023 Jared DeCook and Zach DeCook
  // with help from Nicholas Fox
  // Hardware Information:
  // https://github.com/earlephilhower/arduino-pico
  // Generic RP2040 running at 133MHz with 16MB of flash
  // Licensed under the GNU GPL Version 3.
  // (Additional boards manager URL: https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json)
  // Tools > USB Stack > (Adafruit TinyUSB)
  // Sketch > Export Compiled Binary
  //
  // Brilliant resource for dealing with hexagonal coordinates. https://www.redblobgames.com/grids/hexagons/
  // Used this to get my hexagonal animations sorted. http://ondras.github.io/rot.js/manual/#hex/indexing
  // Menu library documentation https://github.com/Spirik/GEM
  //
  // Patches needed for U8G2, Rotary.h
  //
  // Wishlist:
  //  * multiple palettes per tuning
  //  * customize wheel placement and order
  //  *
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
  #define LED_PIN 22
  #define ROT_PIN_A 20
  #define ROT_PIN_B 21
  #define ROT_PIN_C 24
  #define MPLEX_1_PIN 4
  #define MPLEX_2_PIN 5
  #define MPLEX_4_PIN 2
  #define MPLEX_8_PIN 3
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
  #define TONEPIN 23

  // grid related
  #define LED_COUNT 140
  #define COLCOUNT 10
  #define ROWCOUNT 14

  #define HEX_DIRECTION_EAST 0
  #define HEX_DIRECTION_NE   1
  #define HEX_DIRECTION_NW   2
  #define HEX_DIRECTION_WEST 3
  #define HEX_DIRECTION_SW   4
  #define HEX_DIRECTION_SE   5

  #define CMDBTN_0 0
  #define CMDBTN_1 20
  #define CMDBTN_2 40
  #define CMDBTN_3 60
  #define CMDBTN_4 80
  #define CMDBTN_5 100
  #define CMDBTN_6 120
  #define CMDCOUNT 7

  // microtonal related
  #define TUNINGCOUNT 13

  #define TUNING_12EDO 0
  #define TUNING_17EDO 1
  #define TUNING_19EDO 2
  #define TUNING_22EDO 3
  #define TUNING_24EDO 4
  #define TUNING_31EDO 5
  #define TUNING_41EDO 6
  #define TUNING_53EDO 7
  #define TUNING_72EDO 8
  #define TUNING_BP 9
  #define TUNING_ALPHA 10
  #define TUNING_BETA 11
  #define TUNING_GAMMA 12     

  #define MAX_SCALE_DIVISIONS 72
  #define ALL_TUNINGS 255

  // MIDI-related
  #define CONCERT_A_HZ 440.0
  #define PITCH_BEND_SEMIS 2
  #define CMDB 192
  #define UNUSED_NOTE 255
  #define CC_MSG_COOLDOWN_MICROSECONDS 32768

  // buzzer related
  #define TONE_SL 3
  #define TONE_CH 1
  #define WAVEFORM_SINE 0
  #define WAVEFORM_STRINGS 1
  #define WAVEFORM_CLARINET 2
  #define WAVEFORM_SQUARE 8
  #define WAVEFORM_SAW 9
  #define WAVEFORM_TRIANGLE 10 
  #define POLL_INTERVAL_IN_MICROSECONDS 24
  // buzzer polyphony limit should be 8.
  // MIDI should be independent and use MPE logic.
  #define POLYPHONY_LIMIT 8
  #define ALARM_NUM 2
  #define ALARM_IRQ TIMER_IRQ_2
  #define BUZZ_OFF 0
  #define BUZZ_MONO 1
  #define BUZZ_ARPEGGIO 2
  #define BUZZ_POLY 3

  // LED related

  // value / brightness ranges from 0..255
  // black = 0, full strength = 255
  #define BRIGHT_MAX 255
  #define BRIGHT_HIGH 192
  #define BRIGHT_MID 168
  #define BRIGHT_LOW 144
  #define BRIGHT_DIM 108


  #define VALUE_BLACK 0
  #define VALUE_LOW   127
  #define VALUE_SHADE 170
  #define VALUE_NORMAL 192
  #define VALUE_FULL  255

  // saturation ranges from 0..255
  // 0 = black and white
  // 255 = full chroma

  #define SAT_BW 0
  #define SAT_TINT 32
  #define SAT_DULL 85
  #define SAT_MODERATE 170
  #define SAT_VIVID 255

  // hue is an angle from 0.0 to 359.9
  // there is a transform function to map "perceptual"
  // hues to RGB. the hue values below are perceptual.
  #define HUE_NONE 0.0
  #define HUE_RED 0.0
  #define HUE_ORANGE 36.0
  #define HUE_YELLOW 72.0
  #define HUE_LIME 108.0
  #define HUE_GREEN 144.0
  #define HUE_CYAN 180.0
  #define HUE_BLUE 216.0
  #define HUE_INDIGO 252.0
  #define HUE_PURPLE 288.0
  #define HUE_MAGENTA 324.0

  #define RAINBOW_MODE 0
  #define TIERED_COLOR_MODE 1
  #define ALTERNATE_COLOR_MODE 2

  // animations
  #define ANIMATE_NONE 0
  #define ANIMATE_STAR 1 
  #define ANIMATE_SPLASH 2 
  #define ANIMATE_ORBIT 3 
  #define ANIMATE_OCTAVE 4 
  #define ANIMATE_BY_NOTE 5

  // menu-related
  #define MENU_ITEM_HEIGHT 10
  #define MENU_PAGE_SCREEN_TOP_OFFSET 10
  #define MENU_VALUES_LEFT_OFFSET 78

  // debug
  #define DIAGNOSTIC_OFF 0
  #define DIAGNOSTIC_ON 1  

  // class definitions are in a header so that
  // they get read before compiling the main program.

  class tuningDef {
  public:
    std::string name; // limit is 17 characters for GEM menu
    byte cycleLength; // steps before period/cycle/octave repeats
    float stepSize;   // in cents, 100 = "normal" semitone.
    SelectOptionInt keyChoices[MAX_SCALE_DIVISIONS];
    int spanCtoA() {
      return keyChoices[0].val_int;
    }
  };

  class layoutDef {
  public:
    std::string name;    // limit is 17 characters for GEM menu
    bool isPortrait;     // affects orientation of the GEM menu only.
    byte hexMiddleC;     // instead of "what note is button 1", "what button is the middle"
    int8_t acrossSteps;  // defined this way to be compatible with original v1.1 firmare
    int8_t dnLeftSteps;  // defined this way to be compatible with original v1.1 firmare
    byte tuning;         // index of the tuning that this layout is designed for
  };

  class colorDef {
  public:
    float hue;
    byte sat;
    byte val;
    colorDef tint() {
      colorDef temp;
      temp.hue = this->hue;
      temp.sat = ((this->sat > SAT_TINT) ? SAT_TINT : this->sat);
      temp.val = VALUE_FULL;
      return temp;
    }
    colorDef shade() {
      colorDef temp;
      temp.hue = this->hue;
      temp.sat = ((this->sat > SAT_TINT) ? SAT_TINT : this->sat);
      temp.val = VALUE_LOW;
      return temp;
    }
  };

  class paletteDef {
  public:
    colorDef swatch[MAX_SCALE_DIVISIONS]; // the different colors used in this palette
    byte colorNum[MAX_SCALE_DIVISIONS];   // map key (c,d...) to swatches
    colorDef getColor(byte givenStepFromC) {
      return swatch[colorNum[givenStepFromC] - 1];
    }
    float getHue(byte givenStepFromC) {
      return getColor(givenStepFromC).hue;
    }
    byte getSat(byte givenStepFromC) {
      return getColor(givenStepFromC).sat;
    }
    byte getVal(byte givenStepFromC) {
      return getColor(givenStepFromC).val;
    }
  };

  class buttonDef {
  public:
    byte     btnState = 0;        // binary 00 = off, 01 = just pressed, 10 = just released, 11 = held
    void interpBtnPress(bool isPress) {
      btnState = (((btnState << 1) + isPress) & 3);
    }
    int8_t   coordRow = 0;        // hex coordinates
    int8_t   coordCol = 0;        // hex coordinates
    uint32_t timePressed = 0;     // timecode of last press
    uint32_t LEDcodeAnim = 0;    // calculate it once and store value, to make LED playback snappier 
    uint32_t LEDcodePlay = 0;    // calculate it once and store value, to make LED playback snappier
    uint32_t LEDcodeRest = 0;      // calculate it once and store value, to make LED playback snappier
    uint32_t LEDcodeOff = 0;     // calculate it once and store value, to make LED playback snappier
    uint32_t LEDcodeDim = 0;     // calculate it once and store value, to make LED playback snappier
    bool     animate = 0;         // hex is flagged as part of the animation in this frame, helps make animations smoother
    int16_t  stepsFromC = 0;      // number of steps from C4 (semitones in 12EDO; microtones if >12EDO)
    bool     isCmd = 0;           // 0 if it's a MIDI note; 1 if it's a MIDI control cmd
    bool     inScale = 0;         // 0 if it's not in the selected scale; 1 if it is
    byte     note = UNUSED_NOTE;  // MIDI note or control parameter corresponding to this hex
    int16_t  bend = 0;            // in microtonal mode, the pitch bend for this note needed to be tuned correctly
    byte     MIDIch = 0;          // what MIDI channel this note is playing on
    byte     buzzCh = 0;          // what buzzer ch this is playing on
    float    frequency = 0.0;     // what frequency to ring on the buzzer
  };

  class wheelDef {
  public:
    bool alternateMode; // two ways to control
    bool isSticky;      // TRUE if you leave value unchanged when no buttons pressed
    byte* topBtn;       // pointer to the key Status of the button you use as this button
    byte* midBtn;
    byte* botBtn;
    int16_t minValue;
    int16_t maxValue;
    int* stepValue; // this can be changed via GEM menu
    int16_t defValue; // snapback value
    int16_t curValue;
    int16_t targetValue;
    uint32_t timeLastChanged;
    void setTargetValue() {
      if (alternateMode) {
        if (*midBtn >> 1) { // middle button toggles target (0) vs. step (1) mode
          int16_t temp = curValue;
              if (*topBtn == 1)     {temp += *stepValue;} // tap button
              if (*botBtn == 1)     {temp -= *stepValue;} // tap button
              if (temp > maxValue)  {temp  = maxValue;} 
          else if (temp <= minValue) {temp  = minValue;}
          targetValue = temp;
        } else {
          switch (((*topBtn >> 1) << 1) + (*botBtn >> 1)) {
            case 0b10:   targetValue = maxValue;     break;
            case 0b11:   targetValue = defValue;     break;
            case 0b01:   targetValue = minValue;     break;
            default:     targetValue = curValue;     break;
          }
        }
      } else {
        switch (((*topBtn >> 1) << 2) + ((*midBtn >> 1) << 1) + (*botBtn >> 1)) {
          case 0b100:  targetValue = maxValue;                         break;
          case 0b110:  targetValue = (3 * maxValue + minValue) / 4;    break;
          case 0b010:
          case 0b111:
          case 0b101:  targetValue = (maxValue + minValue) / 2;        break;
          case 0b011:  targetValue = (maxValue + 3 * minValue) / 4;    break;
          case 0b001:  targetValue = minValue;                         break;
          case 0b000:  targetValue = (isSticky ? curValue : defValue); break;
          default: break;
        }
      }
    }
    bool updateValue(uint32_t givenTime) {
      int16_t temp = targetValue - curValue;
      if (temp != 0) {
        if ((givenTime - timeLastChanged) >= CC_MSG_COOLDOWN_MICROSECONDS ) {
          timeLastChanged = givenTime;
          if (abs(temp) < *stepValue) {
            curValue = targetValue;
          } else {
            curValue = curValue + (*stepValue * (temp / abs(temp)));
          }
          return 1;
        } else {
          return 0;
        }
      } else {
        return 0;
      }
    }   
  };

  class scaleDef {
  public:
    std::string name;
    byte tuning;
    byte pattern[MAX_SCALE_DIVISIONS];
  };

  // this class should only be touched by the 2nd core
  class oscillator {
  public:
    uint16_t increment = 0;
    uint16_t counter = 0;
    byte eq = 0;
  };

  // 1/8192 of a whole tone pitch bend accuracy ~ 0.025 cents.
  // over 128 possible notes, error shd be less than 0.0002 cents to avoid drift.
  // expressing cents to 6 sig figs should be sufficient.
  // notation -- comma delimited string.
  // first entry should be the label for A=440. 
  // last entry should be C, i.e. the "home key".
  // the rest of the scale C thru G will be spelled using the same pattern.
  // the number of commas is used to count where A and C are located in step space.
  tuningDef tuningOptions[] = {
    { "12 EDO", 12, 100.000, 
      {{"C" ,-9},{"C#",-8},{"D" ,-7},{"Eb",-6},{"E" ,-5},{"F",-4}
      ,{"F#",-3},{"G" ,-2},{"G#",-1},{"A" , 0},{"Bb", 1},{"B", 2}
    }},
    { "17 EDO", 17, 70.5882, 
      {{"C",-13},{"Db",-12},{"C#",-11},{"D",-10},{"Eb",-9},{"D#",-8}
      ,{"E", -7},{"F" , -6},{"Gb", -5},{"F#",-4},{"G", -3},{"Ab",-2}
      ,{"G#",-1},{"A" ,  0},{"Bb",  1},{"A#", 2},{"B",  3}
    }},
    { "19 EDO", 19, 63.1579, 
      {{"C" ,-14},{"C#",-13},{"Db",-12},{"D",-11},{"D#",-10},{"Eb",-9},{"E",-8}
      ,{"E#", -7},{"F" , -6},{"F#", -5},{"Gb",-4},{"G",  -3},{"G#",-2}
      ,{"Ab", -1},{"A" ,  0},{"A#",  1},{"Bb", 2},{"B",   3},{"Cb", 4}
    }},  
    { "22 EDO", 22, 54.5455, 
      {{" C", -17},{"^C",-16},{"vC#",-15},{"vD",-14},{" D",-13},{"^D",-12}
      ,{"^Eb",-11},{"vE",-10},{" E",  -9},{" F", -8},{"^F", -7},{"vF#",-6}
      ,{"vG",  -5},{" G", -4},{"^G",  -3},{"vG#",-2},{"vA", -1},{" A",  0}
      ,{"^A",   1},{"^Bb", 2},{"vB",   3},{" B",  4}
    }},
    { "24 EDO", 24, 50.0000, 
      {{"C", -18},{"C+",-17},{"C#",-16},{"Dd",-15},{"D",-14},{"D+",-13}
      ,{"Eb",-12},{"Ed",-11},{"E", -10},{"E+", -9},{"F", -8},{"F+", -7}
      ,{"F#", -6},{"Gd", -5},{"G",  -4},{"G+", -3},{"G#",-2},{"Ad", -1}
      ,{"A",   0},{"A+",  1},{"Bb",  2},{"Bd",  3},{"B",  4},{"Cd",  5}
    }},
    { "31 EDO", 31, 38.7097, 
      {{"C",-23},{"C+",-22},{"C#",-21},{"Db",-20},{"Dd",-19}
      ,{"D",-18},{"D+",-17},{"D#",-16},{"Eb",-15},{"Ed",-14}
      ,{"E",-13},{"E+",-12}                      ,{"Fd",-11}
      ,{"F",-10},{"F+", -9},{"F#", -8},{"Gb", -7},{"Gd", -6}
      ,{"G", -5},{"G+", -4},{"G#", -3},{"Ab", -2},{"Ad", -1}
      ,{"A",  0},{"A+",  1},{"A#",  2},{"Bb",  3},{"Bd",  4}
      ,{"B",  5},{"B+",  6}                      ,{"Cd",  7}
    }},
    { "41 EDO", 41, 29.2683, 
      {{" C",-31},{"^C",-30},{" C+",-29},{" Db",-28},{" C#",-27},{" Dd",-26},{"vD",-24}
      ,{" D",-24},{"^D",-23},{" D+",-22},{" Eb",-21},{" D#",-20},{" Ed",-19},{"vE",-18}
      ,{" E",-17},{"^E",-16}                                                ,{"vF",-15}
      ,{" F",-14},{"^F",-13},{" F+",-12},{" Gb",-11},{" F#",-10},{" Gd", -9},{"vG", -8}
      ,{" G", -7},{"^G", -6},{" G+", -5},{" Ab", -4},{" G#", -3},{" Ad", -2},{"vA", -1}
      ,{" A",  0},{"^A",  1},{" A+",  2},{" Bb",  3},{" A#",  4},{" Bd",  5},{"vB",  6}
      ,{" B",  7},{"^B",  8}                                                ,{"vC",  9}
    }},
    { "53 EDO", 53, 22.6415, 
      {{" C", -40},{"^C", -39},{">C",-38},{"vDb",-37},{"Db",-36}
      ,{" C#",-35},{"^C#",-34},{"<D",-33},{"vD", -32}
      ,{" D", -31},{"^D", -30},{">D",-29},{"vEb",-28},{"Eb",-27}
      ,{" D#",-26},{"^D#",-25},{"<E",-24},{"vE", -23}
      ,{" E", -22},{"^E", -21},{">E",-20},{"vF", -19}
      ,{" F", -18},{"^F", -17},{">F",-16},{"vGb",-15},{"Gb",-14}
      ,{" F#",-13},{"^F#",-12},{"<G",-11},{"vG", -10}
      ,{" G",  -9},{"^G",  -8},{">G", -7},{"vAb", -6},{"Ab", -5}
      ,{" G#", -4},{"^G#", -3},{"<A", -2},{"vA",  -1}
      ,{" A",   0},{"^A",   1},{">A",  2},{"vBb",  3},{"Bb",  4}
      ,{" A#",  5},{"^A#",  6},{"<B",  7},{"vB",   8}
      ,{" B",   9},{"^B",  10},{"<C", 11},{"vC",  12}
    }},
    { "72 EDO", 72, 16.6667, 
      {{" C", -54},{"^C", -53},{">C", -52},{" C+",-51},{"<C#",-50},{"vC#",-49}
      ,{" C#",-48},{"^C#",-47},{">C#",-46},{" Dd",-45},{"<D" ,-44},{"vD" ,-43}
      ,{" D", -42},{"^D", -41},{">D", -40},{" D+",-39},{"<Eb",-38},{"vEb",-37}
      ,{" Eb",-36},{"^Eb",-35},{">Eb",-34},{" Ed",-33},{"<E" ,-32},{"vE" ,-31}
      ,{" E", -30},{"^E", -29},{">E", -28},{" E+",-27},{"<F" ,-26},{"vF" ,-25}
      ,{" F", -24},{"^F", -23},{">F", -22},{" F+",-21},{"<F#",-20},{"vF#",-19}
      ,{" F#",-18},{"^F#",-17},{">F#",-16},{" Gd",-15},{"<G" ,-14},{"vG" ,-13}
      ,{" G", -12},{"^G", -11},{">G", -10},{" G+", -9},{"<G#", -8},{"vG#", -7}
      ,{" G#", -6},{"^G#", -5},{">G#", -4},{" Ad", -3},{"<A" , -2},{"vA" , -1}
      ,{" A",   0},{"^A",   1},{">A",   2},{" A+",  3},{"<Bb",  4},{"vBb",  5}
      ,{" Bb",  6},{"^Bb",  7},{">Bb",  8},{" Bd",  9},{"<B" , 10},{"vB" , 11}
      ,{" B",  12},{"^B",  13},{">B",  14},{" Cd", 15},{"<C" , 16},{"vC" , 17}
    }},
    { "Bohlen-Pierce", 13, 146.304, 
      {{"C",-10},{"Db",-9},{"D",-8},{"E",-7},{"F",-6},{"Gb",-5}
      ,{"G",-4},{"H",-3},{"Jb",-2},{"J",-1},{"A",0},{"Bb",1},{"B",2}
    }},
    { "Carlos Alpha", 9, 77.9650, 
      {{"I",0},{"I#",1},{"II-",2},{"II+",3},{"III",4}
      ,{"III#",5},{"IV-",6},{"IV+",7},{"Ib",8}
    }},
    { "Carlos Beta", 11, 63.8329,
      {{"I",0},{"I#",1},{"IIb",2},{"II",3},{"II#",4},{"III",5}
      ,{"III#",6},{"IVb",7},{"IV",8},{"IV#",9},{"Ib",10}
    }},
    { "Carlos Gamma", 20, 35.0985,
      {{" I",  0},{"^I",  1},{" IIb", 2},{"^IIb", 3},{" I#",   4},{"^I#",   5}
      ,{" II", 6},{"^II", 7}
      ,{" III",8},{"^III",9},{" IVb",10},{"^IVb",11},{" III#",12},{"^III#",13}
      ,{" IV",14},{"^IV",15},{" Ib", 16},{"^Ib", 17},{" IV#", 18},{"^IV#", 19}
    }},
  };

  paletteDef palette[] = {
    // 12 EDO
      {{ {HUE_NONE,   SAT_BW,    VALUE_NORMAL  }
      , {HUE_BLUE,    SAT_DULL,  VALUE_SHADE  }
      , {HUE_CYAN,    SAT_DULL,  VALUE_NORMAL  }
      , {HUE_INDIGO,  SAT_VIVID, VALUE_NORMAL  }
      }, {1,2,1,2,1,3,4,3,4,3,4,3}},
    // 17 EDO
      {{ {HUE_NONE,    SAT_BW,    VALUE_NORMAL  }
      , {HUE_INDIGO,  SAT_VIVID, VALUE_NORMAL  }
      , {HUE_RED,     SAT_VIVID, VALUE_NORMAL  }
      }, {1,2,3,1,2,3,1,1,2,3,1,2,3,1,2,3,1}},
    // 19 EDO
      {{ {HUE_NONE,    SAT_BW,    VALUE_NORMAL  } // n
      , {HUE_YELLOW,  SAT_VIVID, VALUE_NORMAL  } //  #
      , {HUE_BLUE,    SAT_VIVID, VALUE_NORMAL  } //  b
      , {HUE_MAGENTA, SAT_VIVID, VALUE_NORMAL  } // enh
      }, {1,2,3,1,2,3,1,4,1,2,3,1,2,3,1,2,3,1,4}},
    // 22 EDO
      {{ {HUE_NONE,    SAT_BW,    VALUE_NORMAL  } // n
      , {HUE_BLUE,    SAT_VIVID, VALUE_NORMAL  } // ^
      , {HUE_MAGENTA, SAT_VIVID, VALUE_NORMAL  } // mid
      , {HUE_YELLOW,  SAT_VIVID, VALUE_NORMAL  } // v
      }, {1,2,3,4,1,2,3,4,1,1,2,3,4,1,2,3,4,1,2,3,4,1}},
    // 24 EDO
      {{ {HUE_NONE,    SAT_BW,    VALUE_NORMAL  } // n
      , {HUE_LIME,   SAT_DULL,  VALUE_SHADE } //  +
      , {HUE_CYAN,    SAT_VIVID, VALUE_NORMAL  } //  #/b  
      , {HUE_INDIGO,    SAT_DULL,  VALUE_SHADE } //  d
      , {HUE_CYAN,    SAT_DULL,  VALUE_SHADE } // enh
      }, {1,2,3,4,1,2,3,4,1,5,1,2,3,4,1,2,3,4,1,2,3,4,1,5}},
    // 31 EDO
      {{ {HUE_NONE,    SAT_BW,    VALUE_NORMAL  } // n
      , {HUE_RED,     SAT_DULL,  VALUE_NORMAL  } //  +
      , {HUE_YELLOW,  SAT_DULL,  VALUE_SHADE } //  #
      , {HUE_CYAN,    SAT_DULL,  VALUE_SHADE } //  b
      , {HUE_INDIGO,  SAT_DULL,  VALUE_NORMAL  } //  d
      , {HUE_RED,     SAT_DULL,  VALUE_SHADE } //  enh E+ Fb
      , {HUE_INDIGO,  SAT_DULL,  VALUE_SHADE } //  enh E# Fd
      }, {1,2,3,4,5,1,2,3,4,5,1,6,7,1,2,3,4,5,1,2,3,4,5,1,2,3,4,5,1,6,7}},
    // 41 EDO
      {{ {HUE_NONE,    SAT_BW,    VALUE_NORMAL  } // n
      , {HUE_RED,     SAT_DULL, VALUE_NORMAL  } //  ^
      , {HUE_BLUE,    SAT_VIVID, VALUE_NORMAL  } //  +
      , {HUE_CYAN,    SAT_DULL,  VALUE_SHADE } //  b
      , {HUE_GREEN,   SAT_DULL, VALUE_SHADE } //  #
      , {HUE_MAGENTA, SAT_DULL, VALUE_NORMAL  } //  d
      , {HUE_YELLOW,  SAT_VIVID, VALUE_NORMAL  } //  v
      }, {1,2,3,4,5,6,7,1,2,3,4,5,6,7,1,2,3,1,2,3,4,5,6,7,
          1,2,3,4,5,6,7,1,2,3,4,5,6,7,1,6,7}},
    // 53 EDO
      {{ {HUE_NONE,    SAT_BW,    VALUE_NORMAL  } // n
      , {HUE_ORANGE,  SAT_VIVID,  VALUE_NORMAL  } //  ^
      , {HUE_MAGENTA, SAT_DULL,  VALUE_NORMAL  } //  L
      , {HUE_INDIGO,  SAT_VIVID, VALUE_NORMAL  } // bv
      , {HUE_GREEN,   SAT_VIVID, VALUE_SHADE  } // b
      , {HUE_YELLOW,  SAT_VIVID, VALUE_SHADE  } // #
      , {HUE_RED,     SAT_VIVID, VALUE_NORMAL  } // #^
      , {HUE_PURPLE,  SAT_DULL,  VALUE_NORMAL  } //  7
      , {HUE_CYAN,    SAT_VIVID,  VALUE_SHADE  } //  v
      }, {1,2,3,4,5,6,7,8,9,1,2,3,4,5,6,7,8,9,1,2,3,9,1,2,3,4,5,6,7,8,9,
          1,2,3,4,5,6,7,8,9,1,2,3,4,5,6,7,8,9,1,2,3,9}},
    // 72 EDO
      {{ {HUE_NONE,    SAT_BW,    VALUE_NORMAL  } // n
      , {HUE_GREEN,   SAT_DULL,  VALUE_SHADE } // ^
      , {HUE_RED,     SAT_DULL,  VALUE_SHADE } // L
      , {HUE_PURPLE,  SAT_DULL,  VALUE_SHADE } // +/d
      , {HUE_BLUE,    SAT_DULL,  VALUE_SHADE } // 7
      , {HUE_YELLOW,  SAT_DULL,  VALUE_SHADE } // v
      , {HUE_INDIGO,  SAT_VIVID, VALUE_SHADE  } // #/b
      }, {1,2,3,4,5,6,7,2,3,4,5,6,1,2,3,4,5,6,7,2,3,4,5,6,1,2,3,4,5,6,1,2,3,4,5,6,
          7,2,3,4,5,6,1,2,3,4,5,6,7,2,3,4,5,6,1,2,3,4,5,6,7,2,3,4,5,6,1,2,3,4,5,6}},
    // BOHLEN PIERCE
      {{ {HUE_NONE,    SAT_BW,    VALUE_NORMAL  }
      , {HUE_INDIGO,  SAT_VIVID, VALUE_NORMAL  }
      , {HUE_RED,     SAT_VIVID, VALUE_NORMAL  }
      }, {1,2,3,1,2,3,1,1,2,3,1,2,3}},
    // ALPHA
      {{ {HUE_NONE,    SAT_BW,    VALUE_NORMAL  } // n
      , {HUE_YELLOW,  SAT_VIVID, VALUE_NORMAL  } // #
      , {HUE_INDIGO,  SAT_VIVID, VALUE_NORMAL  } // d
      , {HUE_LIME,    SAT_VIVID, VALUE_NORMAL  } // +
      , {HUE_RED,     SAT_VIVID, VALUE_NORMAL  } // enharmonic
      , {HUE_CYAN,    SAT_VIVID, VALUE_NORMAL  } // b
      }, {1,2,3,4,1,2,3,5,6}},
    // BETA
      {{ {HUE_NONE,    SAT_BW,    VALUE_NORMAL  } // n
      , {HUE_INDIGO,  SAT_VIVID, VALUE_NORMAL  } // #
      , {HUE_RED,     SAT_VIVID, VALUE_NORMAL  } // b
      , {HUE_MAGENTA, SAT_DULL,  VALUE_NORMAL  } // enharmonic
      }, {1,2,3,1,4,1,2,3,1,2,3}},
    // GAMMA
      {{ {HUE_NONE,    SAT_BW,    VALUE_NORMAL  } // n
      , {HUE_RED,     SAT_VIVID, VALUE_NORMAL  } // b
      , {HUE_BLUE,    SAT_VIVID, VALUE_NORMAL  } // #
      , {HUE_YELLOW,  SAT_VIVID, VALUE_NORMAL  } // n^
      , {HUE_PURPLE,  SAT_VIVID, VALUE_NORMAL  } // b^
      , {HUE_GREEN,   SAT_VIVID, VALUE_NORMAL  } // #^
      }, {1,4,2,5,3,6,1,4,1,4,2,5,3,6,1,4,2,5,3,6}},
  };

  layoutDef layoutOptions[] = {
    { "Wicki-Hayden",      1, 64,   2,  -7, TUNING_12EDO },
    { "Harmonic Table",    0, 75,  -7,   3, TUNING_12EDO },
    { "Janko",             0, 65,  -1,  -1, TUNING_12EDO },
    { "Gerhard",           0, 65,  -1,  -3, TUNING_12EDO },
    { "Accordion C-sys.",  1, 75,   2,  -3, TUNING_12EDO },
    { "Accordion B-sys.",  1, 64,   1,  -3, TUNING_12EDO },

    { "Full Gamut",        1, 65,   1,  -9, TUNING_17EDO },
    { "Bosanquet-Wilson",  0, 65,  -2,  -1, TUNING_17EDO },
    { "Neutral Thirds A",  0, 65,  -1,  -2, TUNING_17EDO },
    { "Neutral Thirds B",  0, 65,   1,  -3, TUNING_17EDO },

    { "Full Gamut",        1, 65,   1,  -9, TUNING_19EDO },
    { "Bosanquet-Wilson",  0, 65,  -1,  -2, TUNING_19EDO },
    { "Kleismic",          0, 65,  -1,  -4, TUNING_19EDO },
    
    { "Full Gamut",        1, 65,   1,  -8, TUNING_22EDO },
    { "Bosanquet-Wilson",  0, 65,  -3,  -1, TUNING_22EDO },
    { "Porcupine",         0, 65,   1,  -4, TUNING_22EDO },
    
    { "Full Gamut",        1, 65,   1,  -9, TUNING_24EDO },
    { "Bosanquet-Wilson",  0, 65,  -1,  -3, TUNING_24EDO },
    { "Inverted",          0, 65,   1,  -4, TUNING_24EDO },
    
    { "Full Gamut",        1, 65,   1,  -7, TUNING_31EDO },
    { "Bosanquet-Wilson",  0, 65,  -2,  -3, TUNING_31EDO },
    { "Double Bosanquet",  0, 65,  -1,  -4, TUNING_31EDO },
    { "Anti-Double Bos.",  0, 65,   1,  -5, TUNING_31EDO },
    
    { "Full Gamut",        0, 65,   1,  -8, TUNING_41EDO },  // forty-one #3
    { "Bosanquet-Wilson",  0, 65,  -4,  -3, TUNING_41EDO },  // forty-one #1
    { "Gerhard",           0, 65,   3, -10, TUNING_41EDO },  // forty-one #2
    { "Baldy",             0, 65,  -1,  -6, TUNING_41EDO },  
    { "Rodan",             1, 65,  -1,  -7, TUNING_41EDO },  
    
    { "Wicki-Hayden",      1, 64,   9, -31, TUNING_53EDO },
    { "Bosanquet-Wilson",  0, 65,  -5,  -4, TUNING_53EDO },
    { "Kleismic A",        0, 65,  -8,  -3, TUNING_53EDO },
    { "Kleismic B",        0, 65,  -5,  -3, TUNING_53EDO },
    { "Harmonic Table",    0, 75, -31,  14, TUNING_53EDO },
    { "Buzzard",           0, 65,  -9,  -1, TUNING_53EDO },
    
    { "Full Gamut",        1, 65,   1,  -9, TUNING_72EDO },
    { "Expanded Janko",    0, 65,  -1,  -6, TUNING_72EDO },
    
    { "Full Gamut",        1, 65,   1,  -9, TUNING_BP },
    { "Standard",          0, 65,  -2,  -1, TUNING_BP },
    
    { "Full Gamut",        1, 65,   1,  -9, TUNING_ALPHA },
    { "Compressed",        0, 65,  -2,  -1, TUNING_ALPHA },
    
    { "Full Gamut",        1, 65,   1,  -9, TUNING_BETA },
    { "Compressed",        0, 65,  -2,  -1, TUNING_BETA },
    
    { "Full Gamut",        1, 65,   1,  -9, TUNING_GAMMA },
    { "Compressed",        0, 65,  -2,  -1, TUNING_GAMMA }    
  };

  scaleDef scaleOptions[] = {
    { "None",              ALL_TUNINGS,      { 0 } },
    // 12 EDO
    { "Major",             TUNING_12EDO,     { 2,2,1,2,2,2,1 } },
    { "Minor, natural",    TUNING_12EDO,     { 2,1,2,2,1,2,2 } },
    { "Minor, melodic",    TUNING_12EDO,     { 2,1,2,2,2,2,1 } },
    { "Minor, harmonic",   TUNING_12EDO,     { 2,1,2,2,1,3,1 } },
    { "Pentatonic, major", TUNING_12EDO,     { 2,2,3,2,3 } },
    { "Pentatonic, minor", TUNING_12EDO,     { 3,2,2,3,2 } },
    { "Blues",             TUNING_12EDO,     { 3,1,1,1,1,3,2 } },
    { "Double Harmonic",   TUNING_12EDO,     { 1,3,1,2,1,3,1 } },
    { "Phrygian",          TUNING_12EDO,     { 1,2,2,2,1,2,2 } },
    { "Phrygian Dominant", TUNING_12EDO,     { 1,3,1,2,1,2,2 } },
    { "Dorian",            TUNING_12EDO,     { 2,1,2,2,2,1,2 } },
    { "Lydian",            TUNING_12EDO,     { 2,2,2,1,2,2,1 } },
    { "Lydian Dominant",   TUNING_12EDO,     { 2,2,2,1,2,1,2 } },
    { "Mixolydian",        TUNING_12EDO,     { 2,2,1,2,2,1,2 } },
    { "Locrian",           TUNING_12EDO,     { 1,2,2,1,2,2,2 } },
    { "Whole tone",        TUNING_12EDO,     { 2,2,2,2,2,2 } },
    { "Octatonic",         TUNING_12EDO,     { 2,1,2,1,2,1,2,1 } },
    // 17 EDO; for more: https://en.xen.wiki/w/17edo#Scales
    { "Diatonic",          TUNING_17EDO,  { 3,3,1,3,3,3,1 } },
    { "Pentatonic",        TUNING_17EDO,  { 3,3,4,3,4 } },
    { "Harmonic",          TUNING_17EDO,  { 3,2,3,2,2,2,3 } },
    { "Husayni maqam",     TUNING_17EDO,  { 2,2,3,3,2,1,1,3 } },
    { "Blues",             TUNING_17EDO,  { 4,3,1,1,1,4,3 } },
    { "Hydra",             TUNING_17EDO,  { 3,3,1,1,2,3,2,1,1 } },
    // 19 EDO; for more: https://en.xen.wiki/w/19edo#Scales
    { "Diatonic",          TUNING_19EDO,   { 3,3,2,3,3,3,2 } },
    { "Pentatonic",        TUNING_19EDO,   { 3,3,5,3,5 } },
    { "Semaphore",         TUNING_19EDO,   { 3,1,3,1,3,3,1,3,1 } },
    { "Negri",             TUNING_19EDO,   { 2,2,2,2,2,1,2,2,2,2 } },
    { "Sensi",             TUNING_19EDO,   { 2,2,1,2,2,2,1,2,2,2,1 } },
    { "Kleismic",          TUNING_19EDO,   { 1,3,1,1,3,1,1,3,1,3,1 } },
    { "Magic",             TUNING_19EDO,   { 3,1,1,1,3,1,1,1,3,1,1,1,1 } },
    { "Kind of blues",     TUNING_19EDO,   { 4,4,1,2,4,4 } },
    // 22 EDO; for more: https://en.xen.wiki/w/22edo_modes
    { "Diatonic",          TUNING_22EDO,  { 4,4,1,4,4,4,1 } },
    { "Pentatonic",        TUNING_22EDO,  { 4,4,5,4,5 } },
    { "Orwell",            TUNING_22EDO,  { 3,2,3,2,3,2,3,2,2 } },
    { "Porcupine",         TUNING_22EDO,  { 4,3,3,3,3,3,3 } },
    { "Pajara",            TUNING_22EDO,  { 2,2,3,2,2,2,3,2,2,2 } },
    // 24 EDO; for more: https://en.xen.wiki/w/24edo_scales
    { "Diatonic 12",       TUNING_24EDO, { 4,4,2,4,4,4,2 } },
    { "Diatonic Soft",     TUNING_24EDO, { 3,5,2,3,5,4,2 } },
    { "Diatonic Neutral",  TUNING_24EDO, { 4,3,3,4,3,4,3 } },
    { "Pentatonic (12)",   TUNING_24EDO, { 4,4,6,4,6 } },
    { "Pentatonic (Haba)", TUNING_24EDO, { 5,5,5,5,4 } },
    { "Invert Pentatonic", TUNING_24EDO, { 6,3,6,6,3 } },
    { "Rast maqam",        TUNING_24EDO, { 4,3,3,4,4,2,1,3 } },
    { "Bayati maqam",      TUNING_24EDO, { 3,3,4,4,2,1,3,4 } },      
    { "Hijaz maqam",       TUNING_24EDO, { 2,6,2,4,2,1,3,4 } },
    { "8-EDO",             TUNING_24EDO, { 3,3,3,3,3,3,3,3 } },
    { "Wyschnegradsky",    TUNING_24EDO, { 2,2,2,2,2,1,2,2,2,2,2,2,1 } },
    // 31 EDO; for more: https://en.xen.wiki/w/31edo#Scales
    { "Diatonic",          TUNING_31EDO,  { 5,5,3,5,5,5,3 } },
    { "Pentatonic",        TUNING_31EDO,  { 5,5,8,5,8 } },
    { "Harmonic",          TUNING_31EDO,  { 5,5,4,4,4,3,3,3 } },
    { "Mavila",            TUNING_31EDO,  { 5,3,3,3,5,3,3,3,3 } },
    { "Quartal",           TUNING_31EDO,  { 2,2,7,2,2,7,2,7 } },
    { "Orwell",            TUNING_31EDO,  { 4,3,4,3,4,3,4,3,3 } },
    { "Neutral",           TUNING_31EDO,  { 4,4,4,4,4,4,4,3 } },
    { "Miracle",           TUNING_31EDO,  { 4,3,3,3,3,3,3,3,3,3 } },
    // 41 EDO; for more: https://en.xen.wiki/w/41edo#Scales_and_modes
    { "Diatonic",          TUNING_41EDO,   { 7,7,3,7,7,7,3 } },
    { "Pentatonic",        TUNING_41EDO,   { 7,7,10,7,10 } },
    { "Pure major",        TUNING_41EDO,   { 7,6,4,7,6,7,4 } },
    { "5-limit chromatic", TUNING_41EDO,   { 4,3,4,2,4,3,4,4,2,4,3,4 } },
    { "7-limit chromatic", TUNING_41EDO,   { 3,4,2,4,4,3,4,2,4,3,3,4 } },
    { "Harmonic",          TUNING_41EDO,   { 5,4,4,4,4,3,3,3,3,3,2,3 } },
    { "Middle East-ish",   TUNING_41EDO,   { 7,5,7,5,5,7,5 } },
    { "Thai",              TUNING_41EDO,   { 6,6,6,6,6,6,5 } },
    { "Slendro",           TUNING_41EDO,   { 8,8,8,8,9 } },
    { "Pelog / Mavila",    TUNING_41EDO,   { 8,5,5,8,5,5,5 } },
    // 53 EDO
    { "Diatonic",          TUNING_53EDO, { 9,9,4,9,9,9,4 } },
    { "Pentatonic",        TUNING_53EDO, { 9,9,13,9,13 } },
    { "Rast makam",        TUNING_53EDO, { 9,8,5,9,9,4,4,5 } },
    { "Usshak makam",      TUNING_53EDO, { 7,6,9,9,4,4,5,9 } },
    { "Hicaz makam",       TUNING_53EDO, { 5,12,5,9,4,9,9 } },
    { "Orwell",            TUNING_53EDO, { 7,5,7,5,7,5,7,5,5 } },
    { "Sephiroth",         TUNING_53EDO, { 6,5,5,6,5,5,6,5,5,5 } },
    { "Smitonic",          TUNING_53EDO, { 11,11,3,11,3,11,3 } },
    { "Slendric",          TUNING_53EDO, { 7,3,7,3,7,3,7,3,7,3,3 } },
    { "Semiquartal",       TUNING_53EDO, { 9,2,9,2,9,2,9,2,9 } },
    // 72 EDO
    { "Diatonic",          TUNING_72EDO, { 12,12,6,12,12,12,6 } },
    { "Pentatonic",        TUNING_72EDO, { 12,12,18,12,18 } },
    { "Ben Johnston",      TUNING_72EDO, { 6,6,6,5,5,5,9,8,4,4,7,7 } },
    { "18-EDO",            TUNING_72EDO, { 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 } },
    { "Miracle",           TUNING_72EDO, { 5,2,5,2,5,2,2,5,2,5,2,5,2,5,2,5,2,5,2,5,2 } },
    { "Marvolo",           TUNING_72EDO, { 5,5,5,5,5,5,5,2,5,5,5,5,5,5 } },
    { "Catakleismic",      TUNING_72EDO, { 4,7,4,4,4,7,4,4,4,7,4,4,4,7,4 } },
    { "Palace",            TUNING_72EDO, { 10,9,11,12,10,9,11 } },
    // BP
    { "Lambda",            TUNING_BP, { 2,1,2,1,2,1,2,1,1 } },
    // Alpha
    { "Super Meta Lydian", TUNING_ALPHA, { 3,2,2,2 } },
    // Beta
    { "Super Meta Lydian", TUNING_BETA,  { 3,3,3,2 } },
    // Gamma
    { "Super Meta Lydian", TUNING_GAMMA, { 6,5,5,4 } }
  };

  byte sine[] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   2,   3,   3, 
      4,   5,   6,   7,   8,   9,  10,  12,  13,  15,  16,  18,  19,  21,  23,  25, 
     27,  29,  31,  33,  35,  37,  39,  42,  44,  46,  49,  51,  54,  56,  59,  62, 
     64,  67,  70,  73,  76,  79,  81,  84,  87,  90,  93,  96,  99, 103, 106, 109, 
    112, 115, 118, 121, 124, 127, 131, 134, 137, 140, 143, 146, 149, 152, 156, 159, 
    162, 165, 168, 171, 174, 176, 179, 182, 185, 188, 191, 193, 196, 199, 201, 204, 
    206, 209, 211, 213, 216, 218, 220, 222, 224, 226, 228, 230, 232, 234, 236, 237, 
    239, 240, 242, 243, 245, 246, 247, 248, 249, 250, 251, 252, 252, 253, 254, 254, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 254, 253, 252, 252, 
    251, 250, 249, 248, 247, 246, 245, 243, 242, 240, 239, 237, 236, 234, 232, 230, 
    228, 226, 224, 222, 220, 218, 216, 213, 211, 209, 206, 204, 201, 199, 196, 193, 
    191, 188, 185, 182, 179, 176, 174, 171, 168, 165, 162, 159, 156, 152, 149, 146, 
    143, 140, 137, 134, 131, 127, 124, 121, 118, 115, 112, 109, 106, 103,  99,  96, 
     93,  90,  87,  84,  81,  79,  76,  73,  70,  67,  64,  62,  59,  56,  54,  51, 
     49,  46,  44,  42,  39,  37,  35,  33,  31,  29,  27,  25,  23,  21,  19,  18, 
     16,  15,  13,  12,  10,   9,   8,   7,   6,   5,   4,   3,   3,   2,   1,   1
  };

  byte strings[] = {
      0,   0,   0,   1,   3,   6,  10,  14,  20,  26,  33,  41,  50,  59,  68,  77, 
     87,  97, 106, 115, 124, 132, 140, 146, 152, 157, 161, 164, 166, 167, 167, 167, 
    165, 163, 160, 157, 153, 149, 144, 140, 135, 130, 126, 122, 118, 114, 111, 109, 
    106, 104, 103, 101, 101, 100, 100, 100, 100, 101, 101, 102, 103, 103, 104, 105, 
    106, 107, 108, 109, 110, 111, 113, 114, 115, 116, 117, 119, 120, 121, 123, 124, 
    126, 127, 129, 131, 132, 134, 135, 136, 138, 139, 140, 141, 142, 144, 145, 146, 
    147, 148, 149, 150, 151, 152, 152, 153, 154, 154, 155, 155, 155, 155, 154, 154, 
    152, 151, 149, 146, 144, 140, 137, 133, 129, 125, 120, 115, 111, 106, 102,  98, 
     95,  92,  90,  88,  88,  88,  89,  91,  94,  98, 103, 109, 115, 123, 131, 140, 
    149, 158, 168, 178, 187, 196, 205, 214, 222, 229, 235, 241, 245, 249, 252, 254, 
    255, 255, 255, 254, 253, 250, 248, 245, 242, 239, 236, 233, 230, 227, 224, 222, 
    220, 218, 216, 215, 214, 213, 212, 211, 210, 210, 209, 208, 207, 206, 205, 203, 
    201, 199, 197, 194, 191, 188, 184, 180, 175, 171, 166, 161, 156, 150, 145, 139, 
    133, 127, 122, 116, 110, 105,  99,  94,  89,  84,  80,  75,  71,  67,  64,  61, 
     58,  56,  54,  52,  50,  49,  48,  47,  46,  45,  45,  44,  43,  42,  41,  40, 
     39,  37,  35,  33,  31,  28,  25,  22,  19,  16,  13,  10,   7,   5,   2,   1
  };

  byte clarinet[] = {
      0,   0,   2,   7,  14,  21,  30,  38,  47,  54,  61,  66,  70,  72,  73,  74, 
     73,  73,  72,  71,  70,  71,  72,  74,  76,  80,  84,  88,  93,  97, 101, 105, 
    109, 111, 113, 114, 114, 114, 113, 112, 111, 110, 109, 109, 109, 110, 112, 114, 
    116, 118, 121, 123, 126, 127, 128, 129, 128, 127, 126, 123, 121, 118, 116, 114, 
    112, 110, 109, 109, 109, 110, 111, 112, 113, 114, 114, 114, 113, 111, 109, 105, 
    101,  97,  93,  88,  84,  80,  76,  74,  72,  71,  70,  71,  72,  73,  73,  74, 
     73,  72,  70,  66,  61,  54,  47,  38,  30,  21,  14,   7,   2,   0,   0,   2, 
      9,  18,  31,  46,  64,  84, 105, 127, 150, 171, 191, 209, 224, 237, 246, 252, 
    255, 255, 253, 248, 241, 234, 225, 217, 208, 201, 194, 189, 185, 183, 182, 181, 
    182, 182, 183, 184, 185, 184, 183, 181, 179, 175, 171, 167, 162, 158, 154, 150, 
    146, 144, 142, 141, 141, 141, 142, 143, 144, 145, 146, 146, 146, 145, 143, 141, 
    139, 136, 134, 132, 129, 128, 127, 126, 127, 128, 129, 132, 134, 136, 139, 141, 
    143, 145, 146, 146, 146, 145, 144, 143, 142, 141, 141, 141, 142, 144, 146, 150, 
    154, 158, 162, 167, 171, 175, 179, 181, 183, 184, 185, 184, 183, 182, 182, 181, 
    182, 183, 185, 189, 194, 201, 208, 217, 225, 234, 241, 248, 253, 255, 255, 252, 
    246, 237, 224, 209, 191, 171, 150, 127, 105,  84,  64,  46,  31,  18,   9,   2, 
  };

  // ====== useful math functions
    int positiveMod(int n, int d) {
      return (((n % d) + d) % d);
    }
        
    byte byteLerp(byte xOne, byte xTwo, float yOne, float yTwo, float y) {
      float weight = (y - yOne) / (yTwo - yOne);
      int temp = xOne + ((xTwo - xOne) * weight);
      if (temp < xOne) {temp = xOne;}
      if (temp > xTwo) {temp = xTwo;}
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
    byte maxKnobTurns = 1;

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
    const byte layoutCount = sizeof(layoutOptions) / sizeof(layoutDef);
    const byte scaleCount = sizeof(scaleOptions) / sizeof(scaleDef);

  // Tone and Arpeggiator variables
    oscillator synth[POLYPHONY_LIMIT]; // maximum polyphony
    std::queue<byte> MPEchQueue;
    std::queue<byte> buzzChQueue;
    const byte attenuation[] = {24,24,17,14,12,11,10,9,8}; // kind of a fast inverse square


    byte arpeggiatingNow = UNUSED_NOTE;         // if this is 255, buzzer set to off (0% duty cycle)
    uint64_t arpeggiateTime = 0;         // Used to keep track of when this note started buzzin
    uint32_t arpeggiateLength = 65'536;   // in microseconds

    byte scaleLock = 0;
    byte perceptual = 1;

    int velWheelSpeed = 8;
    int modWheelSpeed = 8;
    int pbWheelSpeed = 1024;

    wheelDef modWheel = { false, true, // standard mode, sticky
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
    GEMPage  menuPageColors("Color options");
    GEMItem  menuGotoColors("Color options", menuPageColors);
    GEMItem  menuColorsBack("<< Back", menuPageMain);
    GEMPage  menuPageSynth("Buzzer options");
    GEMItem  menuGotoSynth("Buzzer options", menuPageSynth);
    GEMItem  menuSynthBack("<< Back", menuPageMain);
    GEMPage  menuPageControl("Control wheel");
    GEMItem  menuGotoControl("Control wheel", menuPageControl);
    GEMItem  menuControlBack("<< Back", menuPageMain);
    GEMPage  menuPageTesting("Testing");
    GEMItem  menuGotoTesting("Testing", menuPageTesting);
    GEMItem  menuTestingBack("<< Back", menuPageMain);
    char* blank = "";
    GEMItem  menuItemVersion("v0.6.0",blank,true);

    // the following get initialized in the setup() routine.
    GEMItem* menuItemTuning[TUNINGCOUNT];       
    GEMItem* menuItemLayout[layoutCount];  
    GEMItem* menuItemScales[scaleCount];       
    GEMSelect* selectKey[TUNINGCOUNT];         
    GEMItem* menuItemKeys[TUNINGCOUNT];       

    byte paletteBeginsAtKeyCenter = 1;

    void setLEDcolorCodes(); // forward-declaration
    SelectOptionByte optionByteYesOrNo[] =  { { "No", 0 }, { "Yes" , 1 } };
    GEMSelect selectYesOrNo( sizeof(optionByteYesOrNo)  / sizeof(SelectOptionByte), optionByteYesOrNo);
    GEMItem  menuItemScaleLock( "Scale lock?", scaleLock, selectYesOrNo);
    GEMItem  menuItemPercep( "Fix color:", perceptual, selectYesOrNo, setLEDcolorCodes);
    GEMItem  menuItemShiftColor( "ColorByKey", paletteBeginsAtKeyCenter, selectYesOrNo, setLEDcolorCodes);

    void resetBuzzers(); // forward declaration
    byte playbackMode = BUZZ_POLY;
    SelectOptionByte optionBytePlayback[] = { { "Off", BUZZ_OFF }, { "Mono", BUZZ_MONO }, { "Arp'gio", BUZZ_ARPEGGIO }, { "Poly", BUZZ_POLY } };
    GEMSelect selectPlayback(sizeof(optionBytePlayback) / sizeof(SelectOptionByte), optionBytePlayback);
    GEMItem  menuItemPlayback(  "Buzzer:",       playbackMode,  selectPlayback, resetBuzzers);

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
    
    byte colorMode = RAINBOW_MODE;
    SelectOptionByte optionByteColor[] =    { { "Rainbow", RAINBOW_MODE }, { "Tiered" , TIERED_COLOR_MODE }, {"Alt", ALTERNATE_COLOR_MODE} };
    GEMSelect selectColor( sizeof(optionByteColor) / sizeof(SelectOptionByte), optionByteColor);
    GEMItem  menuItemColor( "Color mode:", colorMode, selectColor, setLEDcolorCodes);

    byte animationType = ANIMATE_NONE;
    SelectOptionByte optionByteAnimate[] =  { { "None" , ANIMATE_NONE }, { "Octave", ANIMATE_OCTAVE },
      { "By Note", ANIMATE_BY_NOTE }, { "Star", ANIMATE_STAR }, { "Splash" , ANIMATE_SPLASH }, { "Orbit", ANIMATE_ORBIT } };
    GEMSelect selectAnimate( sizeof(optionByteAnimate)  / sizeof(SelectOptionByte), optionByteAnimate);
    GEMItem  menuItemAnimate( "Animation:", animationType, selectAnimate);

    byte globalBrightness = BRIGHT_MID;
    SelectOptionByte optionByteBright[] = { { "Dim", BRIGHT_DIM}, {"Low", BRIGHT_LOW}, {"Normal", BRIGHT_MID}, {"High", BRIGHT_HIGH}, {"THE SUN", BRIGHT_MAX } };
    GEMSelect selectBright( sizeof(optionByteBright) / sizeof(SelectOptionByte), optionByteBright);
    GEMItem menuItemBright( "Brightness", globalBrightness, selectBright, setLEDcolorCodes);

    byte currWave = WAVEFORM_SAW;
    SelectOptionByte optionByteWaveform[] = { { "Square", WAVEFORM_SQUARE }, { "Saw", WAVEFORM_SAW },
    {"Triangl", WAVEFORM_TRIANGLE}, {"Sine", WAVEFORM_SINE}, {"Strings", WAVEFORM_STRINGS}, {"Clrinet", WAVEFORM_CLARINET} };
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
      }
      layoutDef layout() {
        return layoutOptions[layoutIndex];
      }
      scaleDef scale() {
        return scaleOptions[scaleIndex];
      }
      int layoutsBegin() {
        if (tuningIndex == TUNING_12EDO) {
          return 0;
        } else {
          int temp = 0;
          while (layoutOptions[temp].tuning < tuningIndex) {
            temp++;
          }
          return temp;
        }
      }
      int keyStepsFromC() {
        return tuning().spanCtoA() - keyStepsFromA;
      }
      int pitchRelToA4(int givenStepsFromC) {
        return givenStepsFromC + tuning().spanCtoA() + transpose;
      }
      int keyDegree(int givenStepsFromC) {
        return positiveMod(givenStepsFromC + keyStepsFromC(), tuning().cycleLength);
      }
    };

    presetDef current = {
      "Default",      // name
      TUNING_12EDO,      // tuning
      0,              // default to the first layout, wicki hayden
      0,              // default to using no scale (chromatic)
      -9,              // default to the key of C, which in 12EDO is -9 steps from A.
      0               // default to no transposition
    };

// ====== diagnostic wrapper

  void sendToLog(std::string msg) {
    if (diagnostics) {
      Serial.println(msg.c_str());
    }
  }

// ====== LED routines

  int16_t transformHue(float h) {
    float D = fmod(h,360);
    if (!perceptual) {
      return 65536 * D / 360;
    } else {
      //                red            yellow             green        cyan         blue
      int hueIn[] =  {    0,    9,   18,  102,  117,  135,  142,  155,  203,  240,  252,  261,  306,  333,  360};
      //              #ff0000          #ffff00           #00ff00      #00ffff     #0000ff     #ff00ff
      int hueOut[] = {    0, 3640, 5861,10922,12743,16384,21845,27306,32768,38229,43690,49152,54613,58254,65535};
      byte B = 0;
      while (D - hueIn[B] > 0) {
        B++;
      }
      float T = (D - hueIn[B - 1]) / (float)(hueIn[B] - hueIn[B - 1]);
      return (hueOut[B - 1] * (1 - T)) + (hueOut[B] * T);
    }
  }

  uint32_t getLEDcode(colorDef c) {
    return strip.gamma32(strip.ColorHSV(transformHue(c.hue),c.sat,c.val * globalBrightness / 255));
  }

  void setLEDcolorCodes() { // calculate color codes for each hex, store for playback
    for (byte i = 0; i < LED_COUNT; i++) {
      if (!(h[i].isCmd)) {
        colorDef setColor;
        byte paletteIndex = positiveMod(h[i].stepsFromC,current.tuning().cycleLength);
        if (paletteBeginsAtKeyCenter) {
          paletteIndex = current.keyDegree(paletteIndex);
        }
        switch (colorMode) {
          case TIERED_COLOR_MODE:
            setColor = palette[current.tuningIndex].getColor(paletteIndex);
            break;
          case RAINBOW_MODE:
            setColor = 
              { 360.0 * ((float)paletteIndex / (float)current.tuning().cycleLength)
              , SAT_VIVID
              , VALUE_NORMAL
              };
            break;
          case ALTERNATE_COLOR_MODE:
            float cents = current.tuning().stepSize * paletteIndex;
            bool perf = 0;
            float center = 0.0;
                   if                    (cents <   50)  {perf = 1; center =    0.0;}
              else if ((cents >=  50) && (cents <  250)) {          center =  147.1;}
              else if ((cents >= 250) && (cents <  450)) {          center =  351.0;}
              else if ((cents >= 450) && (cents <  600)) {perf = 1; center =  498.0;}
              else if ((cents >= 600) && (cents <= 750)) {perf = 1; center =  702.0;}
              else if ((cents >  750) && (cents <= 950)) {          center =  849.0;}
              else if ((cents >  950) && (cents <=1150)) {          center = 1053.0;}
              else if ((cents > 1150) && (cents < 1250)) {perf = 1; center = 1200.0;}
              else if ((cents >=1250) && (cents < 1450)) {          center = 1347.1;}
              else if ((cents >=1450) && (cents < 1650)) {          center = 1551.0;}
              else if ((cents >=1650) && (cents < 1850)) {perf = 1; center = 1698.0;}
              else if ((cents >=1800) && (cents <=1950)) {perf = 1; center = 1902.0;}
            float offCenter = cents - center;
            int16_t altHue = positiveMod((int)(150 + (perf * ((offCenter > 0) ? -72 : 72)) - round(1.44 * offCenter)), 360);
            float deSaturate = perf * (abs(offCenter) < 20) * (1 - (0.02 * abs(offCenter)));
            setColor = { (float)altHue, 255 - (255 * deSaturate), (cents ? VALUE_SHADE : VALUE_NORMAL) };
            break;
        }
        h[i].LEDcodeRest   = getLEDcode(setColor);
        h[i].LEDcodePlay = getLEDcode(setColor.tint()); 
        h[i].LEDcodeDim  = getLEDcode(setColor.shade());  
        setColor = {HUE_NONE,SAT_BW,VALUE_BLACK};
        h[i].LEDcodeOff  = getLEDcode(setColor);                // turn off entirely
        h[i].LEDcodeAnim = h[i].LEDcodePlay;
      }
    }
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
    }
  }
  uint32_t applyNotePixelColor(byte x) {
           if (h[x].animate) { return h[x].LEDcodeAnim;
    } else if (h[x].MIDIch)  { return h[x].LEDcodePlay;
    } else if (h[x].inScale) { return h[x].LEDcodeRest;
    } else if (scaleLock)    { return h[x].LEDcodeOff;
    } else                   { return h[x].LEDcodeDim;
    }
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
        }
        sendToLog(
          "hex #" + std::to_string(i) + ", " +
          "steps=" + std::to_string(h[i].stepsFromC) + ", " +
          "isCmd? " + std::to_string(h[i].isCmd) + ", " +
          "note=" + std::to_string(h[i].note) + ", " + 
          "bend=" + std::to_string(h[i].bend) + ", " + 
          "freq=" + std::to_string(h[i].frequency) + ", " + 
          "inScale? " + std::to_string(h[i].inScale) + "."
        );
      }
    }
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
            }  // add the steps in the scale, and you're in scale
            h[i].inScale = (tempSum == degree);   // if the note lands on one of those sums
          }
        }
        sendToLog(
          "hex #" + std::to_string(i) + ", " +
          "steps=" + std::to_string(h[i].stepsFromC) + ", " +
          "isCmd? " + std::to_string(h[i].isCmd) + ", " +
          "note=" + std::to_string(h[i].note) + ", " + 
          "inScale? " + std::to_string(h[i].inScale) + "."
        );
      }
    }
    setLEDcolorCodes();
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
      }
    }
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
    uint32_t mix = 0;
    byte voices = POLYPHONY_LIMIT;
    byte p;
    byte level = 0;
    for (byte i = 0; i < POLYPHONY_LIMIT; i++) {
      if (synth[i].increment) {
        synth[i].counter += synth[i].increment; // should loop from 65536 -> 0
        p = (synth[i].counter >> 8);
        switch (currWave) {
          case WAVEFORM_SAW:                                               break;
          case WAVEFORM_TRIANGLE: p = 2 * ((p < 128) ? p : (255 - p));     break;
          case WAVEFORM_SQUARE:   p = 0 - (p > (128 - modWheel.curValue)); break;
          case WAVEFORM_SINE:     p = sine[p];                             break;
          case WAVEFORM_STRINGS:  p = strings[p];                          break;
          case WAVEFORM_CLARINET: p = clarinet[p];                         break;
          default:                                                         break;
        }
        mix += (p * synth[i].eq);  // P[8bit] * EQ[8bit] =[16bit]
      } else {
        --voices;
      }
    }
    mix = mix * attenuation[voices] * velWheel.curValue; // [16bit]*vel[7bit]=[23bit], poly+atten=[6bit] = [29bit] 
    level = mix >> 21;  // [29bit] - [8bit] = [21bit]
    pwm_set_chan_level(TONE_SL, TONE_CH, level);
  }
  // RUN ON CORE 1
  byte isoTwoTwentySix(float f) {
    // a very crude implementation of ISO 226
    // equal loudness curves
    //   Hz dB  Amp = sqrt(10^(dB/10))
    //  200  0  255
    //  800 -3  181   
    // 1500  0  255
    // 3250 -6  127
    // 5000  0  255
    if ((f < 8.0) || (f > 12500.0)) {   // really crude low- and high-pass
      return 0;
    } else {
      if ((f <= 200.0) || (f >= 5000.0)) {
        return 255;
      } else {
        if (f < 1500.0) {
          return 181 + 74  * (float)(abs(f-800) / 700);
        } else {
          return 127 + 128 * (float)(abs(f-3250) / 1750);
        }
      }
    }
  }
  void setBuzzer(float f, byte c) {
    synth[c - 1].counter = 0;
    float FwithPB = f * exp2(pbWheel.curValue * PITCH_BEND_SEMIS / 98304.0);
    synth[c - 1].increment = round(FwithPB * POLL_INTERVAL_IN_MICROSECONDS * 0.065536);   // cycle 0-65535 at resultant frequency
    synth[c - 1].eq = isoTwoTwentySix(FwithPB);
  }

  // USE THIS IN MONO OR ARPEG MODE ONLY

  byte findNextHeldNote() {
    byte n = UNUSED_NOTE;
    for (byte i = 1; i <= LED_COUNT; i++) {
      byte j = positiveMod(arpeggiatingNow + i, LED_COUNT);
      if ((h[j].MIDIch) && (!h[j].isCmd)) {
        n = j;
        break;
      }
    }
    return n;
  }
  void replaceBuzzerWith(byte x) {
    if (arpeggiatingNow != x) {
      arpeggiatingNow = x;
      if (arpeggiatingNow != UNUSED_NOTE) {
        setBuzzer(h[arpeggiatingNow].frequency, 1);
      } else {
        setBuzzer(0, 1);
      }
    }
  }

  void resetBuzzers() {
    while (!buzzChQueue.empty()) {
      buzzChQueue.pop();
    }
    for (byte i = 0; i < POLYPHONY_LIMIT; i++) {
      synth[i].increment = 0;
      synth[i].counter = 0;
    }
    if (playbackMode == BUZZ_POLY) {
      for (byte i = 0; i < POLYPHONY_LIMIT; i++) {
        buzzChQueue.push(i + 1);
      }
    }
  }
// ====== MIDI routines
  void setPitchBendRange(byte Ch, byte semitones) {
    MIDI.beginRpn(0, Ch);
    MIDI.sendRpnValue(semitones << 7, Ch);
    MIDI.endRpn(Ch);
    sendToLog(
      "set pitch bend range on ch " +
      std::to_string(Ch) + " to be " + 
      std::to_string(semitones) + " semitones"
    );
  }
  void setMPEzone(byte masterCh, byte sizeOfZone) {
    MIDI.beginRpn(6, masterCh);
    MIDI.sendRpnValue(sizeOfZone << 7, masterCh);
    MIDI.endRpn(masterCh);
    sendToLog(
      "tried sending MIDI msg to set MPE zone, master ch " +
      std::to_string(masterCh) + ", zone of this size: " + std::to_string(sizeOfZone)
    );
  }
  void resetTuningMIDI() {
    while (!MPEchQueue.empty()) {         // empty the channel queue
      MPEchQueue.pop();
    }
    for (byte i = 1; i <= 16; i++) {
      MIDI.sendControlChange(123, 0, i);
      setPitchBendRange(i, PITCH_BEND_SEMIS);   // force pitch bend back to the expected range of 2 semitones.
    }
    if (current.tuningIndex == TUNING_12EDO) {
      setMPEzone(1, 0);
    } else {
      setMPEzone(1, 15);   // MPE zone 1 = ch 2 thru 16
      for (byte i = 0; i < 15; i++) {
        MPEchQueue.push(i + 2);
        sendToLog("pushed ch " + std::to_string(i + 2) + " to the open channel queue");
      }
    }
    resetBuzzers();
  }
  void chgModulation() {
    MIDI.sendControlChange(1, modWheel.curValue, 1);
    sendToLog("sent mod value " + std::to_string(modWheel.curValue) + " to ch 1");
  }
  void chgUniversalPB() {
    MIDI.sendPitchBend(pbWheel.curValue, 1);
    for (byte i = 0; i < LED_COUNT; i++) {
      if (!(h[i].isCmd)) {
        if (h[i].buzzCh) {
          setBuzzer(h[i].frequency,h[i].buzzCh);           // rebuzz all notes if the pitch bend changes
        }
      }
      sendToLog("sent pb wheel value " + std::to_string(pbWheel.curValue) + " to ch 1");
    }
  }
  
// ====== hex press routines

  void playNote(byte x) {
    // this gets called on any non-command hex
    // that is not scale-locked.
    if (!(h[x].MIDIch)) {    
      if (current.tuningIndex == TUNING_12EDO) {
        h[x].MIDIch = 1;
      } else {
        if (MPEchQueue.empty()) {   // if there aren't any open channels
          sendToLog("MPE queue was empty so did not play a midi note");
        } else {
          h[x].MIDIch = MPEchQueue.front();   // value in MIDI terms (1-16)
          MPEchQueue.pop();
          sendToLog("popped " + std::to_string(h[x].MIDIch) + " off the MPE queue");
          MIDI.sendPitchBend(h[x].bend, h[x].MIDIch); // ch 1-16
        }
      }
      if (h[x].MIDIch) {
        MIDI.sendNoteOn(h[x].note, velWheel.curValue, h[x].MIDIch); // ch 1-16
        sendToLog(
          "sent MIDI noteOn: " + std::to_string(h[x].note) +
          " pb "  + std::to_string(h[x].bend) +
          " vel " + std::to_string(velWheel.curValue) +
          " ch "  + std::to_string(h[x].MIDIch)
        );
      } 
    }
    if (playbackMode != BUZZ_OFF) {
      if (playbackMode == BUZZ_POLY) {
        // operate independently of MIDI
        if (buzzChQueue.empty()) {
          sendToLog("synths all firing, so did not buzz");
        } else {
          h[x].buzzCh = buzzChQueue.front();
          buzzChQueue.pop();
          sendToLog("popped " + std::to_string(h[x].buzzCh) + " off the synth queue");
          setBuzzer(h[x].frequency, h[x].buzzCh);
        }
      } else {    
        // operate in lockstep with MIDI
        if (h[x].MIDIch) {
          replaceBuzzerWith(x);
        }
      }
    }
  } 
  
  void stopNote(byte x) {
    // this gets called on any non-command hex
    // that is not scale-locked.
    if (h[x].MIDIch) {    // but just in case, check
      MIDI.sendNoteOff(h[x].note, velWheel.curValue, h[x].MIDIch);    
      sendToLog(
        "sent note off: " + std::to_string(h[x].note) +
        " pb " + std::to_string(h[x].bend) +
        " vel " + std::to_string(velWheel.curValue) +
        " ch " + std::to_string(h[x].MIDIch)
      );
      if (current.tuningIndex != TUNING_12EDO) {
        MPEchQueue.push(h[x].MIDIch);
        sendToLog("pushed " + std::to_string(h[x].MIDIch) + " on the MPE queue");
      }
      h[x].MIDIch = 0;
      if (playbackMode && (playbackMode != BUZZ_POLY)) {
        replaceBuzzerWith(findNextHeldNote());
      }
    }
    if (playbackMode == BUZZ_POLY) {
      if (h[x].buzzCh) {
        setBuzzer(0, h[x].buzzCh);
        buzzChQueue.push(h[x].buzzCh);
        h[x].buzzCh = 0;
      }
    }
  }
  void cmdOn(byte x) {   // volume and mod wheel read all current buttons
    switch (h[x].note) {
      case CMDB + 3:
        toggleWheel = !toggleWheel;
        break;
      default:
        // the rest should all be taken care of within the wheelDef structure
        break;
    }
  }
  void cmdOff(byte x) {   // pitch bend wheel only if buttons held.
    switch (h[x].note) {
      default:
        break;  // nothing; should all be taken care of within the wheelDef structure
    }
  }

// ====== animations
  uint64_t animFrame(byte x) {     
    if (h[x].timePressed) {          // 2^20 microseconds is close enough to 1 second
      return 1 + (((runTime - h[x].timePressed) * animationFPS) >> 20);
    } else {
      return 0;
    }
  }
  void flagToAnimate(int8_t r, int8_t c) {
    if (! 
      (    ( r < 0 ) || ( r >= ROWCOUNT )
        || ( c < 0 ) || ( c >= (2 * COLCOUNT) )
        || ( ( c + r ) & 1 )
      )
    ) {
      h[(10 * r) + (c / 2)].animate = 1;
    }
  }
  void animateMirror() {
    for (byte i = 0; i < LED_COUNT; i++) {                   // check every hex
      if ((!(h[i].isCmd)) && (h[i].MIDIch)) {              // that is a held note     
        for (byte j = 0; j < LED_COUNT; j++) {               // compare to every hex
          if ((!(h[j].isCmd)) && (!(h[j].MIDIch))) {       // that is a note not being played
            int16_t temp = h[i].stepsFromC - h[j].stepsFromC;         // look at difference between notes
            if (animationType == ANIMATE_OCTAVE) {              // set octave diff to zero if need be
              temp = positiveMod(temp, current.tuning().cycleLength);
            }
            if (temp == 0) {                                // highlight if diff is zero
              h[j].animate = 1;
            }
          }
        }  
      }
    }
  }

  void animateOrbit() {
    for (byte i = 0; i < LED_COUNT; i++) {                               // check every hex
      if ((!(h[i].isCmd)) && (h[i].MIDIch) && ((h[i].inScale) || (!scaleLock))) {    // that is a held note
        byte tempDir = (animFrame(i) % 6);
        flagToAnimate(h[i].coordRow + vertical[tempDir], h[i].coordCol + horizontal[tempDir]);       // different neighbor each frame
      }
    }
  }

  void animateRadial() {
    for (byte i = 0; i < LED_COUNT; i++) {                              // check every hex
      if (!(h[i].isCmd)) {                                              // that is a note
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
            }
          }
        }
      }      
    }    
  }

// ====== menu routines
  void menuHome() {
    menu.setMenuPageCurrent(menuPageMain);
    menu.drawMenu();
  }
  void showOnlyValidLayoutChoices() { // re-run at setup and whenever tuning changes
    for (byte L = 0; L < layoutCount; L++) {
      menuItemLayout[L]->hide((layoutOptions[L].tuning != current.tuningIndex));
    }
    sendToLog("menu: Layout choices were updated.");
  }
  void showOnlyValidScaleChoices() { // re-run at setup and whenever tuning changes
    for (int S = 0; S < scaleCount; S++) {
      menuItemScales[S]->hide((scaleOptions[S].tuning != current.tuningIndex) && (scaleOptions[S].tuning != ALL_TUNINGS));
    }
    sendToLog("menu: Scale choices were updated.");
  }
  void showOnlyValidKeyChoices() { // re-run at setup and whenever tuning changes
    for (int T = 0; T < TUNINGCOUNT; T++) {
      menuItemKeys[T]->hide((T != current.tuningIndex));
    }
    sendToLog("menu: Key choices were updated.");
  }
  void changeLayout(GEMCallbackData callbackData) {  // when you change the layout via the menu
    byte selection = callbackData.valByte;
    if (selection != current.layoutIndex) {
      current.layoutIndex = selection;
      applyLayout();
    }
    menuHome();
  }
  void changeScale(GEMCallbackData callbackData) {   // when you change the scale via the menu
    int selection = callbackData.valInt;
    if (selection != current.scaleIndex) {
      current.scaleIndex = selection;
      applyScale();
    }
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
      resetTuningMIDI();  // clear out MIDI queue
    }
    menuHome();
  }
  void createTuningMenuItems() {
    for (byte T = 0; T < TUNINGCOUNT; T++) {
      menuItemTuning[T] = new GEMItem(tuningOptions[T].name.c_str(), changeTuning, T);
      menuPageTuning.addMenuItem(*menuItemTuning[T]);
    }
  }
  void createLayoutMenuItems() {
    for (byte L = 0; L < layoutCount; L++) { // create pointers to all layouts
      menuItemLayout[L] = new GEMItem(layoutOptions[L].name.c_str(), changeLayout, L);
      menuPageLayout.addMenuItem(*menuItemLayout[L]);
    }
    showOnlyValidLayoutChoices();
  }
  void createKeyMenuItems() {
    for (byte T = 0; T < TUNINGCOUNT; T++) {
      selectKey[T] = new GEMSelect(tuningOptions[T].cycleLength, tuningOptions[T].keyChoices);
      menuItemKeys[T] = new GEMItem("Key:", current.keyStepsFromA, *selectKey[T], changeKey);
      menuPageScales.addMenuItem(*menuItemKeys[T]);
    }
    showOnlyValidKeyChoices();
  }
  void createScaleMenuItems() {
    for (int S = 0; S < scaleCount; S++) {  // create pointers to all scale items, filter them as you go
      menuItemScales[S] = new GEMItem(scaleOptions[S].name.c_str(), changeScale, S);
      menuPageScales.addMenuItem(*menuItemScales[S]);
    }
    showOnlyValidScaleChoices();
  }

// ====== setup routines
  void testDiagnostics() {
    sendToLog("theHDM was here");
  }
  void setupMIDI() {
    usb_midi.setStringDescriptor("HexBoard MIDI");  // Initialize MIDI, and listen to all MIDI channels
    MIDI.begin(MIDI_CHANNEL_OMNI);                  // This will also call usb_midi's begin()
    resetTuningMIDI
();
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
    }
    for (byte c = 0; c < CMDCOUNT; c++) {
      h[assignCmd[c]].isCmd = 1;
      h[assignCmd[c]].note = CMDB + c;
    }
    sendToLog("initializing hex grid...");
    applyLayout();
  }
  void setupLEDs() { 
    strip.begin();    // INITIALIZE NeoPixel strip object
    strip.show();     // Turn OFF all pixels ASAP
    sendToLog("LEDs started..."); 
    setLEDcolorCodes();
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
    menuPageMain.addMenuItem(menuGotoColors);
      menuPageColors.addMenuItem(menuItemColor);
      menuPageColors.addMenuItem(menuItemBright);
      menuPageColors.addMenuItem(menuItemAnimate);
      menuPageColors.addMenuItem(menuColorsBack);
    menuPageMain.addMenuItem(menuGotoSynth);
      menuPageSynth.addMenuItem(menuItemPlayback);  
      menuPageSynth.addMenuItem(menuItemWaveform);
      menuPageSynth.addMenuItem(menuSynthBack);
    menuPageMain.addMenuItem(menuItemTransposeSteps);
    menuPageMain.addMenuItem(menuGotoTesting);
      menuPageTesting.addMenuItem(menuItemVersion);
      menuPageTesting.addMenuItem(menuItemPercep);
      menuPageTesting.addMenuItem(menuItemShiftColor);
      menuPageTesting.addMenuItem(menuTestingBack);
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
        }
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
          }
          break;
        case 2: // just released
          if (h[i].isCmd) {
            cmdOff(i);
          } else if (h[i].inScale || (!scaleLock)) {
            stopNote(i);
          }
          break;
        case 3: // held
          break;
        default: // inactive
          break;
      }
    }
  }
  void arpeggiate() {
    if (playbackMode == BUZZ_ARPEGGIO) {
      if (runTime - arpeggiateTime > arpeggiateLength) {
        arpeggiateTime = runTime;
        replaceBuzzerWith(findNextHeldNote());
      }
    }
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
      }
    } else {
      modWheel.setTargetValue();
      upd = modWheel.updateValue(runTime);
      if (upd) {
        chgModulation();
      }
    }
  }
  
  void animateLEDs() {  
    for (byte i = 0; i < LED_COUNT; i++) {      
      h[i].animate = 0;
    }
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
      }
    }
  }
  void lightUpLEDs() {   
    for (byte i = 0; i < LED_COUNT; i++) {      
      if (!(h[i].isCmd)) {
        strip.setPixelColor(i,applyNotePixelColor(i));
      }
    }
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
    }
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
  }
  void loop1() {  // run on second core
    keepTrackOfRotaryKnobTurns();
  }
