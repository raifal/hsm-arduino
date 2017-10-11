# HSM - Heating System Monitoring

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


# Technical Solution
Several components are included in order to realize this project. We start with an overview of the architecture and continue to focus on the arduino board. 

## Architecture
![](http://www.rainer-faller.de/arduino_pics/mp.png | width=600 )

The internet server contains a database with temperature values. The temperature values are sent by the monitor installed at home. The outside temperature is collected by a weather station. A website exists that an be viewed with the laptop, just as you do right now. A arduino is responsible for collecting the temperature values at home. 

## Arduino Parts
A number of parts are required in order to setup the arduino. Please find below a list of components with a link of a possible shop. 

<table>
	<tr>
		<th>Quantity</th>
		<th>Product</th>
		<th>Possible Shop URL</th>
		<th>Price</th>
	</tr>
	<tr>
		<td>1</td>
		<td>Arduino UNO Rev3</td>
		<td><a href="http://store.arduino.cc/index.php?main_page=product_info&cPath=11&products_id=195">Arduino.cc</a></td>
		<td>EUR 23,80</td>
	</tr>
	<tr>
		<td>1</td>
		<td>Arduino Ethernet Shield Rev3 WITHOUT PoE Module</td>
		<td><a href="http://store.arduino.cc/index.php?main_page=product_info&cPath=37_5&products_id=199">Arduino.cc</a></td>
		<td>EUR 34,51</td>
	</tr>	
	<tr>
		<td>10</td>
		<td>1m Waterproof Digital Thermal Temperature Temp Sensor Probe DS18B20 Connector</td>
		<td><a href="http://r.ebay.com/oJ5Xta">sureelectronics (Ebay)</a></td>
		<td>EUR 13,72</td>
	</tr>
	<tr>
		<td>5</td>
		<td>5m Waterproof Digital Thermal Temperature Temp Sensor Probe DS18B20 Connector</td>
		<td><a href="http://r.ebay.com/ZWwbxL">sureelectronics (Ebay)</a></td>
		<td>EUR 18,69</td>
	</tr>
	<tr>
		<td>1</td>
		<td>Breadboard</td>		
		<td><a href="http://www.amazon.de/dp/B009P04XG8">Amazon Shop</a></td>
		<td>EUR 2,67</td>
	</tr>
	<tr>
		<td>1</td>
		<td>9V Netzteil</td>
		<td><a href="http://www.amazon.de/dp/B008QXOMQQ">Amazon Shop</a></td>
		<td>EUR 9,99</td>
	</tr>
	<tr>
		<td>1</td>
		<td>Male Header Pins (Height: 19.8mm)</td>		
		<td><a href="http://www.conrad.de/ce/de/product/741133/Stiftleiste-RM-254-gerade-Pole-1-x-36-10120510-BKL-Electronic-Inhalt-1-St">Conrad</a></td>
		<td>EUR 0,97</td>
	</tr>
	<tr>
		<td>3</td>
		<td>220 Ohm Resistor</td>		
		<td>&nbsp;</td>
		<td>few cents</td>
	</tr>
	<tr>
		<td>1</td>
		<td>10 kOhm Resistor</td>		
		<td>&nbsp;</td>
		<td>few cents</td>
	</tr>
	<tr>
		<td>2</td>
		<td>4.7 kOhm Resistor</td>		
		<td>&nbsp;</td>
		<td>few cents</td>
	</tr>	
	<tr>
		<td>1</td>
		<td>Push Button</td>		
		<td>&nbsp;</td>
		<td>few cents</td>
	</tr>
	<tr>
		<td>3</td>
		<td>LEDs, different colors</td>		
		<td>&nbsp;</td>
		<td>few cents</td>
	</tr>	
	<tr>
		<td colspan="3" align="right">Total without shipping</td>
		<td>EUR 104,35</td>
	</tr>		
</table>

 

 

# Arduino Board Setup
As the arduino uno does not have a possibility to connect to the internet, an ethernet shield is required. The ethernet shield is not shown in the next diagram.

![](http://www.rainer-faller.de/arduino_pics/arduino_frizzing_v1_3.png  )

The temperature sensors are connected using the 1-Wire bus protocol. There are actually two busses installed. The reason is that some temperature sensors required a long cable (up to 10 meters). It seems like the power of the arduino is limited and only a sum of temperature sensors cable length of about 25 meters is possible to be connected to the bus. Therefore the board supports to busses in order to handle longer cable length. 
The board contains a push button. This push button can be used to send the data immediatly to the internet. A red led indicates that the button push was recognized. 
The green led changes its state with every loop done. One loop is delayed by one second, so the led changes about every second.
The yellow led behaves the same way as the green leed, but changes its state when data is sent to the internet server.

# Pictures


 <img src="http://www.rainer-faller.de/arduino_pics/pic_arduino_3.jpg" width="300" alt="Arduino with connected sensors 1">           
 <img src="http://www.rainer-faller.de/arduino_pics/pic_arduino_1.jpg" width="300" alt="Arduino with connected sensors 2">
 <img src="http://www.rainer-faller.de/arduino_pics/pic_arduino_2.jpg" width="300" alt="Arduino with connected sensors 3">
 <img src="http://www.rainer-faller.de/arduino_pics/pic_arduino_4.jpg" width="300" alt="Arduino with connected sensors 4">
 <img src="http://www.rainer-faller.de/arduino_pics/pic_arduino_5.jpg" width="300" alt="Arduino with connected sensors 5">
 <img src="http://www.rainer-faller.de/arduino_pics/pic_arduino_6.jpg" width="300" alt="Arduino & Ethernet Shield">
 <img src="http://www.rainer-faller.de/arduino_pics/pic_sensor.jpg" width="300" alt="Sensor DS18B20">
 <img src="http://www.rainer-faller.de/arduino_pics/pic_transferstation2.jpg" width="300" alt="Transferstation with sensors 1">
 <img src="http://www.rainer-faller.de/arduino_pics/transferstation_zoom.jpg" width="300" alt="Transferstation with sensors 2">
 <img src="http://www.rainer-faller.de/arduino_pics/pic_solar.jpg" width="300" alt="Solarstation with sensors">
 <img src="http://www.rainer-faller.de/arduino_pics/pic_arduino_8.jpg" width="300" alt="Arduino in the Box 1">   
 <img src="http://www.rainer-faller.de/arduino_pics/pic_arduino_7.jpg" width="300" alt="Arduino in the Box 2">   