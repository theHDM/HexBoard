void setOctLED() {
  if (octave <= 0) {
    strip.setPixelColor(octUpSW, 120, 0, 120);
    strip.setPixelColor(octDnSW, 0, 0, 0);  // No lower to go.
  } else if (octave <= 12) {
    strip.setPixelColor(octUpSW, 0, 0, 120);
    strip.setPixelColor(octDnSW, 120, 0, 0);
  } else if (octave <= 24) {
    strip.setPixelColor(octUpSW, 0, 0, 0);  //No higher to go.
    strip.setPixelColor(octDnSW, 120, 0, 120);
  }
}

void setLayoutLEDs() {
  for (int i = 0; i < elementCount; i++) {
    if (currentLayout[i] <= 127) {
      setLayoutLED(i);
    }
  }
}
void setLayoutLED(int i) {
  strip.setPixelColor(i, strip.ColorHSV((currentLayout[i] % 12) * 5006, 255, 120));
  // black keys darker
  if (blackKeys) {
    // LEET programmers stuff
    switch (currentLayout[i] % 12) {
      // If it is one of the black keys, fall through to case 10.
      case 1:
      case 3:
      case 6:
      case 8:
      // bitshift by 2 (efficient division by four)
      case 10: strip.setPixelColor(i, strip.ColorHSV((currentLayout[i] % 12) * 5006, 255, 30)); break;
      // otherwise it was a white key. Do nothing
      default: break;
    }
  }
}