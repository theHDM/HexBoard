#pragma once
#include <Arduino.h>
#include <Wire.h>

const     uint rotary_rate_in_uS  = 512;

class rotaryKnob {
  public:
    rotaryKnob(byte _Apin, byte _Bpin, byte _Cpin, bool _bufferTurns); // declare constructor
    void invertDirection(bool arg_invert); // declare function to swap A/B pins
    void update();
    int  getTurnFromBuffer(); // positive = counterclockwise
    int  getClick();
    int  getValueInTurnBuffer();
    byte getApin();
    byte getBpin();
    byte getCpin();
    int  getKnobState();
  private:
    int  turnBuffer;
    int  clickBuffer;
    byte Apin;
    byte Bpin;
    byte Cpin;
    byte state;
    byte press;
    bool invert;
    bool clicked;
    bool bufferTurns;
};

/*
  Documentation:
    Rotary knob code derived from:
      https://github.com/buxtronix/arduino/tree/master/libraries/Rotary
  Copyright 2011 Ben Buxton. Licenced under the GNU GPL Version 3.
  Contact: bb@cactii.net

  when the mechanical rotary knob is turned,
  the two pins go through a set sequence of
  states during one physical "click", as follows:
    Direction          Binary state of pin A\B
    Counterclockwise = 1\1, 0\1, 0\0, 1\0, 1\1
    Clockwise        = 1\1, 1\0, 0\0, 0\1, 1\1

  The neutral state of the knob is 1\1; a turn
  is complete when 1\1 is reached again after
  passing through all the valid states above,
  at which point action should be taken depending
  on the direction of the turn.
  
  The variable "state" captures all this as follows
    Value    Meaning
    0        Knob is in neutral state
    1, 2, 3  CCW turn state 1, 2, 3
    4, 5, 6   CW turn state 1, 2, 3
    8, 16    Completed turn CCW, CW
*/

const byte stateTable[7][4] = {
  {0,4,1,0},
  {2,0,1,0},{2,3,1,0},{2,3,0,8},
  {5,4,0,0},{5,4,6,0},{5,0,6,16}
};

rotaryKnob::rotaryKnob(byte _Apin, byte _Bpin, byte _Cpin, bool _bufferTurns) {
  // reset pins based on input BEFORE creating the object
  pinMode(_Apin, INPUT_PULLUP);
  pinMode(_Bpin, INPUT_PULLUP);
  pinMode(_Cpin, INPUT_PULLUP);
  turnBuffer = 0;
  Apin = _Apin;
  Bpin = _Bpin;
  Cpin = _Cpin;
  state = 0;
  clicked = true;
  bufferTurns = _bufferTurns;
}

void rotaryKnob::invertDirection(bool arg_invert) {
  invert = arg_invert;
}

void rotaryKnob::update() {
  byte prevState = (state & 0b00111);
  byte getRotation;
  if (invert) {
    getRotation = ((digitalRead(Apin) << 1) | digitalRead(Bpin));
  } else {
    getRotation = ((digitalRead(Bpin) << 1) | digitalRead(Apin));
  }
  state = stateTable[prevState][getRotation];
  turnBuffer += (state & 0b01000) >> 3;
  turnBuffer -= (state & 0b10000) >> 4;
}

int rotaryKnob::getTurnFromBuffer() {
  int temp = ((turnBuffer > 0) ? 1 : -1) * (turnBuffer != 0);
  if (bufferTurns) {
    turnBuffer -= temp;
  } else {
    turnBuffer = 0;
  }
  return temp;
}

int rotaryKnob::getClick() {
  bool temp = digitalRead(Cpin);
  bool result = (temp > clicked);
  clicked = temp;
  return result;
}

int rotaryKnob::getValueInTurnBuffer() {return turnBuffer;}
byte rotaryKnob::getApin() {return Apin;}
byte rotaryKnob::getBpin() {return Bpin;}
byte rotaryKnob::getCpin() {return Cpin;}
int rotaryKnob::getKnobState() {return state;}