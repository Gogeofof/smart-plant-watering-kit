# smart plant watering kit

Arduino based, time-programming, sensor fault detection
comments within script in german
 
 ¨
functionality:

Set the RTC
example: "set 28.08.2013 10:54"
 
Read Sensor Values
detect Sensor - fault: open or short circuit, internal resistance of source
Mapping to scale 0..100

time program: Starttime to endtime 
multiple times possible
example: 6:00 Starttime – 8:00 endtime
purpose: water plant - wait for effect; repeat

manual (temporary) rh setpoint
example: "rH 40.40.40.40"
this sets all the 4 chanels to 40 %

