# HexBoard MIDI Controller

The HexBoard is a 140-key board designed for techno-musicians, babies, and computer people.

![A HexBoard with the default layout active](https://shapingthesilence.com/wp-content/uploads/2023/05/IMG_7850-scaled-e1683770617108.jpeg)

You can [order your HexBoard today](https://shapingthesilence.com/tech/hexboard-midi-controller/),
or [contact Jared](mailto:jared@shapingthesilence.com) if you're interested in assembling it yourself.

## The Team
* Jared DeCook has been writing music, developing hardware, and performing as [Shaping The Silence](https://shapingthesilence.com/) for over a decade.
* Zach DeCook has been listening to music, breaking hardware, and occasionally writing software since the former discovered his exploitable talents.

## HexBoard Arduino firmware

The Arduino firmware is the default firmware for the HexBoard (and is what this repository contains).

### Building the firmware

If you want to build the firmware,
we'll assume you're already proficient at installing software on your computer at this point.

#### Using [Arduino-IDE](https://www.arduino.cc/en/software)

(Instructions to come)

#### Using [arduino-cli](https://arduino.github.io/arduino-cli/latest/)

(Instructions to come)

### Flashing the firmware

1. Unplug the HexBoard from your computer, then plug it in while holding the button by the USB port.
2. It should appear as a disk in your computer.
3. Copy the .uf2 firmware file onto that disk
4. The disk should eject, and the HexBoard should automatically reboot into that firmware.
