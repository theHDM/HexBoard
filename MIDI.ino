// Send MIDI Note On
void noteOn(byte channel, byte pitch, byte velocity) {
  MIDI.sendNoteOn(pitch, velocity, channel);
  if (diagnostics == 3) {
    Serial.print(pitch);
    Serial.print(", ");
    Serial.print(velocity);
    Serial.print(", ");
    Serial.println(channel);
  }
}
// Send MIDI Note Off
void noteOff(byte channel, byte pitch, byte velocity) {
  MIDI.sendNoteOff(pitch, velocity, channel);
}