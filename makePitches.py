#!/usr/bin/env python3
import sys

pitches=128

# calculate integer values in hertz for equal-temperament scales
def makePitches(root, tones, offset):
    a = []
    for index in range(offset, pitches+offset):
        a.append(round(root*(2**(index/tones))))
    return a

if __name__ == '__main__':
    offset = 0
    if len(sys.argv) > 3:
        offset = int(sys.argv[3])
    pitches = makePitches(float(sys.argv[1]), int(sys.argv[2]), offset)
    print(pitches)
