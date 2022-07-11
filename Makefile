# As of arduino-cli 0.24.0, teensy boards are supported
# from https://www.pjrc.com/teensy/td_156/package_teensy_index.json
build/HexBoard_V1.ino.hex: HexBoard_V1.ino
	arduino-cli compile -b teensy:avr:teensyLC --board-options usb=midi --output-dir build	

install: build/HexBoard_V1.ino.hex
	echo "Waiting to install, maybe press program-mode button?"
	teensy_loader_cli -w --mcu=TEENSYLC build/HexBoard_V1.ino.hex
	echo "Installed."
