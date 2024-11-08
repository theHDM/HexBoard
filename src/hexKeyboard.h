// appears to work on https://binji.github.io/wasm-clang/
// tested Nov 7 2024
#pragma once
#include "syntacticSugar.h"

// ---------------------------------------

class pinGrid_obj {
    private:
        bool _isEnabled;
        bool _cycle_mux_pins_first;
        int_vec _muxPins;
        int_vec _colPins;
        std::size_t _muxSize;
        std::size_t _colSize;
        int_matrix _gridState;
        int_matrix _outputMap;
        int _muxCounter;
        int _colCounter;
        int _gridCounter;
        int _muxMaxValue;
        bool _readComplete;
        void resetCounter() {
            _readComplete = false;
            _gridCounter = 0;
        }
        void init_pin_states() {
            for (auto &m : _muxPins) {
                pinMode(m, OUTPUT);
            }
            for (auto &c : _colPins) {
                pinMode(c, INPUT_PULLUP);
            }
        }
        bool advanceMux() {
            _muxCounter = (++_muxCounter) % _muxMaxValue;
            for (int b = 0; b < _muxSize; b++) {
                digitalWrite(_muxPins[b], (_muxCounter >> b) & 1);
            }
            return (!(_muxCounter));
        }
        bool advanceCol() {
            pinMode(_colPins[_colCounter], INPUT);
            _colCounter = (++_colCounter) % _colSize;
            pinMode(_colPins[_colCounter], INPUT_PULLUP);
            return (!(_colCounter));
        }
    public:    
        void setup(int_vec muxPins, int_vec colPins, int_matrix outputMap, bool cycleMuxFirst = true) {
            _cycle_mux_pins_first = cycleMuxFirst;
            _muxPins = muxPins;
            _muxSize = _muxPins.size();
            _muxMaxValue = (1u << _muxSize);
            _colPins = colPins;
            _colSize = _colPins.size();
            _outputMap = outputMap;
            _gridState.resize(_colSize);
            for (auto& row : _gridState) {
                row.resize(_muxMaxValue);
            }
            _muxCounter = 0;
            _colCounter = 0;
            resetCounter();
            init_pin_states();
        }
        void poll() {
            if (!(_readComplete)) {
                _gridState[_colCounter][_muxCounter] = 
                  digitalRead(_colPins[_colCounter]);
                //  analogRead(_colPins[_colCounter]);
                ++_gridCounter;
                if (_cycle_mux_pins_first) {
                    if (advanceMux()) {
                        _readComplete = advanceCol();
                    }
                } else {
                    if (advanceCol()) {
                        _readComplete = advanceMux();
                    }
                }
            }
        }
        bool readTo(int_vec &refTo) {
            if (_readComplete) {
                for (size_t i = 0; i < _outputMap.size(); i++) {
                    for (size_t j = 0; j < _outputMap[i].size(); j++) {
                        refTo[_outputMap[i][j]] = _gridState[i][j];
                    }
                }
                resetCounter();
                return true;
            } else {
                return false;
            }
        }
};