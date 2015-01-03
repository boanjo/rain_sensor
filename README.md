# rain_sensor
A tipping bucket 433MHz rain sensor compatible with Tellstick, Rfxcom etc.

The firmware is based on Brage SIP lite (an Arduino UNO like board with smaller footprint and less components that draw current). I have chosen to emulate the existing Oregon PCR800 tipping bucket protocol rather than to create a propertary one (for compatibility reasons mainly but it is also a pretty good protocol). The board can be bought by the swedish company Lawicel, but you can assemble your own board. Note that you cant't use a regular Arduino UNO etc that has a voltage regulator on it. The firmware has been developed to shut down as much functionality in the Atmega 328 and go into deep sleep between reporting periods (every ~55sec). 

### Here are some samples of the prototyping

![Schematic of rain_sensor hardware](https://github.com/epkboan/epkboan.github.io/blob/master/rain_sensor_schem.png?raw=true "Schematic: rain_sensor Hardware")

![Breadboard of rain_sensor](https://github.com/epkboan/epkboan.github.io/blob/master/rain_sensor_bb.png?raw=true "Breadboard: rain_sensor Hardware")

The Hardware is a tipping bucket from Rainwise 111. It's pretty pricy, but after trying numerous cheaper solutions (including the Oregon PCR800) i have concluded that it is worth every penny!  

### Here is the final product

![rain_sensor](https://github.com/epkboan/epkboan.github.io/blob/master/rain_sensor_1.JPG?raw=true)

![rain_sensor](https://github.com/epkboan/epkboan.github.io/blob/master/rain_sensor_2.JPG?raw=true)

![rain_sensor](https://github.com/epkboan/epkboan.github.io/blob/master/rain_sensor_3.JPG?raw=true)

![rain_sensor](https://github.com/epkboan/epkboan.github.io/blob/master/rain_sensor_4.JPG?raw=true)

