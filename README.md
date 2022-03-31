# Power Meter

See a writeup here: https://imateapot.dev/homemade-power-meter/

This is the complete code for a crank-based power meter, transmitting over BLE
to a bike computer. Schematics and a hardware parts list for the hardware, as well as 
notes on calibrating, are on [the wiki](https://gitlab.com/sjarvis/powermeter/-/wikis/home).
In all, it's a full instruction set for building your own cycling power meter,
compatible with any bike computer that supports Bluetooth (i.e. any Wahoo
or Garmin).

This is a fun project and I trained with it just great for a year. It can handle the 
bumps and (minor) hits from the road, but I did struggle to get it any kind of 
water resistant. I'd say that was the biggest pain point. It's also not a super 
attractive form factor, but function > form, right?!

## Performance

As far as accuracy, it's within a reasonable margin of error when compared with
commercial units, and usually on the low side. As far as training goes, it's for 
certain valuable, because consistency is most important. As far as ego goes, I 
guess it's not the friendliest. I tested these times and durations with about 
these results:

1. Wahoo Kickr, the programmed 20 min FTP test. This meter read 9% lower.
2. Computrainer, 40 min at ~190 watts. This meter read 6% higher.
3. Favero Assioma Duo, 40 min at ~190 watts. This meter read 6% lower.

## License

This project is licensed under the terms of the MIT license.
