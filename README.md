#esp8266_bitseq

General purpose synchronized bit sampler for esp8266.

You can often find a hidden serial interface on your gadgets, it is used for in-factory testing and to sell you overpriced 'addons' with very limited functionality. 

This library you can help you sample that data and do whatever you like with it.

![Caliper](/doc/caliper-serial.png)
This is how the caliper sends data: one channel for the clock pulses and another for the data. 

These packages are usually separated by an idle period and then repeated. What this library does is to sample each databit at rising or falling clock pulse into a cyclic buffer. When it has aquired enough bits (configurable) and detected an idle period it will send your code a callback so that the data can be parsed and used.  

I've added examples where i connect a cheap digital caliper, a digital dial and a power plug energy meter to a [mqtt](http://mqtt.org) message broker.

## Digital Caliper [(wiki)](https://github.com/eadf/esp8266_bitseq/wiki/caliper)

![Caliper](/doc/caliper.jpg). 

The main inspiration for this sampler was this [blog.](https://sites.google.com/site/marthalprojects/home/arduino/arduino-reads-digital-caliper)

## Digital Dial [(wiki)](https://github.com/eadf/esp8266_bitseq/wiki/dial)

![Dial](/doc/dial.jpg) 
 
 [Justin R.](https://hackaday.io/hacker/1910-justin-r) wrote a really useful [blog](https://hackaday.io/project/511-digital-dial-indicator-cnc-surface-probe/log/814-the-digital-dial-indicator-and-how-to-read-from-it) about the digital dial that helped me a lot (no code though).

## Power plug energy meter [(wiki)](https://github.com/eadf/esp8266_bitseq/wiki/watt)

![Dial](/doc/watt.png)

The energy meter is described in [Kalle Löfgrens blog] (http://gizmosnack.blogspot.se/2014/10/power-plug-energy-meter-hack.html).
Just be aware that there is 50% chance that the logic wires (including GND) in that power meter will be directly connected to 230V.
Don't even open the case if you don't know what you are doing.
The LCD in the image is a Nokia 5110 LCD connected to another [mqtt](https://github.com/tuanpmt/esp_mqtt)-enabled esp8266. Find the [lcd project here.](https://github.com/eadf/esp_mqtt_lcd)

# Default pin configuration

Device| ESP8266
-------|------------------
Clock | GPIO0
Data | GPIO2

You can easily change the pin numbers as you see fit.

# License 

GNU GENERAL PUBLIC LICENSE Version 3

The makefile is copied from [esp_mqtt.](https://github.com/tuanpmt/esp_mqtt)

###Building and installing:

First you need to install the sdk and the easy way of doing that is to use [esp_open_sdk.](https://github.com/pfalcon/esp-open-sdk)

You can put that anywhere you like (/opt/local/esp-open-sdk, /esptools etc etc)

Then you could create a small ```setenv.sh``` file, containing the location of your newly compiled sdk and other platform specific info;
```
export SDK_BASE=/opt/local/esp-open-sdk/sdk
export PATH=${SDK_BASE}/../xtensa-lx106-elf/bin:${PATH}
export ESPPORT=/dev/ttyO0  
```
(or setup your IDE to do the same)

To make a clean build, flash and connect to the esp console you just do this in a shell:
```
source setenv.sh # This is only needed once per session
make clean && make test
```

You won't be needing esptool, the makefile only uses esptool.py (provided by [esp_open_sdk](https://github.com/pfalcon/esp-open-sdk))

I have tested this with sdk v0.9.5 and v0.9.4 (linux & mac)
