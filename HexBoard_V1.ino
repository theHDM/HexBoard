// Hardware Information:
// Teensy LC set to 48MHz with USB type MIDI

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
int diagnostics = 1;

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
const int UNUSED = 255;

#define ROW_FLIP(x, ix, viii, vii, vi, v, iv, iii, ii, i) i, ii, iii, iv, v, vi, vii, viii, ix, x

// MIDI note value tables
const byte wickiHaydenLayout[elementCount] = {
ROW_FLIP(OCT_UP, 78,  80,  82,  84,  86,  88,  90,  92,  94),
ROW_FLIP(     71,  73,  75,  77,  79,  81,  83,  85,  87,  89),
ROW_FLIP(OCT_DN, 66,  68,  70,  72,  74,  76,  78,  80,  82),
ROW_FLIP(     59,  61,  63,  65,  67,  69,  71,  73,  75,  77),
ROW_FLIP(UNUSED, 54,  56,  58,  60,  62,  64,  66,  68,  70),
ROW_FLIP(     47,  49,  51,  53,  55,  57,  59,  61,  63,  65),
ROW_FLIP(UNUSED, 42,  44,  46,  48,  50,  52,  54,  56,  58),
ROW_FLIP(     35,  37,  39,  41,  43,  45,  47,  49,  51,  53),
ROW_FLIP(UNUSED, 30,  32,  34,  36,  38,  40,  42,  44,  46),
ROW_FLIP(     23,  25,  27,  29,  31,  33,  35,  37,  39,  41)
};
const byte harmonicTableLayout[elementCount] = {
ROW_FLIP(OCT_UP, 20,  27,  34,  41,  48,  55,  62,  69,  76),
ROW_FLIP(     17,  24,  31,  38,  45,  52,  59,  66,  73,  80),
ROW_FLIP(OCT_DN, 21,  28,  35,  42,  49,  56,  63,  70,  77),
ROW_FLIP(     18,  25,  32,  39,  46,  53,  60,  67,  74,  81),
ROW_FLIP(UNUSED, 22,  29,  36,  43,  50,  57,  64,  71,  78),
ROW_FLIP(     19,  26,  33,  40,  47,  54,  61,  68,  75,  82),
ROW_FLIP(UNUSED, 23,  30,  37,  44,  51,  58,  65,  72,  79),
ROW_FLIP(     20,  27,  34,  41,  48,  55,  62,  69,  76,  83),
ROW_FLIP(UNUSED, 24,  31,  38,  45,  52,  59,  66,  73,  80),
ROW_FLIP(     21,  28,  35,  42,  49,  56,  63,  70,  77,  84)
};
const byte gerhardLayout[elementCount] = {
ROW_FLIP(OCT_UP, 20,  21,  22,  23,  24,  25,  26,  27,  28),
ROW_FLIP(     23,  24,  25,  26,  27,  28,  29,  30,  31,  32),
ROW_FLIP(OCT_DN, 27,  28,  29,  30,  31,  32,  33,  34,  35),
ROW_FLIP(     30,  31,  32,  33,  34,  35,  36,  37,  38,  39),
ROW_FLIP(UNUSED, 34,  35,  36,  37,  38,  39,  40,  41,  42),
ROW_FLIP(     37,  38,  39,  40,  41,  42,  43,  44,  45,  46),
ROW_FLIP(UNUSED, 41,  42,  43,  44,  45,  46,  47,  48,  49),
ROW_FLIP(     44,  45,  46,  47,  48,  49,  50,  51,  52,  53),
ROW_FLIP(UNUSED, 48,  49,  50,  51,  52,  53,  54,  55,  56),
ROW_FLIP(     51,  52,  53,  54,  55,  56,  57,  58,  59,  60)
};

//byte *currentLayout = &wickiHaydenLayout;

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

// MIDI program variables
byte midiProgram[16];                                                   // MIDI program selection per channel (0-15)

// Octave modifier
int octave = 0;// Apply a MIDI note number offset (changed via user input in steps of 12)

// Velocity levels
byte velocity = 95;                                                     // Non-zero default velocity for testing; this will update via analog pot
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
        if (wickiHaydenLayout[i] < 128) {
          noteOn(midiChannel, (wickiHaydenLayout[i] + octave) % 128 , velocity);
        } else {
          commandPress(wickiHaydenLayout[i]);
        }
      } else {
      // If the button is inactive (released)
        if (wickiHaydenLayout[i] < 128) {
          noteOff(midiChannel, (wickiHaydenLayout[i] + octave) % 128, 0);
        } else {
          commandRelease(wickiHaydenLayout[i]);
        }
      }
    }
  }
}

// MIDI PACKET FUNCTIONS

// Send MIDI Note On
// 1st byte = Event type (0x09 = note on, 0x08 = note off).
// 2nd byte = Event type bitwise ORed with MIDI channel.
// 3rd byte = MIDI note number.
// 4th byte = Velocity (7-bit range 0-127)
void noteOn(byte channel, byte pitch, byte velocity)
{
  usbMIDI.sendNoteOn(pitch, velocity, channel);
}
void loopNoteOn(byte channel, byte pitch, byte velocity)
{

}

void commandPress(byte command)
{
  // Keep octave between -12 and 24
  if(command == OCT_DN) {
    if (octave >= 0) {
      octave -= 12;
    }
  } else if (command == OCT_UP) {
    if (octave <= 12) {
      octave += 12;
    }
  }
}
void commandRelease(byte command)
{
  if (command == OCT_DN) {
    // Do something?
  } else if (command == OCT_UP) {
    // Do something else?
  }
}


// Send MIDI Note Off
// 1st byte = Event type (0x09 = note on, 0x08 = note off).
// 2nd byte = Event type bitwise ORed with MIDI channel.
// 3rd byte = MIDI note number.
// 4th byte = Velocity (7-bit range 0-127)
void noteOff(byte channel, byte pitch, byte velocity)
{
  usbMIDI.sendNoteOff(pitch, velocity, channel);
}
void loopNoteOff(byte channel, byte pitch, byte velocity)
{

}

// Control Change
// 1st byte = Event type (0x0B = Control Change).
// 2nd byte = Event type bitwise ORed with MIDI channel.
// 3rd byte = MIDI CC number (7-bit range 0-127).
// 4th byte = Control value (7-bit range 0-127).
void controlChange(byte channel, byte control, byte value)
{

}
void loopControlChange(byte channel, byte control, byte value)
{

}

// Program Change
// 1st byte = Event type (0x0C = Program Change).
// 2nd byte = Event type bitwise ORed with MIDI channel.
// 3rd byte = Program value (7-bit range 0-127).
void programChange(byte channel, byte value)
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
void loopPitchBendChange(byte channel, byte lowValue, byte highValue)
{

}

// END FUNCTIONS SECTION
// ------------------------------------------------------------------------------------------------------------------------------------------------------------

// END OF PROGRAM
// ------------------------------------------------------------------------------------------------------------------------------------------------------------
