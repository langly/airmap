#include <WiFi.h>
#include <HTTPClient.h>
#include <CSV_Parser.h>
#include <FastLED.h>

#include "config.hpp"

/// Set up your wifi details
char *SSID= WIFI_SSID;
char *PWD = WIFI_PASS;

/// Configure the airports you're interested in here.
// Changing LNS/ Los banos to Merced since it's the closest one reporting a metar.
const String airports[] = {"KMRY", "KSNS", "KMCE", "KMOD", "KSCK", "KAPC", "KO69", "KHAF","KPAO","KHWD", "KLVK"};
const int num_airports = 11;

// How often to poll the metars in minutes.
// The ESP32 will go into deep sleep for these minutes. 
const int poll_freq_in_minutes = 10;

// Which pin on your board is the led out connected to. 
const int DATA_PIN = 27;

//// Some FastLed constants
CRGB leds[num_airports];

void setLed(String ident, String cat){

  Serial.println(ident);
  Serial.println(cat);

  CRGB col = CRGB::Yellow;

  // Very simple decoding for now. 
  if ( cat == "VFR"){
    col = CRGB::Green;
  } else {
    col = CRGB::Red;
  }
  
  for ( int i = 0; i < num_airports; i++){
    if ( ident == airports[i] ){
      leds[i] = col;
      return;
    }
  }
  Serial.println("Airport" + ident + " not configured");
}

void fetchMetars(int reset){
  FastLED.setBrightness(64);
  if (reset){
    for ( int i = 0; i < num_airports+1;i++){
      leds[i] = CRGB::DimGrey;
    }
    FastLED.show();
  }

  String server_base = "https://www.aviationweather.gov/adds/dataserver_current/httpparam?datasource=metars&requestType=retrieve&format=csv&mostRecentForEachStation=constraint&hoursBeforeNow=1.25&stationString=";
  String idents = "";

  for ( int i = 0; i < num_airports; i++){
    idents += airports[i] + ",";
  }

  server_base += idents;
  
  HTTPClient http;

  Serial.println("Fetching webpage" + server_base);
  http.begin(server_base.c_str());
  int httpResponseCode = http.GET();

  if ( httpResponseCode > 0 ){
    Serial.println("Downloading");
    String payload = http.getString();
    Serial.print(payload);
    Serial.println();

    // Find where the CSV file starts, and parse that.
    for( int i = 0; i < 5; i++ ){
        int idx = payload.indexOf('\n');
        payload = payload.substring(idx+1);
    }

    Serial.println(payload);

    // Now parse the CSV file
    // Assume everything is string for now. We do not care. We discard the first column being the RAW metar
    // TODO: The memory footprint here can be optimized quite a lot....
    String fields = "-sssssssssssssssssssssssssssssssssssssssssss";

    CSV_Parser cv(fields.c_str());
    cv << payload;

    // Go through the return data and set the led colours as a function of the Metar.
    for ( int row = 0; row < cv.getRowsCount(); row++ ){
      char **stations = (char**)cv["station_id"];
      char **flightCat = (char**)cv["flight_category"];
      
      String station = stations[row];
      String cat = flightCat[row];

      setLed(station, cat);
    }
    
  } else {
    Serial.println("Error"+ httpResponseCode);
  }


  FastLED.show();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFi.begin(SSID,PWD);

  pinMode(2,OUTPUT);
  digitalWrite(2,LOW);

  const int uS_TO_S = 1000000;
  const int TIME_TO_SLEEP = poll_freq_in_minutes * 60;
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP*uS_TO_S);

  FastLED.addLeds<WS2812, DATA_PIN>(leds, num_airports);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print('.');
  }

  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  fetchMetars(1);
  digitalWrite(2,HIGH);

  Serial.println("Going to sleep");
  esp_deep_sleep_start();
  Serial.println("We should not get here");
}

void loop() {
  // put your main code here, to run repeatedly:

}
