# buttwarmer

Buttwarmer controller

This is a little program to control the PWMs on my car's butt warmers.  It
monitors enable switches for each side, and two knobs for each side (for the
back, and bottom, of the seat).  Unlike the factory controls, this one can keep
running with the engine stopped, so it also monitors the battery voltage so it
can be shut down before the car can't restart.

This is done as a weekend hack, for the single prototype control board
installed in my car.  It is written for the ATMEGA168 with absolutely no HAL.
Registers are unceremoniously peeked and poked, basically because I was having
fun exploring the AVR's peripherals.
