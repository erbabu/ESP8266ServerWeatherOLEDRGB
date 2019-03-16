/**The MIT License (MIT)

Copyright (c) 2018 by Daniel Eichhorn - ThingPulse

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at https://thingpulse.com

Libraries, Inspirations and Sample code from:
https://github.com/ThingPulse/esp8266-weather-station
http://microcontrollerkits.blogspot.com/2016/05/esp8266-wifi-control-rgbled.html

Update SSID, Password, OpenweatherAPIKey, OpenweatherCityCode

Keep serial monitor window open before uploading the sketch to esp8266
Find out the web server ip address. Browse to that IP address from a web browser (desktop / mobile).
RGB LED (Common Anode) values can be changed either in the provided text boxes and click button with text TestRGB (or) pick a color from the color canvas.
    If you have Common Cathode RGB LED, you might have to change code - one such reference: https://create.arduino.cc/projecthub/techmirtz/using-common-cathode-and-common-anode-rgb-led-with-arduino-7f3aa9
*/

#include <ESPWiFi.h>
#include <ESPHTTPClient.h>
#include <JsonListener.h>
// time
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
//oled
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
//openweather
#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"
//webserver
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>


/***************************
 * Begin Settings
 * Update SSID, Password, OpenweatherAPIKey, OpenweatherCityCode
 **************************/

// WIFI
const char* WIFI_SSID = "yourssid";
const char* WIFI_PWD = "yourpassw0rd";

#define TZ              -8       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries

// Setup
const int UPDATE_INTERVAL_SECS = 20 * 60; // Update every 20 minutes

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
#if defined(ESP8266)
const int SDA_PIN = D3;
const int SDC_PIN = D4;
#else
const int SDA_PIN = 5; //D3;
const int SDC_PIN = 4; //D4;
#endif


// OpenWeatherMap Settings
// Sign up here to get an API key:
// https://docs.thingpulse.com/how-tos/openweathermap-key/
String OPEN_WEATHER_MAP_APP_ID = "XXX";
/*
Go to https://openweathermap.org/find?q= and search for a location. Go through the
result set and select the entry closest to the actual location you want to display 
data for. It'll be a URL like https://openweathermap.org/city/2657896. The number
at the end is what you assign to the constant below.
 */
String OPEN_WEATHER_MAP_LOCATION_ID = "5786882";

// Pick a language code from this list:
// Arabic - ar, Bulgarian - bg, Catalan - ca, Czech - cz, German - de, Greek - el,
// English - en, Persian (Farsi) - fa, Finnish - fi, French - fr, Galician - gl,
// Croatian - hr, Hungarian - hu, Italian - it, Japanese - ja, Korean - kr,
// Latvian - la, Lithuanian - lt, Macedonian - mk, Dutch - nl, Polish - pl,
// Portuguese - pt, Romanian - ro, Russian - ru, Swedish - se, Slovak - sk,
// Slovenian - sl, Spanish - es, Turkish - tr, Ukrainian - ua, Vietnamese - vi,
// Chinese Simplified - zh_cn, Chinese Traditional - zh_tw.
String OPEN_WEATHER_MAP_LANGUAGE = "en";
const uint8_t MAX_FORECASTS = 4;

const boolean IS_METRIC = false;

// Adjust according to your language
const String WDAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const String MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

/***************************
 * End Settings
 **************************/
 // Initialize the oled display for address 0x3c
 // sda-pin=14 and sdc-pin=12
 SSD1306Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
 OLEDDisplayUi   ui( &display );

//web server
ESP8266WebServer server(80);

//RGB Common Anode LED
//Code needs to be modified for Common Cathode RGB LED
//   example: https://create.arduino.cc/projecthub/techmirtz/using-common-cathode-and-common-anode-rgb-led-with-arduino-7f3aa9
const int R = 14;
const int G = 12;
const int B = 13;

//openweather
OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapCurrent currentWeatherClient;

OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
OpenWeatherMapForecast forecastClient;

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
time_t now;

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;

String lastUpdate = "--";

long timeSinceLastWUpdate = 0;

//declaring prototypes
void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();


// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawForecast };
int numberOfFrames = 3;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;

//webserver and RGB
void handleRoot() {

  String red = server.arg(0);          // read RGB arguments
  String green = server.arg(1);
  String blue = server.arg(2);

  analogWrite(R, red.toInt());
  analogWrite(G, green.toInt());
  analogWrite(B, blue.toInt());

  Serial.println(red.toInt());   // for TESTING
  Serial.println(green.toInt()); // for TESTING
  Serial.println(blue.toInt());  // for TESTING

/* //since we have weather forecast for display, commenting this code
  display.clear();
  display.display();
  display.drawString(20,0,"R = "+red);
  display.drawString(20,10,"G = "+green);
  display.drawString(20,20,"B = "+blue); 
  display.drawString(50,30,ssid); 
  display.drawString(50,40,WiFi.localIP().toString()); 
  display.display();
*/

  if (red=="0" and green=="0" and blue=="0"){
    testRGB();
  }

  String webpage;     
      webpage += "<!DOCTYPE HTML>\r\n";
      webpage += "<html>\r\n";
      webpage += "<header><title>ESP8266 RGB Common Anode LED</title><h1>ESP8266 RGB Common Anode LED</h1></header>";
      
      webpage += "<head>";    
      webpage += "<meta name='mobile-web-app-capable' content='yes' />";
      webpage += "<meta name='viewport' content='width=device-width' />";
      webpage += "</head>";

      webpage += "<body style='margin: 0px; padding: 0px;'>";
      webpage += "Red:<input type='text' id='red' value='600' style='width:40px;'>&nbsp;";
      webpage += "Green:<input type='text' id='green' value='600' style='width:40px;'>&nbsp;";
      webpage += "Blue:<input type='text' id='blue' value='600' style='width:40px;'>&nbsp;";
      webpage += "<button id='testrgb'>Test RGB</button><br/><p>RGB(0,0,0) runs test mode. RGB(1023,1023,1023) is OFF. Red(0,1023,1023).</p><br/>";
      webpage += "<canvas id='colorspace'></canvas></body>";
     
      webpage += "<script type='text/javascript'>";
      webpage += "document.getElementById('testrgb').addEventListener('click', function(){";
      webpage += "var params = ['r=' + document.getElementById('red').value,'g=' + document.getElementById('green').value,'b=' + document.getElementById('blue').value].join('&');";
      webpage += "var req = new XMLHttpRequest();req.open('POST', '?' + params, true);req.send();});";
      webpage += "</script>";

      webpage += "<script type='text/javascript'>";
      webpage += "(function () {";
      webpage += " var canvas = document.getElementById('colorspace');";
      webpage += " var ctx = canvas.getContext('2d');";
      webpage += " function drawCanvas() {";
      webpage += " var colours = ctx.createLinearGradient(0, 0, window.innerWidth, 0);";
      webpage += " for(var i=0; i <= 360; i+=10) {";
      webpage += " colours.addColorStop(i/360, 'hsl(' + i + ', 100%, 50%)');";
      webpage += " }";
      
      webpage += " ctx.fillStyle = colours;";
      webpage += " ctx.fillRect(0, 0, window.innerWidth, window.innerHeight);";
      webpage += " var luminance = ctx.createLinearGradient(0, 0, 0, ctx.canvas.height);";
      webpage += " luminance.addColorStop(0, '#ffffff');";
      webpage += " luminance.addColorStop(0.05, '#ffffff');";
      webpage += " luminance.addColorStop(0.5, 'rgba(0,0,0,0)');";
      webpage += " luminance.addColorStop(0.95, '#000000');";
      webpage += " luminance.addColorStop(1, '#000000');";
      webpage += " ctx.fillStyle = luminance;";
      webpage += " ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);";
      webpage += " }";
      webpage += " var eventLocked = false;";
      
      webpage += " function handleEvent(clientX, clientY) {";
      webpage += " if(eventLocked) {";
      webpage += " return;";
      webpage += " }";
      
      webpage += " function colourCorrect(v) {";
      webpage += " return Math.round(1023-(v*v)/64);";
      webpage += " }";
      webpage += " var data = ctx.getImageData(clientX, clientY, 1, 1).data;";
      webpage += " var params = [";
      webpage += " 'r=' + colourCorrect(data[0]),";
      webpage += " 'g=' + colourCorrect(data[1]),";
      webpage += " 'b=' + colourCorrect(data[2])";
      webpage += " ].join('&');";
      webpage += " var req = new XMLHttpRequest();";
      webpage += " req.open('POST', '?' + params, true);";
      webpage += " req.send();";
      webpage += " eventLocked = true;";
      webpage += " req.onreadystatechange = function() {";
      webpage += " if(req.readyState == 4) {";
      webpage += " eventLocked = false;";
      webpage += " }";
      webpage += " }";
      webpage += " }";
      webpage += " canvas.addEventListener('click', function(event) {";
      webpage += " handleEvent(event.clientX, event.clientY, true);";
      webpage += " }, false);";
      webpage += " canvas.addEventListener('touchmove', function(event){";
      webpage += " handleEvent(event.touches[0].clientX, event.touches[0].clientY);";
      webpage += "}, false);";
      webpage += " function resizeCanvas() {";
      webpage += " canvas.width = window.innerWidth;";
      webpage += " canvas.height = window.innerHeight;";
      webpage += " drawCanvas();";
      webpage += " }";
      
      webpage += " window.addEventListener('resize', resizeCanvas, false);";
      webpage += " resizeCanvas();";
      webpage += " drawCanvas();";
      webpage += " document.ontouchmove = function(e) {e.preventDefault()};";
      webpage += " })();";   
      webpage += "</script><html>\r\n";

      server.send(200, "text/html", webpage);    
}


void testRGB() { // fade in and out of Red, Green, Blue
    analogWrite(R, 1023);     // R off
    analogWrite(G, 1023);     // G off
    analogWrite(B, 1023);     // B off 
    fade(R); // R
    fade(G); // G
    fade(B); // B
}

void fade(int pin) {
    for (int u = 0; u < 1024; u++) {
      analogWrite(pin,  1023 - u);
      delay(1);
    }
    for (int u = 0; u < 1024; u++) {
      analogWrite(pin, u);
      delay(1);
    }
}

void handleNotFound(){
  //digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  //digitalWrite(led, 0);
}

void setup() {
  //RGB
  pinMode(R, OUTPUT);
  pinMode(G, OUTPUT);
  pinMode(B, OUTPUT);

  analogWrite(R, 600);     // R 
  analogWrite(G, 600);     // G 
  analogWrite(B, 600);     // B 

  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // initialize dispaly
  display.init();
  display.clear();
  display.display();

  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  WiFi.begin(WIFI_SSID, WIFI_PWD);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connecting to WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();

    counter++;
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(WIFI_SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Get time from network time service
  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");

  ui.setTargetFPS(30);

  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  ui.setFrames(frames, numberOfFrames);

  ui.setOverlays(overlays, numberOfOverlays);

  // Inital UI takes care of initalising the display too.
  ui.init();

  Serial.println("");

  updateData(&display);

  //webserver and rgb setup
  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }
  server.on("/", handleRoot);
  testRGB();
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

}

void loop() {

  if (millis() - timeSinceLastWUpdate > (1000L*UPDATE_INTERVAL_SECS)) {
    setReadyForWeatherUpdate();
    timeSinceLastWUpdate = millis();
  }

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);
  }

  //webserver
  server.handleClient();

}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

void updateData(OLEDDisplay *display) {
  drawProgress(display, 10, "Updating time...");
  drawProgress(display, 30, "Updating weather...");
  Serial.println("Updating time and weather...");
  currentWeatherClient.setMetric(IS_METRIC);
  currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  currentWeatherClient.updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID);
  drawProgress(display, 50, "Updating forecasts...");
  Serial.println("Updating forecasts...");
  forecastClient.setMetric(IS_METRIC);
  forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  uint8_t allowedHours[] = {12};
  forecastClient.setAllowedHours(allowedHours, sizeof(allowedHours));
  forecastClient.updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);

  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  Serial.println("Done...");
  delay(1000);
}



void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[16];


  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = WDAY_NAMES[timeInfo->tm_wday];

  sprintf_P(buff, PSTR("%s, %02d/%02d/%04d"), WDAY_NAMES[timeInfo->tm_wday].c_str(), timeInfo->tm_mday, timeInfo->tm_mon+1, timeInfo->tm_year + 1900);
  display->drawString(64 + x, 5 + y, String(buff));
  display->setFont(ArialMT_Plain_24);

  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  display->drawString(64 + x, 15 + y, String(buff));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, currentWeather.description);

  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(60 + x, 5 + y, temp);

  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(32 + x, 0 + y, currentWeather.iconMeteoCon);
}


void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 1);
  drawForecastDetails(display, x + 88, y, 2);
}

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  time_t observationTimestamp = forecasts[dayIndex].observationTime;
  struct tm* timeInfo;
  timeInfo = localtime(&observationTimestamp);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y, WDAY_NAMES[timeInfo->tm_wday]);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, forecasts[dayIndex].iconMeteoCon);
  String temp = String(forecasts[dayIndex].temp, 0) + (IS_METRIC ? "°C" : "°F");
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, temp);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[14];
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);

  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(128, 54, temp);
  display->drawHorizontalLine(0, 52, 128);
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}
