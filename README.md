# ESP8266 WebServer OpenWeather OLED RGBcommonanodeLED
This project uses ESP8266 WiFi Module to control RGB LED via WiFi Network.
We will also read weather forecast for a city and display those details in OLED display.

Hardware   
1.ESP8266 WiFi Module ( ESP12E - NodeMCU LoLin v3 )   
2.RGB Common Anode LED   
3.OLED Display (SSD1306)

Wiring Diagram  
ESP8266 Pin GPIO_14 to LED R //D5
ESP8266 Pin GPIO_12 to LED G //D6
ESP8266 Pin GPIO_13 to LED B //D7
ESP8266 Pin 3.3v to LED Common Anode

ESP8266 Pin 3.3v to OLED VCC
ESP8266 Pin Gnd to OLED Gnd
ESP8266 Pin GPIO_0 to OLED SDA //D3
ESP8266 Pin GPIO_2 to OLED SCL //D4

Libraries, Inspirations and Sample code from:
https://github.com/ThingPulse/esp8266-weather-station
http://microcontrollerkits.blogspot.com/2016/05/esp8266-wifi-control-rgbled.html

ToDo: Update SSID, Password, OpenweatherAPIKey, OpenweatherCityCode

Suggestions:
* Keep serial monitor window open with correct baud rate before uploading the sketch to esp8266
* Selected city datetime, Current Weather and 3 day forecast displays in OLED display
* Find out the web server ip address in serial monitor window. Browse to that IP address from a web browser (desktop / mobile).
* RGB LED (Common Anode) values can be changed either in the provided text boxes and click button with text TestRGB (or) pick a color from the color canvas.
  Note: If you have Common Cathode RGB LED, you might have to change code - one such reference: https://create.arduino.cc/projecthub/techmirtz/using-common-cathode-and-common-anode-rgb-led-with-arduino-7f3aa9

![ESP8266 OLED RGB Hardware Setup](https://github.com/erbabu/ESP8266ServerWeatherOLEDRGB/blob/master/Setup-ESP8266OLEDRGB.jpg)

 BLOG
 https://erbabu.wordpress.com/2019/02/13/compare-arduino-raspberry-pi/
