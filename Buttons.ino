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
        // TODO: Implement debounce?
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

void playNotes() {
  for (int i = 0; i < elementCount; i++)  // For all buttons in the deck
  {
    if (activeButtons[i] != previousActiveButtons[i])  // If a change is detected
    {
      if (activeButtons[i] == 1)  // If the button is active (newpress)
      {
        if (currentLayout[i] < 128) {
          strip.setPixelColor(i, strip.ColorHSV((currentLayout[i] % 12) * 5006, 255, 255));
          noteOn(midiChannel, (currentLayout[i] + octave) % 128, midiVelocity);
        } else {
          commandPress(currentLayout[i]);
        }
      } else {
        // If the button is inactive (released)
        if (currentLayout[i] < 128) {
          setLayoutLED(i);
          noteOff(midiChannel, (currentLayout[i] + octave) % 128, 0);
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