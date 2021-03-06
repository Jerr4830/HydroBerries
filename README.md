﻿# HydroBerries
Arduino Software Setup

Ethernet
1.	Set MAC variable to unique MAC address
a.	byte mac[] = {  0x90, 0xA2, 0xDA, 0x11, 0x19, 0x19};
b.	MAC addresses can be found taped underneath the Ethernet Shield
2.	Set IP variable to unique IP address
a.	IPAddress ip(129,21,63,152);
b.	This ensures that the shield will initialize the ethernet connection if DHCP configuration fails
3.	If configuring server module:
a.	Create server module with unique port:
i.	EthernetServer server(80);
ii.	80 is the port number
4.	If configuring client module:
a.	set an unique UDP port

Email
1.	If you don’t have a SMTP account:
  a.	Create one at https://www.smtp2go.com/
2.	Username and password must be base64 encoded with utf-8 character set:
  a.	Go to https://www.base64encode.org/
  b.	Type username then copy result to the following line
    i.	// change to base64 encoded user
    ii.	emailClient.println("xxxx");
      1.	xxxx must be replaced by encoded username
  c.	Type password then copy result to the following line
    i.	// base64 encode password
    ii.	emailClient.println("yyyy");
      1.	yyyy must be replaced by encoded password
3.	Set the recipient email:
  a.	emailClient.println("To: <recipient’s email>");
4.	Set the sender’s email:
  a.	emailClient.println("From: <sender’s email>");
Final Setup
1.	Set unique module id:
  a.	String MODULE_NUMBER = "A";
  b.	Replace “A” by unique ID number


Arduino Pin Setup

1. Temperature (must have a 4.7K pull-up resistor)
  a. Power Pin					A3
  b. Data Pin 					A4
  c. Ground Pin					A5

2. Electrical Conductivity Sensor (must have a 1K pull-up resistor)
  a. Power Pin					A1
  b. Data Pin 					A2
  c. Ground	Pin					Arduino ground pin
3. Dissolved Oxygen Sensor
  a. BNC connection to Tentacle mini shield
4. pH Sensor
  a. BNC connection to Tentacle mini shield
5. Level Sensor
  a. Power Pin					A12
  b. ECHO Pin			  		A14
  c. TRIGGER Pin  			A13
  d. Ground Pin					A15
6. Flow Sensor
  a. Power Pin					Arduino 5V pin
  b. Data Pin 					A0
  c. Ground Pin					Arduino Ground Pin

References
1. Temperature Sensor
  a.	Sensor Website: https://www.sparkfun.com/products/11050
  b.	Example Code: https://github.com/sparkfun/simple_sketches/tree/master/DS18B20 
2. Electrical Conductivity 
  a.	Example Code: https://hackaday.io/project/7008-fly-wars-a-hackers-solution-to-world-hunger/log/24646-three-dollar-ec-ppm-meter-arduino 
3. Dissolved Oxygen Sensor
  a.	Sensor Website: https://www.atlas-scientific.com/dissolved-oxygen.html 
  b.	Example Code: https://www.atlas-scientific.com/_files/code/do-i2c.pdf 
4. pH Sensor
  a.	Sensor Website: https://www.atlas-scientific.com/ph.html 
  b.	Example Code: https://www.atlas-scientific.com/_files/code/ph-i2c.pdf  
5. Level Sensor
  a.	Sensor Website: https://www.robotshop.com/en/weatherproof-ultrasonic-sensor-separate-probe.html?gclid=EAIaIQobChMIzPi-z5Di1wIVFyWBCh0cnwzoEAkYBSABEgLe_fD_BwE 
  b.	Example Code : https://github.com/RobotShop/RB-Dfr-720_Weatherproof_Ultrasonic_Sensor_with_Separate_Probe 
6. Flow Sensor
  a.	Sensor Website: https://www.adafruit.com/product/828 
  b.	Example Code:  https://github.com/adafruit/Adafruit-Flow-Meter 
7. Email Example Code
  a.	https://playground.arduino.cc/Code/Email 
8. Ethernet Library Example
  a.	Web Server: https://www.arduino.cc/en/Tutorial/WebServer
  b.	Web Client: https://www.arduino.cc/en/Tutorial/WebClient 
  c.	Use Ethernet2.h instead of Ethernet.h for the ethernet library
