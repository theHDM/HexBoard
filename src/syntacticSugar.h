#pragma once

using timeStamp = unsigned long long int;
using colorCode = unsigned long int;

#include <vector>
using int_vec = std::vector<int>;
using int_matrix = std::vector<std::vector<int>>;

#include <deque>
using time_queue = std::deque<timeStamp>;
using int_queue = std::deque<int>;

#include <numeric>              // need that GCD function, son
#include <string>               // standard C++ library string classes (use "std::string" to invoke it); these do not cause the memory corruption that Arduino::String does.
#include <queue>                // standard C++ library construction to store open channels in microtonal mode (use "std::queue" to invoke it)

#include "hardware/timer.h"
timeStamp getTheCurrentTime() {
  timeStamp temp = timer_hw->timerawh;
  return (temp << 32) | timer_hw->timerawl;
}

/*
  C++ returns a negative value for 
  negative N % D. This function
  guarantees the mod value is always
  positive.
*/
int positiveMod(int n, int d) {
  return (((n % d) + d) % d);
}
/*
  There may already exist linear interpolation
  functions in the standard library. This one is helpful
  because it will do the weighting division for you.
  It only works on byte values since it's intended
  to blend color values together. A better C++
  coder may be able to allow automatic type casting here.
*/
byte byteLerp(byte xOne, byte xTwo, float yOne, float yTwo, float y) {
  float weight = (y - yOne) / (yTwo - yOne);
  int temp = xOne + ((xTwo - xOne) * weight);
  if (temp < xOne) {temp = xOne;}
  if (temp > xTwo) {temp = xTwo;}
  return temp;
}
