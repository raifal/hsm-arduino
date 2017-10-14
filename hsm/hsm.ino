
/* 
 * HSM - Heating System Monitoring
 * -------------------------------
 *
 * for Arduino Uno with DS18B20 temperature sensors
 *
 * Version 1.5
 * Author: Rainer Faller (www.rainer-faller.de)
 * Created: 17/MAY/2014
 *
 * Supports Two 1-Wire Buses for temperature measurement
 * Supports water leak level alarm
 *
 * Time measurement is done by counting loops, which is not really accurate at all.
 *
 * Update V 1.4: Includes a water leak detection
 * Update V 1.5: Includes a second water leak detection 
 *
 */

// Includes
#include <OneWire.h>
#include <DallasTemperature.h>
#include <stdlib.h>
#include <SPI.h>
#include <Ethernet.h>

// MetaData
const char VERSION[5] = "1.5";
const char CREATED[12] = "17/MAY/2014";

// ************************
// Individual Parameters
// ************************
const char      CLIENT_CODE[] = "r$f%667"; 
      byte      MAC_OF_ARDUINO_BOARD[] = { 0x90, 0xA2, 0xDA, 0x0E, 0xC8, 0x42 }; 
const IPAddress ARDUINO_CLIENT_IP (192,168,0,177);

const char      TARGET_SERVER[] = "85.93.26.146";
const int       TARGET_PORT = 7071;
const char      TARGET_ENDPOINT[] = "/";

// TODO add base64 encoded string from Keepass "HZS php (called by Arduino)"
// generate with: bash: echo -n "$username:$password" | base64
const char      BASE64_AUTH_STRING[] = "aHNtOmFiJTV0MTNh";

// ************************
// Board Setup (Pin#)
// ************************
   
#define ONE_WIRE_BUS_1   2
#define ONE_WIRE_BUS_2   3
#define PIN_LOWER_WATER_LEAK   A0
#define PIN_UPPER_WATER_LEAK   A1
#define PIN_LED_LOOP_TOGGLE             5
#define PIN_LED_SWITCH_SEND_IMMEDIATLY  6
#define PIN_SWITCH_SEND_IMMEDIATLY      7
#define PIN_LED_COLLECTION_BLOCK_TOGGLE 8

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
// one loop is approx. 1 second  // recommend: 120
const int       NUMBER_OF_LOOPS_UNTIL_NEXT_MEASUREMENT = 120; 
// how many measurements are stored. recommended: 5 (seems to be max)
const int       SENDING_BLOCK_SIZE = 5; 
const int       CRITICAL_WATER_LEAK_VALUE = 800;


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
int            measurement_values[SENDING_BLOCK_SIZE][MAX_SENSORS]; 
int            number_of_connected_sensors;
int            loop_counter_until_next_measurement;
int            current_measurement_index;
char           tempAddr[40];
boolean        sendNow;
boolean        loopToggle;
boolean        sendingToggle;
EthernetClient client;
// state of the connection last time through the main loop
boolean        lastConnected = false;
int            lower_water_leak_voltage = 1023; // MAX Voltage
int            upper_water_leak_voltage = 1023; // MAX Voltage
boolean        exceptional_lower_water_leak_sent = false;
boolean        exceptional_upper_water_leak_sent = false;


void setup() 
{ 
  // wait a second for arduino to startup
  delay(ONE_SECOND);
  
  Serial.begin(9600);
  printInfoHeader();
  
  pinMode(PIN_SWITCH_SEND_IMMEDIATLY,INPUT);
  pinMode(PIN_LED_COLLECTION_BLOCK_TOGGLE,OUTPUT);
  pinMode(PIN_LED_SWITCH_SEND_IMMEDIATLY, OUTPUT);
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
  current_measurement_index = 0;
  sendNow = false;
  loopToggle = true;
  sendingToggle = true;
  
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
     current_measurement_index++;
     loop_counter_until_next_measurement = 0;
  }
  
  if ( digitalRead(PIN_SWITCH_SEND_IMMEDIATLY) == HIGH)
  {
    digitalWrite(PIN_LED_SWITCH_SEND_IMMEDIATLY,HIGH);
    sendNow = true;
    
  }
  else
  {
    digitalWrite(PIN_LED_SWITCH_SEND_IMMEDIATLY,LOW);
  } 
  
  lower_water_leak_voltage = analogRead( PIN_LOWER_WATER_LEAK );
  upper_water_leak_voltage = analogRead( PIN_UPPER_WATER_LEAK );
  //Serial.println(water_leak_voltage);
  if ( lower_water_leak_voltage < CRITICAL_WATER_LEAK_VALUE )
  {
    if ( exceptional_lower_water_leak_sent == false )
    {
       sendNow = true;
       Serial.println("Lower water leak level critical, sending exceptional message");
       exceptional_lower_water_leak_sent = true; // only sent once 
    }
  }
  else
  {
    exceptional_lower_water_leak_sent = false; // water leak back to normal
  }
  
  if ( upper_water_leak_voltage < CRITICAL_WATER_LEAK_VALUE )
  {
    if ( exceptional_upper_water_leak_sent == false )
    {
       sendNow = true;
       Serial.println("Upper water leak level critical, sending exceptional message");
       exceptional_upper_water_leak_sent = true; // only sent once 
    }
  }
  else
  {
    exceptional_upper_water_leak_sent = false; // water leak back to normal
  }  
  
  // send to internet server
  if ( current_measurement_index >= SENDING_BLOCK_SIZE || sendNow == true )
  {
    sendToServer();
    current_measurement_index = 0;
    sendNow = false;
    
      // sending indicator
    if ( sendingToggle == true )
    {
      digitalWrite(PIN_LED_COLLECTION_BLOCK_TOGGLE,HIGH);
      sendingToggle = false;
    }
    else
    {
      digitalWrite(PIN_LED_COLLECTION_BLOCK_TOGGLE,LOW);
      sendingToggle = true;
    } 
  }


  // store the state of the connection for next time through
  // the loop:
  lastConnected = client.connected();
  
  // active loop indicator
  if ( loopToggle == true )
  {
    digitalWrite(PIN_LED_LOOP_TOGGLE,HIGH);
    loopToggle = false;
  }
  else
  {
    digitalWrite(PIN_LED_LOOP_TOGGLE,LOW);
    loopToggle = true;
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
    
    measurement_values[current_measurement_index][s] = temp_of_sensor * 100;    
  } 
}

void sendToServer()
{
  /*
  Serial.println();
  Serial.println("Debug is on, not sending to server");
  for ( int i=0; i<current_measurement_index; i++ )
  {
    int ts = -( current_measurement_index - i -1 ) * NUMBER_OF_LOOPS_UNTIL_NEXT_MEASUREMENT;
    for ( int s = 0; s<number_of_connected_sensors; s++)
    {
      Serial.print("op=m:");
      Serial.print("ts=");
      Serial.print( ts );
      Serial.print(":addr=");
      Serial.print(sensorAddressToString(tempAddr, connected_sensor_ids[s].address));
      Serial.print(":value=");
      Serial.print(measurement_values[i][s]);
      Serial.println();
    }
  }
  Serial.println();
  */
  
  Serial.println("Connecting to server...");
  
  delay(2 * ONE_SECOND);
  
  if (client.connect(TARGET_SERVER, TARGET_PORT)) 
  {

    Serial.print("Sending data to server ");
    Serial.print(TARGET_SERVER);
    Serial.print(":");
    Serial.print(TARGET_PORT);
    Serial.println("... ");
    
    // send the HTTP GET request:
    client.print("GET ");
    client.print(TARGET_ENDPOINT);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(TARGET_SERVER);
    client.print("Authorization: Basic ");
    client.println(BASE64_AUTH_STRING);   
    client.println("User-Agent: arduino-ethernet");
      
    // send client code first
    client.print("HSM-C:");
    client.print("client=");
    client.print(CLIENT_CODE);
    client.println();
    
    // send water leaking voltage value (5V=1023)
    client.print("HSM-W:");
    client.print("lower_voltage=");
    client.print(lower_water_leak_voltage);
    client.print(":");
    client.print("upper_voltage=");
    client.print(upper_water_leak_voltage);    
    client.println();    
    
    for ( int i=0; i < current_measurement_index; i++)
    {     
      int ts = - ( current_measurement_index - i -1 ) * NUMBER_OF_LOOPS_UNTIL_NEXT_MEASUREMENT;
      for ( int s = 0; s < number_of_connected_sensors; s++)
      {
        client.print("HSM-M");
        client.print(i);
        client.print("x");
        client.print(s);
        client.print(":");
        client.print("ts=");
        client.print( ts );
        client.print(":addr=");
        client.print(sensorAddressToString(tempAddr, connected_sensor_ids[s].address));
        client.print(":value=");
        client.print(measurement_values[i][s]);
        client.println();
      }
    }
      
    client.println("Connection: close");
    client.println();
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
