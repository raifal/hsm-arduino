HSM - Heating System Monitoring
-------------------------------

for Arduino Uno with DS18B20 temperature sensors
- Version 1.5
- Created: 17/MAY/2014

Arduino IDE Setup:
- Download

Install the Libraries in Arduino IDE:
- TemperatureControl (Downloaded from https://github.com/PaulStoffregen/OneWire/releases -> 2.3.3 -> zipfile)
- OneWire (Downloaded form https://github.com/milesburton/Arduino-Temperature-Control-Library/releases -> 3.7.6 -> zipfile)

Features:
- Supports Two 1-Wire Buses for temperature measurement
- Supports water leak level alarm

Time measurement is done by counting loops, which is not really accurate at all.

Update V 1.4: 
- Includes a water leak detection

Update V 1.5: 
- Includes a second water leak detection 
