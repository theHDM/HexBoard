#!/usr/bin/env python3
import sys

evenCols=9
oddCols=10
rows=14

def makeLayout(starting, across, downleft):
    a = []
    for row in range(0, rows):
        a.append([])
        for col in range(0, oddCols if row%2 else evenCols):
            if row == 0 and col == 0:
                a[row].append(starting)
            elif col > 0:
                a[row].append(a[row][col-1] + across)
            else: # col == 0
                if row%2:
                    ref = a[row-1][0]
                else:
                    ref = a[row-1][1]
                a[row].append(ref+downleft)
    return a

if __name__ == '__main__':
    layout = makeLayout(int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]))
    #print(layout)
    row = 0
    for Row in layout:
        if row%2 == 0:
            n = int(row/2)+1
            print(f"  ROW_FLIP(CMDB_{n}, ", end='')
        else:
            print("        ROW_FLIP(", end='')
        col = 1
        for entry in Row:
            end = ', ' if col < (oddCols if row%2 else evenCols) else ''
            print(entry, end=end)
            col = col + 1
        print(')' if row +1 == rows else '),')
        row = row + 1
