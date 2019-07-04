# IR Receiver for BeoSound Core using an ESP3266 WiFi Arduino device
## Setting up hardware
Connect an IR receiver to GPIO5 (labeled D1 on some boards).
## Setting up software
* Install and start Arduino IDE
* Connect the ESP3266 board
* Go to Tools->Card->Manage cards... and install esp3266
* Set the correct card type
* Go to Tools->Manage library... and install IRremoteESP8266 and ArduinoJson
* Open the .ino file and change the SSID and password constants.
* Install the code onto the device
* Open serial monitor and use your remote of choice and press volume up and down and write down the codes.
* Set the variables ircode_volumeUp and ircode_volumeDown to the corresponding codes.
* Install the code again.
* You should now be able to raise and lower the volume of your BeoSound Core with the IR remote. The ESP3266 device can be powered by the USB port on the back of the BeoSound Core.