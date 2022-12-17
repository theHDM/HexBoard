# Copied fqbn from build.options.json
build/HexBoard_V1.1.ino.uf2: HexBoard_V1.1.ino
	arduino-cli compile -b rp2040:rp2040:generic:flash=16777216_0,freq=133,opt=Small,rtti=Disabled,stackprotect=Disabled,exceptions=Disabled,dbgport=Disabled,dbglvl=None,usbstack=tinyusb,ipstack=ipv4only,boot2=boot2_generic_03h_2_padded_checksum --output-dir build	

install: build/HexBoard_V1.1.ino.uf2
	echo "Trying to copy into mounted device"
	cp build/HexBoard_V1.1.ino.uf2 /run/media/zach/RPI-RP2/
	echo "Installed."
