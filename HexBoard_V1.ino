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
bool diagnostics = 0;

// Define digital button matrix pins
const byte columns[] = { 25, 24,  9,  8,  7,  6,  5,  4,  3,  2};                // Column pins in order from right to left
const byte rows[]    = {10, 11, 12, 13, 14, 15, 16, 18, 19, 20, 21, 22, 23, 26}; // Row pins in order from top to bottom
// 16 & 17 reserved for lights.
const byte columnCount      = sizeof(columns);                          // The number of columns in the matrix
const byte rowCount         = sizeof(rows);                             // The number of rows in the matrix
const byte elementCount     = columnCount * rowCount;// The number of elements in the matrix

// Since MIDI only uses 7 bits, we can give greater values special meanings.
// (see commandPress)
const int OCTAVEDOWN = 128;
const int OCTAVEUP = 129;
const int UNUSED = 255;

// MIDI note value tables
const byte wickiHaydenLayout[elementCount] = {
   78,  80,  82,  84,  86,  88,  90,  92,  94,  OCTAVEUP,
71,  73,  75,  77,  79,  81,  83,  85,  87,  89,
   66,  68,  70,  72,  74,  76,  78,  80,  82,  OCTAVEDOWN,
59,  61,  63,  65,  67,  69,  71,  73,  75,  77,
   54,  56,  58,  60,  62,  64,  66,  68,  70,  UNUSED,
47,  49,  51,  53,  55,  57,  59,  61,  63,  65,
   42,  44,  46,  48,  50,  52,  54,  56,  58,  UNUSED,
35,  37,  39,  41,  43,  45,  47,  49,  51,  53,
   30,  32,  34,  36,  38,  40,  42,  44,  46,  UNUSED,
23,  25,  27,  29,  31,  33,  35,  37,  39,  41
};
const byte harmonicTableLayout[elementCount] = {
   20,  27,  34,  41,  48,  55,  62,  69,  76,  OCTAVEUP,
17,  24,  31,  38,  45,  52,  59,  66,  73,  80,
   21,  28,  35,  42,  49,  56,  63,  70,  77,  OCTAVEDOWN,
18,  25,  32,  39,  46,  53,  60,  67,  74,  81,
   22,  29,  36,  43,  50,  57,  64,  71,  78,  UNUSED,
19,  26,  33,  40,  47,  54,  61,  68,  75,  82,
   23,  30,  37,  44,  51,  58,  65,  72,  79,  UNUSED,
20,  27,  34,  41,  48,  55,  62,  69,  76,  83,
   24,  31,  38,  45,  52,  59,  66,  73,  80,  UNUSED,
21,  28,  35,  42,  49,  56,  63,  70,  77,  84
};
const byte gerhardLayout[elementCount] = {
   20,  21,  22,  23,  24,  25,  26,  27,  28,  OCTAVEUP,
23,  24,  25,  26,  27,  28,  29,  30,  31,  32,
   27,  28,  29,  30,  31,  32,  33,  34,  35,  OCTAVEDOWN,
30,  31,  32,  33,  34,  35,  36,  37,  38,  39,
   34,  35,  36,  37,  38,  39,  40,  41,  42,  UNUSED,
37,  38,  39,  40,  41,  42,  43,  44,  45,  46,
   41,  42,  43,  44,  45,  46,  47,  48,  49,  UNUSED,
44,  45,  46,  47,  48,  49,  50,  51,  52,  53,
   48,  49,  50,  51,  52,  53,  54,  55,  56,  UNUSED,
51,  52,  53,  54,  55,  56,  57,  58,  59,  60
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

// Control button states

byte octaveUpState; // Top row (white) right
byte octaveDownState; // Bottom row (white) right


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
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// START LOOP SECTION
void loop()
{
  // Print diagnostic troubleshooting information to serial monitor
  // diagnosticTest();

  // Store the current time in a uniform variable for this program loop
  currentTime = millis();

  // Read and store the digital button states of the scanning matrix
  readDigitalButtons();

  // Set all states and values related to the control buttons and pots
  //    runControlModule(); //turned off for now

  // Run the octave select function
  //    runOctave(); //wating until we get the basics sorted out

  // Run the channel select function
  //runChannelSelect();

  // Send notes to the MIDI bus
  playNotes();
}
// END LOOP SECTION
// ------------------------------------------------------------------------------------------------------------------------------------------------------------


// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// START FUNCTIONS SECTION

void readDigitalButtons()
{
  // Button Deck
  for (byte columnIndex = 0; columnIndex < columnCount; columnIndex++) // Iterate through each of the column pins.
  {
    if (diagnostics == 1){
      Serial.println();
    }
    byte currentColumn = columns[columnIndex]; // Hold the currently selected column pin in a variable.
    pinMode(currentColumn, OUTPUT); // Set that column pin to OUTPUT mode and...
    digitalWrite(currentColumn, LOW); // set the pin state to LOW turning it into a temporary ground.
    for (byte rowIndex = 0; rowIndex < rowCount; rowIndex++) // Now iterate through each of the row pins that are connected to the current column pin.
    {
      byte currentRow = rows[rowIndex]; // Hold the currently selected row pin in a variable.
      pinMode(currentRow, INPUT_PULLUP); // Set that row pin to INPUT_PULLUP mode (+3.3V / HIGH).
      byte buttonNumber = columnIndex + (rowIndex * columnCount); // Assign this location in the matrix a unique number.
      if (diagnostics == 1){
        Serial.print(buttonNumber);
        Serial.print(" - ");
      }
      delayMicroseconds(50); // Delay to give the pin modes time to change state (false readings are caused otherwise).
      byte buttonState = !digitalRead(currentRow); // Invert reading due to INPUT_PULLUP, and store the currently selected pin state.
      if (buttonState == HIGH && (millis() - activeButtonsTime[buttonNumber]) > debounceTime)     // If button is active and passes debounce
      {
        if (diagnostics == 1){
          Serial.print("1, ");
        }
        activeButtons[buttonNumber] = 1;                                                        // write a 1 to the storage variable
        activeButtonsTime[buttonNumber] = millis();                                             // and save the last button press time for later debounce comparison.
      }
      if (buttonState == LOW)
      {
        if (diagnostics == 1){
          Serial.print("0, ");
        }
        activeButtons[buttonNumber] = 0;                                                        // Or if the button is inactive, write a 0.
      }
      pinMode(currentRow, INPUT);                                                                 // Set the selected row pin back to INPUT mode (0V / LOW).
    }
    pinMode(currentColumn, INPUT);                                                                  // Set the selected column pin back to INPUT mode (0V / LOW) and move onto the next column pin.
  }
}

void runControlModule()
{
  // Digital Buttons
  for (int buttonNumber = 9; buttonNumber < 30; buttonNumber++) // Limit to the 10 buttons in the control panel
  {
    if (activeButtons[buttonNumber] != previousActiveButtons[buttonNumber]) // Compare current button state to the previous state, and if a difference is found...
    {
      if (activeButtons[buttonNumber] == 1) // If the buttons is active
      {
        if (buttonNumber == 9) {
          octaveUpState      = HIGH;
        }
        if (buttonNumber == 29) {
          octaveDownState    = HIGH;
        }
        previousActiveButtons[buttonNumber] = 1; // Update the "previous" variable for comparison next loop
      }
      if (activeButtons[buttonNumber] == 0) // If the button is inactive
      {

        if (buttonNumber == 19) {
          octaveUpState      = LOW;
        }
        if (buttonNumber == 39) {
          octaveDownState    = LOW;
        }
        previousActiveButtons[buttonNumber] = 0; // Update the "previous" variable for conparison next loop
      }
    }
  }
}

// TODO: We still want to be able to change octaves with the two buttons 19 and 39.
void runOctave()
{
  /*if (metaKeyState == LOW)                                                                        // If the meta key is not held
    {
      if (octaveUpState == HIGH && previousOctaveUpState == LOW && octave < 24)                   // Highest current Wicki-Hayden layout pitch is 94 - Keep pitch in bounds of 7 bit value range (0-127)
      {
          previousOctaveUpState = HIGH;                                                           // Lock input until released
          for (int i = 10; i < elementCount; i++)                                                 // For all note buttons in the deck
          {
              activeButtons[i] = 0;                                                               // Pop any active notes to prevent hangs when abruptly changing octave modifier
          }
          playNotes();
          octave = octave + 12;                                                                   // Increment octave modifier
          // LCD Update Octave Info
          if (octave ==  0) { lcd.setCursor(14,1); lcd.print(" 0"); }
          if (octave == 12) { lcd.setCursor(14,1); lcd.write(0); lcd.print("1"); }
          if (octave == 24) { lcd.setCursor(14,1); lcd.write(0); lcd.print("2"); }
          if (octave == -12) { lcd.setCursor(14,1); lcd.write(1);lcd.print("1"); }
          if (octave == -24) { lcd.setCursor(14,1); lcd.write(1);lcd.print("2"); }
      }
      if (octaveDownState == HIGH && previousOctaveDownState == LOW && octave > -23)              // Lowest current Wicki-Hayden layout pitch is 30 - Keep pitch in bounds of 7 bit value range (0-127)
      {
          previousOctaveDownState = HIGH;                                                         // Lock input until released
          for (int i = 10; i < elementCount; i++)                                                 // For all note buttons in the deck
          {
              activeButtons[i] = 0;                                                               // Pop any active notes to prevent hangs when abruptly changing octave modifier
          }
          playNotes();
          octave = octave - 12;                                                                   // Decrement octave modifier
          // LCD Update Octave Info
          if (octave ==  0) { lcd.setCursor(14,1); lcd.print(" 0"); }
          if (octave == 12) { lcd.setCursor(14,1); lcd.write(0); lcd.print("1"); }
          if (octave == 24) { lcd.setCursor(14,1); lcd.write(0); lcd.print("2"); }
          if (octave == -12) { lcd.setCursor(14,1); lcd.write(1);lcd.print("1"); }
          if (octave == -24) { lcd.setCursor(14,1); lcd.write(1);lcd.print("2"); }
      }
      if (octaveUpState == HIGH && octaveDownState == HIGH && octave != 0)                        // If both keys are pressed simultaneously and octave is not default
      {
          for (int i = 10; i < elementCount; i++)                                                 // For all note buttons in the deck
          {
              activeButtons[i] = 0;                                                               // Pop any active notes to prevent hangs when abruptly changing octave modifier
          }
          playNotes();
          octave = 0;                                                                             // Reset octave modifier to 0
          // LCD Update Octave Info
          if (octave ==  0) { lcd.setCursor(14,1); lcd.print(" 0"); }
          if (octave == 12) { lcd.setCursor(14,1); lcd.write(0); lcd.print("1"); }
          if (octave == 24) { lcd.setCursor(14,1); lcd.write(0); lcd.print("2"); }
          if (octave == -12) { lcd.setCursor(14,1); lcd.write(1);lcd.print("1"); }
          if (octave == -24) { lcd.setCursor(14,1); lcd.write(1);lcd.print("2"); }
      }
    }*/
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
        
        previousActiveButtons[i] = 1; // Update the "previous" variable for comparison on next loop
      }
      if (activeButtons[i] == 0) // If the button is inactive (released)
      {
        if (wickiHaydenLayout[i] < 128) {
          noteOff(midiChannel, (wickiHaydenLayout[i] + octave) % 128, 0);
        } else {
          commandRelease(wickiHaydenLayout[i]);
        }
        
        previousActiveButtons[i] = 0; // Update the "previous" variable for comparison on next loop
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
  if(command == OCTAVEDOWN){
    octave -= 12;
    octaveDownState = HIGH;
  } else if (command == OCTAVEUP){
    octave += 12;
        octaveUpState = HIGH;
  }
  if (octaveDownState && octaveUpState) {
    timeBothPressed = currentTime;
  } else {
    timeBothPressed = 0;
  }
}
void commandRelease(byte command)
{
  if(command == OCTAVEDOWN){
      octaveDownState = LOW;
  } else if (command == OCTAVEUP){
    octaveUpState = LOW;
  }
  if (timeBothPressed && currentTime > timeBothPressed + 500){
    octave = 0;
    // also change modes
    timeBothPressed = 0;
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
