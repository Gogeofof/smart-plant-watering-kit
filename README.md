
smart plant watering kit


Arduino based, time-programming, sensor fault detection, 4-Channel
uses Elecrow smart-plant watering-kit
comments within script in german
 
 ¨
functionality:

Set the RTC
example: "set 28.08.2013 10:54"

time program: Starttime to endtime 
multiple times possible
example1: Channel_0, 6:00 Starttime – 6:10 endtime
{0, 600, 610},  // Ventil-Flag, Einschaltzeit, Ausschaltzeit
example2: Channel_1, 6:56 Starttime – 7:02 endtime
{1, 656, 702},

purpose: water plant - wait for effect; repeat


manual (temporary) rh setpoint
example: "rH 40.40.40.40"
this sets all the 4 chanels to 40 %  - for testing purpose


Read Sensor Values
detect Sensor - fault: open or short circuit, internal resistance of source
Mapping to scale 0..100


![IMG_5913](https://user-images.githubusercontent.com/90260140/132355500-4ea42fe9-124a-450e-bee8-da5104bc07ad.JPG)
![IMG_5914](https://user-images.githubusercontent.com/90260140/132355510-873d3b39-cca8-48e5-a6f5-e6aa1a370a4f.JPG)
![IMG_5915](https://user-images.githubusercontent.com/90260140/132355519-ad26adef-33c2-486f-8e92-0ae73d1c3f98.JPG)

