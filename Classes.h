// class definitions are in a header so that
// they get read before compiling the main program.

class tuningDef {
public:
  std::string name; // limit is 17 characters for GEM menu
  byte cycleLength; // steps before period/cycle/octave repeats
  float stepSize;   // in cents, 100 = "normal" semitone.
  SelectOptionInt keyChoices[MAX_SCALE_DIVISIONS];
  int spanCtoA() {
    return (- keyChoices[0].val_int);
  };
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
  colorDef mixWithWhite() {
    colorDef temp;
    temp.hue = this->hue;
    temp.sat = ((this->sat > SAT_TINT) ? SAT_TINT : this->sat);
    temp.val = VALUE_FULL;
    return temp;
  };
};

class paletteDef {
public:
  colorDef swatch[MAX_SCALE_DIVISIONS]; // the different colors used in this palette
  byte colorNum[MAX_SCALE_DIVISIONS];   // map key (c,d...) to swatches
  colorDef getColor(byte givenStepFromC) {
    return swatch[colorNum[givenStepFromC] - 1];
  };
  float getHue(byte givenStepFromC) {
    return getColor(givenStepFromC).hue;
  };
  byte getSat(byte givenStepFromC) {
    return getColor(givenStepFromC).sat;
  };
  byte getVal(byte givenStepFromC) {
    return getColor(givenStepFromC).val;
  };
};

class buttonDef {
public:
  byte     btnState = 0;        // binary 00 = off, 01 = just pressed, 10 = just released, 11 = held
  void interpBtnPress(bool isPress) {
    btnState = (((btnState << 1) + isPress) & 3);
  };
  int8_t   coordRow = 0;        // hex coordinates
  int8_t   coordCol = 0;        // hex coordinates
  uint32_t timePressed = 0;     // timecode of last press
  uint32_t LEDcolorAnim = 0;    // calculate it once and store value, to make LED playback snappier 
  uint32_t LEDcolorPlay = 0;    // calculate it once and store value, to make LED playback snappier
  uint32_t LEDcolorOn = 0;      // calculate it once and store value, to make LED playback snappier
  uint32_t LEDcolorOff = 0;     // calculate it once and store value, to make LED playback snappier
  bool     animate = 0;         // hex is flagged as part of the animation in this frame, helps make animations smoother
  int16_t  stepsFromC = 0;      // number of steps from C4 (semitones in 12EDO; microtones if >12EDO)
  bool     isCmd = 0;           // 0 if it's a MIDI note; 1 if it's a MIDI control cmd
  bool     inScale = 0;         // 0 if it's not in the selected scale; 1 if it is
  byte     note = UNUSED_NOTE;  // MIDI note or control parameter corresponding to this hex
  int16_t  bend = 0;            // in microtonal mode, the pitch bend for this note needed to be tuned correctly
  byte     channel = 0;         // what MIDI channel this note is playing on
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
            if (*topBtn == 1)     {temp += *stepValue;}; // tap button
            if (*botBtn == 1)     {temp -= *stepValue;}; // tap button
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
      };
    }
  };
  bool updateValue(uint32_t givenTime) {
    int16_t temp = targetValue - curValue;
    if (temp != 0) {
      if ((givenTime - timeLastChanged) >= CC_MSG_COOLDOWN_MICROSECONDS ) {
        timeLastChanged = givenTime;
        if (abs(temp) < *stepValue) {
          curValue = targetValue;
        } else {
          curValue = curValue + (*stepValue * (temp / abs(temp)));
        };
        return 1;
      } else {
        return 0;
      };
    } else {
      return 0;
    };
  };   
};
// back button

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
};