#esp8266_bitseq

General purpose synchronized bit sampler for esp8266.

You can often find a hidden serial interface on your gadgets, it is used for in-factory testing and to sell you overpriced 'addons' with very limited functionality. 

This library you can help you sample that data and do whatever you like with it.

![Caliper](/doc/caliper-serial.png)
This is how the caliper sends data: 24 bits of clocked data. 

These packages are separated by a idle period and then repeated. What this library does is to sample each databit at rising or falling clock pulse into a cyclic buffer. When it has aquired enough bits (configurable) and detected an idle period it will send your code a callback so that the data can be parsed and used.  

I've added examples where i connect a cheap digital caliper, a digital dial and a power plug energy meter to a mqtt message broker.

## Digital Caliper

![Caliper](/doc/caliper.jpg). 

The main inspiration for this sampler was this [blog](https://sites.google.com/site/marthalprojects/home/arduino/arduino-reads-digital-caliper)

## Digital Dial

![Dial](/doc/dial.jpg) 
 
 [Justin R.](https://hackaday.io/hacker/1910-justin-r) wrote a really useful [blog](https://hackaday.io/project/511-digital-dial-indicator-cnc-surface-probe/log/814-the-digital-dial-indicator-and-how-to-read-from-it) about the digital dial that helped me a lot (no code though).

## power-plug-energy-meter

The energy meter is described in [Kalle Löfgrens blog] (http://gizmosnack.blogspot.se/2014/10/power-plug-energy-meter-hack.html).
Just be aware that there is 50% chance that the logic wires (including GND) in that power meter will be directly connected to 230V.
Don't even open the case if you don't know what you are doing.

