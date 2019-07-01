#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

//   Placa WeMos D1
//   Tiny RTC DS1307 I2C
//   LCD Crystal PCF8574A
//   Consulta GET ejemplo:  indicators/1013?start_date=09-06-2019T00%3A00&end_date=09-06-2019T23%3A00

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "config.h" // donde estan los datos sensibles
// WiFi Parameters
const char* ssid = SSID_NAME;
const char* password = WIFI_PASSWORD;
HTTPClient httpClient;  //Object of class HTTPClient

//API
const String SERVER_ESIOS = SERVER_API;
const String TOKEN = TOKEN_API;
const char* FINGERPRINT=FINGERPRINT_API;

const String TYPE = TYPE_DEFAULT;
const String PEAJE = PEAJE_DEFAULT;
//LCD
const int NUM_CHAR = 20;
const int NUM_LINES = 4;
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

//TINY RTC
RTC_DS3231 rtc; //los puertos son los mismos que la version anterior, solo cambia la precisión del hardware.
String strToday,strTime;
int analogBAT = A0; //leer estado de la bateria ADC

//Comprobacion ERRORES
boolean hasError;
String msgError;
//OTROS FLAGS
boolean _DEBUGGER;
unsigned int statusProgram;
unsigned long previousMillis;  // millis() returns an unsigned long.
void setup() {

  initValues();
    
  Serial.begin(115200);
  Wire.begin(); // Inicia el puerto I2C
  rtc.begin();
  DateTime today = rtc.now();
  if (!rtc.begin()) {
      if(_DEBUGGER == true) {Serial.println(F("No encuentro RTC"));}  
      hasError = true;
      msgError="No encuentro RTC";
      yield();      
      while (1);      
  }
  if (rtc.lostPower()) {
      initDate();
      msgError="Perdida potencia...";
      if(_DEBUGGER == true) {
        Serial.println("Perdida potencia...");
        Serial.println("Se ha adjustado la fecha");
      }
      delay(1000);  
  }

  if( String(today.year(),DEC) != "2019"){
    initDate();
    msgError="Valor de fabrica..."+String(today.year(),DEC) ;
    if(_DEBUGGER == true) {
      Serial.println("Tiene un valor de fabrica y se ha adjustado");      
    }
    delay(1000);    
  }  

  pinMode(analogBAT, OUTPUT);
  
  lcd.begin(NUM_CHAR,NUM_LINES);         // initialize the lcd for 20 chars 4 lines and turn on backlight
  lcd.backlight(); // backlight on 

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    yield();        
    Serial.println("Connecting...");
    delay(5000);
  }
}

void loop() {
  Serial.println("Error: "+ msgError);
  delay(5000); 
  /***     
   *      Comprobamos el valor de BAT del RTC
   */
   unsigned long currentMillis = millis(); // grab current time  
   if ((unsigned long)(currentMillis - previousMillis) >= CHECK_BAT) {
      previousMillis = millis();
      int valBAT = analogRead(analogBAT);  // read the input pin
      Serial.println("BAT: " + valBAT);
  }
  
  /***
   *  En cada vuelta asignamos el DateTime
   */  
  DateTime today = rtc.now(); // Obtiene la fecha y hora del RTC
  strToday = String(today.day(),DEC) + "-"+String(today.month(),DEC) + "-"+String(today.year(),DEC);
  strTime = String(today.hour(),DEC) + ":"+String(today.minute(),DEC) + ":"+String(today.second(),DEC);

  delay(1000);
  Serial.println("ESTADO OUTSIDE: "+statusProgram);
  delay(1000); 
  
  if ( WiFi.status() == WL_CONNECTED && statusProgram == 0) {
    statusProgram == 1;
    delay(1000);       
    Serial.println("ESTADO INSIDE: "+statusProgram);
    delay(1000); 
    //TODO: Activar un led para saber el estado del wifi.
    ///////////    
    String url = getURLIndicatorWithDateTime(strToday);
    if(_DEBUGGER == true) {Serial.println(url);}
    httpClient.begin(url,FINGERPRINT);
    
    
      httpClient.addHeader("Host", "api.esios.ree.es");
      httpClient.addHeader("Authorization", "Token token="+TOKEN);    
      httpClient.addHeader("Accept", "application/json; application/vnd.esios-api-v1+json");
      httpClient.addHeader("Content-Type", "application/json");
      //httpClient.addHeader("Cookie", "");
      int httpCodeResponse = httpClient.GET(); 
      
         
      if(_DEBUGGER == true) {Serial.println("HTTPCODE: " + httpCodeResponse); }
      
      if(!httpCodeResponse>0){
        hasError = true;
        msgError = httpClient.errorToString(httpCodeResponse);
        if(_DEBUGGER == true){Serial.println("Error code response: "+ msgError);}
      }

        //Check the returning code
      if (httpCodeResponse >0) {         
        // Get the request response payload      
        String payload = httpClient.getString();
        //if(_DEBUGGER == true){Serial.println("PAYLOAD: "+ payload);}         
        printPayLoadValues(payload);      
      }
    
        
    httpClient.end();   //Close connection
    if(_DEBUGGER == true){Serial.println("fin conexión");}    
  }
  else {
      Serial.printf("[HTTP} Unable to connect\n");
   }
}

void initValues(){
  
  strToday,strTime= "";
  analogBAT = A0; //leer estado de la bateria ADC

  //Comprobacion ERRORES
  hasError = false;
  msgError="No Error";
  //OTROS FLAGS
  _DEBUGGER = true;
  statusProgram= 0;
  previousMillis=0;  // millis() returns an unsigned long.
}

void printPayLoadValues(String payload){
  Serial.println("printPayLoadValues");
   // Inside the brackets is the capacity of the memory pool in bytes.
   // Don't forget to change this value to match your JSON document.
   // Use arduinojson.org/v6/assistant to compute the capacity.
   DynamicJsonDocument doc(13000) ;
    // Deserialize the JSON document
    yield();
    DeserializationError error = deserializeJson(doc, payload);
   // Test if parsing succeeds.
    if (error) {
      hasError=true;
      msgError=error.c_str();
      if(_DEBUGGER == true) {
        Serial.print("deserializeJson() failed: ");        
      }
      return;
    }
    const char* des = doc["indicator"]["short_name"];
    Serial.println(des);      
    JsonArray values = doc["indicator"]["values"];
    for (JsonObject val : values) {
      Serial.print(" Fecha ");
      Serial.print(val["datetime"].as<char *>());
      Serial.print(", valor: ");
      Serial.print(val["value"].as<long>()); 
      Serial.println(" ");     
   }
}

String getURLIndicatorWithDateTime(String hoy){
  //indicators/1013?start_date=09-06-2019T00%3A00&end_date=09-06-2019T23%3A00
  String url   = SERVER_ESIOS + TYPE + '/' + PEAJE + "?start_date=" + hoy + "T00%3A00";
  return url;
}

void initDate(){
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));// Establece la fecha y hora 
}
void printDate(DateTime date)
{
   Serial.print(date.day(), DEC);  
   Serial.print('/');
   Serial.print(date.month(), DEC);
   Serial.print('/');
   Serial.print(date.year(), DEC);   
   Serial.print("-----");
   Serial.print(date.hour(), DEC);
   Serial.print(':');
   Serial.print(date.minute(), DEC);
   Serial.print(':');
   Serial.print(date.second(), DEC);
   Serial.println();
}
  
