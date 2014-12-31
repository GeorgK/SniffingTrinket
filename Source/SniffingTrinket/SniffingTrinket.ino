/**************************************************************************/
/*!
    @file     SniffingTrinket.ino
    @author   G.Krocker (Mad Frog Labs)
    @license  GNU GPLv3
 
    Arduino (TrinketPro) sketch for SniffingTrinket
 
    @section  HISTORY
 
    v1.0 - First release
*/
/**************************************************************************/

#include "DHT.h"
#include "MQ135.h"
#include "Adafruit_NeoPixel.h"
#include "TrinketKeyboard.h"

// Define on which pins the rest of the circuit is connected
#define LEDPIN 6
#define DHTPIN 5     // what pin we're connected to
#define ANALOGPIN 5
#define BUTTONPIN 8
#define SWITCHPIN 4
#define ALARMPIN 9

// Some more intuitive shortcuts for the LEDs
#define TEMPLED 0
#define HUMLED 1
#define AIRLED 2

// Define brightness levels
#define HIGHLED 150
#define LOWLED 25

// Define timeout in ms
#define TIMEOUT 3000

// Alarmlevels
#define TEMPALARM 30
#define HUMALARM 65
#define CO2ALARM 1000

// Set the low, mid and high thresolds for the LEDS
// T in deg Celsius
float tempthreshold[3] = {
  15, 20, 25};
// CO2 in ppm
float airthreshold[3] = {
  400, 400, 800};
// Rel Humidity in percent
float humthreshold[3] = {
  35, 45, 60};

// Initialize the RGB LEDs
// If you are not using the PCB, you might need to change these settings
Adafruit_NeoPixel strip = Adafruit_NeoPixel(3, LEDPIN, NEO_GRB + NEO_KHZ800);

// Define the type of sensor used, needs to be changed if different DHT sensor
// is in use - see adafruit DHT library example
#define DHTTYPE DHT11   // DHT 11 

// Initialize DHT sensor for normal 16mhz Arduino
DHT dht(DHTPIN, DHTTYPE);

// Initialize the gas Sensor
MQ135 gasSensor = MQ135(ANALOGPIN);

// make sure the state of the button is initialize correctly
int buttonState = 0;
// This defines the modes
// The different modes are indicated by the LEDs, up to 2^3 - 1 = 7 modes are
// supported
// 1      standard Mode
// 2      high Power LED mode
// 3      USB Keyboard output
// define more modes at your will
char mode = 1;
char maxmode = 3;

/**************************************************************************/
/*!
@brief  Setup function

Make sure everything is set up correctly
*/
/**************************************************************************/
void setup() {
  // start the LEDs
  strip.begin();
  strip.setBrightness(LOWLED);
  strip.show(); // Initialize all pixels to 'off'
  
  //Set up the serial terminal
  Serial.begin(9600); 

  // start the humidity/temp sensor
  dht.begin();
  
  // Set up the push button
  pinMode(BUTTONPIN, INPUT_PULLUP);
  
  // Set up the heating of the gas sensor
  pinMode(SWITCHPIN, OUTPUT);
  // We start with the heater ON
  digitalWrite(SWITCHPIN, HIGH);
  
  // Start USB Keyboard
  TrinketKeyboard.begin();
  
}

/**************************************************************************/
/*!
@brief  Controls the color of the LEDs based on the sensor values

@param[in] value  The sensor value
@param[in] led    Which LED to use (0,1 or 3)
*/
/**************************************************************************/
void setLedOutput(float value, int led) {
  // Holds the limits
  float low, mid, high;
  
  //Based on the type of measurement (T, Hum, CO2) fill the limits
  switch(led) {
  case TEMPLED:
    low = tempthreshold[0];
    mid = tempthreshold[1];
    high = tempthreshold[2];
    break;

  case HUMLED:
    low = humthreshold[0];
    mid = humthreshold[1];
    high = humthreshold[2];
    break;

  case AIRLED:
    low = airthreshold[0];
    mid = airthreshold[1];
    high = airthreshold[2];  
    break;
  }

  // Calculate the actual RGB values from the sensor reading
  //Blue is fully on if value <= low and 0 if value = mid
  int blue = map(value, low, mid, 255, 0);
  //RED is fully on if value >= high and 0 if value = mid
  int red = map(value, mid, high, 0, 255);
  //GREEN is fully on if value=mid and gets dimmed down towards low/high
  int green = 0;
  if(value<=mid)
    green = map(value, low, mid, 0, 255);
  else
    green = map(value, mid, high, 255, 0);

  //the map function is somehow broken, make sure we only have values from 0 to 255
  if(red < 0) red=0;
  if(green < 0) green=0;
  if(blue < 0) blue=0;
  if(red > 255) red=255;
  if(green > 255) green=255;
  if(blue > 255) blue=255;
  
  
  // A too low C02 value does not make sense, so we only fade from Green to RED, no blue
  if(led==AIRLED) {
    blue = 0;
    if(value <= low) green = 255;
  }

  // Set the corresponding LED to the calculated color
  strip.setPixelColor(led, red, green, blue);
  strip.show();
  
  //Some debug output in case you want to know the RGB values

#if 0
  Serial.print(led);
  Serial.print(" ");
  Serial.print(red);
  Serial.print(" ");
  Serial.print(green);
  Serial.print(" ");
  Serial.println(blue);
#endif
}

/**************************************************************************/
/*!
@brief Change the mode

*/
/**************************************************************************/
void setMode() {
  // go to next mode
  mode += 1;
  // check if we reached the last mode
  if(mode > maxmode) mode = 1;
  
  // First, turn all LEDs of
  strip.setPixelColor(0, 0, 0, 0);
  strip.setPixelColor(1, 0, 0, 0);
  strip.setPixelColor(2, 0, 0, 0);
  
  // Turn them on to indicate mode in binary
  if(mode & (1<<0)) strip.setPixelColor(2, 255, 255, 255);
  if(mode & (1<<1)) strip.setPixelColor(1, 255, 255, 255);
  if(mode & (1<<2)) strip.setPixelColor(0, 255, 255, 255);
  
  // Do some special things
  if(mode!=2) strip.setBrightness(LOWLED);
  if(mode==2) strip.setBrightness(HIGHLED);
  strip.show();
  
}

/**************************************************************************/
/*!
@brief  The main loop

*/
/**************************************************************************/
void loop() {
  // Wait a few seconds between measurements.
  // TODO: This is not ideal - better put the Arduino to sleep here
  delay(TIMEOUT);

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit - in case you still don't use the SI
  //float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }


  // Read out the Gas Sensor
  float ppm = gasSensor.getPPM();
  // Do not use temperature/humidity correction, it is broken!!!
  //float ppm = gasSensor.getCorrectedPPM(t, h);
  // TODO: Implement some sanity check here!

  // Set the LEDs accordingly
  setLedOutput(t, TEMPLED);
  setLedOutput(h, HUMLED);
  setLedOutput(ppm, AIRLED);
  
  // Check if the alarm level has been reached
  if(t > TEMPALARM ||
    h > HUMALARM ||
    ppm > CO2ALARM)
    tone(ALARMPIN, 5000, 1000);

  // Read if the button was pressed
  buttonState = digitalRead(BUTTONPIN);
  if(!buttonState) setMode();
  //TODO: Better to do this with an Interrupt?!?
  
  // Print the measurements to the serial port
  Serial.print("T: "); 
  Serial.print(t);
  Serial.print(" *C\t");
  Serial.print("H: "); 
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("CO2: ");
  Serial.print(ppm);
  Serial.println(" ppm");
  
  if(mode==3) {
    TrinketKeyboard.print("T "); 
    TrinketKeyboard.print(t);
    TrinketKeyboard.print(" C\t");
    TrinketKeyboard.print("H "); 
    TrinketKeyboard.print(h);
    TrinketKeyboard.print(" %\t");
    TrinketKeyboard.print("CO2 ");
    TrinketKeyboard.print(ppm);
    TrinketKeyboard.println(" ppm");
  }

}



