
/* 
 * HSM - Heating System Monitoring
 * -------------------------------
 *
 * for Arduino Uno with DS18B20 temperature sensors
 *
 * Version 1.6
 * Author: Rainer Faller (www.rainer-faller.de)
 * Created: 17/MAY/2014
 * Last Update: 26/MAR/2026
 *
 * Supports Two 1-Wire Buses for temperature measurement
 * Supports water leak level alarm
 *
 * Time measurement is done by counting loops, which is not really accurate at all.
 *
 * Update V 1.4: Includes a water leak detection
 * Update V 1.5: Includes a second water leak detection 
 * Update V 1.6: Use local PI, single transmits instead of bulk.
 */

// Includes
#include <OneWire.h>
#include <DallasTemperature.h>
#include <stdlib.h>
#include <SPI.h>
#include <Ethernet.h>

// MetaData
const char VERSION[5] = "1.6";
const char CREATED[12] = "26/MAR/2026";

// ************************
// Individual Parameters
// ************************
      byte      MAC_OF_ARDUINO_BOARD[] = { 0x90, 0xA2, 0xDA, 0x0E, 0xC8, 0x42 }; 
const IPAddress ARDUINO_CLIENT_IP (192,168,0,177);

const char      TARGET_SERVER[] = "192.168.0.46";
const int       TARGET_PORT = 8001;
const char      TARGET_ENDPOINT[] = "/api/measurements/batch";

// TODO add base64 encoded string from Keepass "HZS php (called by Arduino)"
// generate with: bash: echo -n "$username:$password" | base64
const char      BASE64_AUTH_STRING[] = "<HERE>";

// ************************
// Board Setup (Pin#)
// ************************
   
#define ONE_WIRE_BUS_1   2
#define ONE_WIRE_BUS_2   3
#define PIN_LED_LOOP_TOGGLE             5

// sum of both 1-WireBuses
#define MAX_SENSORS      12 

typedef struct 
{
  byte address[8];
  int bus_no;
} SENSOR_ADDRESS;



// ************************
// Generic Parameters
// ************************
const int       ONE_SECOND = 1000;
const byte      NO_SENSOR_CONNECTED[8] = {0,0,0,0,0,0,0,0};
// how often to measure and send the temperature data  // recommend: 120
const int       NUMBER_OF_LOOPS_UNTIL_NEXT_MEASUREMENT = 120; 


// ************************
// Singletons
// ************************
OneWire oneWireBus1( ONE_WIRE_BUS_1 ); 
OneWire oneWireBus2( ONE_WIRE_BUS_2 ); 
DallasTemperature temperatureSensors1( &oneWireBus1 );
DallasTemperature temperatureSensors2( &oneWireBus2 );

// ************************
// Global variables
// ************************
SENSOR_ADDRESS connected_sensor_ids[MAX_SENSORS];
// stores temperature value * 100
int            measurement_values[MAX_SENSORS]; 
int            number_of_connected_sensors;
int            loop_counter_until_next_measurement;
char           tempAddr[40];
EthernetClient client;
// state of the connection last time through the main loop
boolean        lastConnected = false;


void setup() 
{ 
  // wait a second for arduino to startup
  delay(ONE_SECOND);
  
  Serial.begin(9600);
  printInfoHeader();
  
  pinMode(PIN_LED_LOOP_TOGGLE, OUTPUT);
  
  delay(2*ONE_SECOND);
  
  // Initialize Network Connection
  initializeNetworkConnection();

  // Initialize dallas temperature library
  temperatureSensors1.begin();
  temperatureSensors2.begin();

  searchConnectedSensors();
  delay(2*ONE_SECOND);
  printConnectedSensors();
  
  loop_counter_until_next_measurement = NUMBER_OF_LOOPS_UNTIL_NEXT_MEASUREMENT;
  
  Serial.println();
  Serial.println("Setup done, starting temperature measurement and transmitting");
}

void loop() 
{
  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only: 
  while (client.available()) 
  {
    char c = client.read();
    Serial.print(c);
  }
  
  // if there's no net connection, but there was one last time
  // through the loop, then stop the client:
  if (!client.connected() && lastConnected) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
  }
    
  // measure temperature  
  if ( loop_counter_until_next_measurement >= NUMBER_OF_LOOPS_UNTIL_NEXT_MEASUREMENT )
  {
     measureTemperature();
     sendToServer();
     loop_counter_until_next_measurement = 0;
  }  

  // store the state of the connection for next time through
  // the loop:
  lastConnected = client.connected();

 // Serial.println("Blink");
  if ( digitalRead(PIN_LED_LOOP_TOGGLE) == HIGH ) 
  {
    digitalWrite(PIN_LED_LOOP_TOGGLE, LOW);  
  }
  else
  {
    digitalWrite(PIN_LED_LOOP_TOGGLE, HIGH);
  }
  delay(ONE_SECOND);
  
  loop_counter_until_next_measurement++;
}

void initializeNetworkConnection()
{
  // start the Ethernet connection:
  if (Ethernet.begin(MAC_OF_ARDUINO_BOARD) == 0) 
  {
    Serial.println("Failed to configure Ethernet using DHCP. Trying static IP");
    // no point in carrying on, so do nothing forevermore:
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(MAC_OF_ARDUINO_BOARD, ARDUINO_CLIENT_IP);
  }
   // give the Ethernet shield a second to initialize:
  delay(ONE_SECOND); 
  
  Serial.print("Connected with client IP address: ");
  Serial.println(Ethernet.localIP());
  Serial.println();
}

/**
 * adds all addresses of the connected sensors to the global list 'connected_sensor_ids'. 
 * The number of sensors connected is stored in the global variable 'number_of_connected_sensors'
 */
void searchConnectedSensors()
{
  number_of_connected_sensors=0;
  searchConnectedSensorsOnBus(oneWireBus1, 1);
  searchConnectedSensorsOnBus(oneWireBus2, 2);
}

void searchConnectedSensorsOnBus(OneWire oneWireBus, int busNo)
{
  byte current_addr[8];
  while(oneWireBus.search(current_addr)) 
  {
    if ( OneWire::crc8( current_addr, 7) != current_addr[7]) 
    {
      Serial.print("CRC of sensor with id "); 
      Serial.print(sensorAddressToString(tempAddr, current_addr));
      Serial.println(" is not valid "); 
    }
    else
    {
      // add this device to list
      connected_sensor_ids[number_of_connected_sensors].bus_no = busNo;
      for ( int k=0; k<8; k++)
      {
        connected_sensor_ids[number_of_connected_sensors].address[k] = current_addr[k];
      }
      number_of_connected_sensors++;
    }
  }
  oneWireBus.reset_search(); 
}

/**
 * prints the global list of 'connected_sensor_ids'
 */
void printConnectedSensors()
{
  Serial.print(number_of_connected_sensors);
  Serial.println(" connected sensor(s) are:");
  for( byte s = 0; s < number_of_connected_sensors; s++) 
  {
    Serial.print("S#");
    Serial.print(s+1);
    Serial.print(": "); 
    Serial.print( sensorAddressToString(tempAddr, connected_sensor_ids[s].address));
    Serial.print(" on Bus#");
    Serial.print(connected_sensor_ids[s].bus_no);
    Serial.println();
  }
}

/**
 * Formats a sensors address in a human readable format
 */
char* sensorAddressToString(char* outstr, byte* address)
{
  //String asString="";
  outstr[0]='\0';
  for( byte i = 0; i < 8; i++) 
  {
    strcat(outstr, "0x\0");
    if (address[i] < 16) 
    {
        strcat(outstr, "0\0");
    }
    String tmp = String(address[i], HEX);
    tmp.toUpperCase();
    char charBuf[4];
    tmp.toCharArray(charBuf, 4);
    
    strcat(outstr, charBuf );
    strcat(outstr, "\0");
     
    if (i < 7) 
    {
      strcat(outstr, ",\0");
    }
  }
  return outstr;
}


/*
 * Measures the temperature of each sensor and sends it to the server
 */
void measureTemperature()
{ 
  temperatureSensors1.requestTemperatures();  
  delay(ONE_SECOND);
  temperatureSensors2.requestTemperatures(); 

  for( byte s = 0; s < number_of_connected_sensors; s++) 
  {
    char * sensorAddr = sensorAddressToString(tempAddr, connected_sensor_ids[s].address);
    Serial.print("Measure. Sensor #");
    Serial.print(s+1);
    Serial.print(": ");
    Serial.print( sensorAddr);
    
    float temp_of_sensor = -100;
    if ( connected_sensor_ids[s].bus_no == 1 )
    {
      temp_of_sensor = temperatureSensors1.getTempC(connected_sensor_ids[s].address);
    }
    else
    {
      temp_of_sensor = temperatureSensors2.getTempC(connected_sensor_ids[s].address);
    }
    
    Serial.print(" -> ");
    Serial.print(temp_of_sensor);
    Serial.println(" Grad Celcius");
    
    measurement_values[s] = temp_of_sensor * 100;    
  } 
}

void sendToServer()
{
 if (client.connect(TARGET_SERVER, TARGET_PORT)) 
 {
  client.print("POST ");
  client.print(TARGET_ENDPOINT);
  client.print(" HTTP/1.1\r\n");
  client.print("Host: ");
  client.print(TARGET_SERVER);
  client.print("\r\n");
  client.print("Authorization: Basic ");
  client.print(BASE64_AUTH_STRING);
  client.print("\r\n");
  client.print("Content-Type: application/json\r\n");   
  client.print("Content-Length: ");
  if ( number_of_connected_sensors == 0 )
  {
    client.print(17+2);
  }
  else if ( number_of_connected_sensors == 1)
  {
    client.print(17+79+2);
  }
  else
  {
    client.print(17+79+ ((number_of_connected_sensors-1)*80) +2);
  }
  client.print("\r\n");
  
  client.print("\r\n");
  client.print("{\"measurements\":[");
  
  for ( int s = 0; s<number_of_connected_sensors; s++)
  {
    if ( s != 0 ) { client.print(","); }
    client.print("{\"sensorAddress\":\"");
    client.print(sensorAddressToString(tempAddr, connected_sensor_ids[s].address));
    client.print("\",\"temperature\":");
    client.print(measurement_values[s]);
    client.print("}");
  }
  client.println("]}");   
  Serial.println("sent done");
 }
}

void printInfoHeader()
{
  Serial.println();
  Serial.println("HSM - Heating System Monitoring");
  Serial.println("(for Arduino Uno with DS18B20 temperature sensors)");
  Serial.println();
  Serial.print("  Version: " );
  Serial.println(VERSION);
  Serial.print("  Created: " );
  Serial.println(CREATED);
  Serial.print("  Author: " );
  Serial.println("Rainer Faller (www.rainer-faller.de)");
  Serial.println();
  Serial.println("System restart done");
  Serial.println(); 
}
