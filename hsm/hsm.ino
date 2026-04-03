
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
#include <Ethernet.h>
#include <avr/wdt.h>

// ************************
// Individual Parameters
// ************************
      byte      MAC_OF_ARDUINO_BOARD[] = { 0x90, 0xA2, 0xDA, 0x0E, 0xC8, 0x42 }; 
const IPAddress ARDUINO_CLIENT_IP (192,168,0,177);
const char      TARGET_SERVER[] = "192.168.0.46";

// ************************
// Board Setup (Pin#)
// ************************
   
#define ONE_WIRE_BUS_1      2
#define ONE_WIRE_BUS_2      3
#define PIN_LED_LOOP_TOGGLE 5

// sum of both 1-WireBuses
#define MAX_SENSORS         13

typedef struct 
{
  byte address[8];
  int bus_no;
} SENSOR_ADDRESS;


// ************************
// Generic Parameters
// ************************
const int       ONE_SECOND = 1000;
const int       SERVER_RESPONSE_TIMEOUT_MS = 5000;
const byte      NO_SENSOR_CONNECTED[8] = {0,0,0,0,0,0,0,0};
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
int            last_http_status = -1;
unsigned int   consecutive_send_failures = 0;
unsigned long  successful_send_count = 0;
unsigned long  failed_send_count = 0;
byte           last_reset_cause = 0;


char toHexChar(byte v)
{
  return (v < 10) ? ('0' + v) : ('A' + (v - 10));
}

int jsonPayloadLength()
{
  int len = 17 + 2; // {"measurements":[ + ]}

  for (int s = 0; s < number_of_connected_sensors; s++)
  {
    if (s != 0)
    {
      len += 1; // comma between measurement objects
    }

    char addrBuf[40];
    char tempBuf[12];
    sensorAddressToString(addrBuf, connected_sensor_ids[s].address);
    itoa(measurement_values[s], tempBuf, 10);
    len += 35; // {"sensorAddress":" + ","temperature": + }
    len += strlen(addrBuf);
    len += strlen(tempBuf);
  }
  return len;
}



extern unsigned int __heap_start;
extern void *__brkval;
int freeMemory() {
  int free_memory;
  if ((int)__brkval == 0) {
    free_memory = ((int)&free_memory) - ((int)&__heap_start);
  } else {
    free_memory = ((int)&free_memory) - ((int)__brkval);
  }
  return free_memory;
}

void setup() 
{ 
  last_reset_cause = MCUSR;
  MCUSR = 0;

  // wait a second for arduino to startup
  delay(ONE_SECOND);
  
  Serial.begin(9600);
  Serial.println("HSM - System restart done");
  
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

  // Restart the MCU automatically if main loop gets stuck.
  wdt_enable(WDTO_8S);
  
  Serial.println();
  Serial.println("Setup done, starting temperature measurement and transmitting");
}

void loop() 
{
  wdt_reset();

  int dhcpStatus = Ethernet.maintain();
  if (dhcpStatus == 1 || dhcpStatus == 3)
  {
    Serial.println("DHCP lease refresh failed, reinitializing network...");
    initializeNetworkConnection();
  }

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
     Serial.print("Freier RAM: ");
     Serial.println(freeMemory());
     measureTemperature();
     sendToServer();
     loop_counter_until_next_measurement = 0;
  }  

  // store the state of the connection for next time through the loop:
  lastConnected = client.connected();

  if ( digitalRead(PIN_LED_LOOP_TOGGLE) == HIGH ) 
  {
    digitalWrite(PIN_LED_LOOP_TOGGLE, LOW);  
  }
  else
  {
    digitalWrite(PIN_LED_LOOP_TOGGLE, HIGH);
  }
  delay(ONE_SECOND);
  wdt_reset();
  
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

void searchConnectedSensorsOnBus(OneWire &oneWireBus, int busNo)
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
      if (number_of_connected_sensors >= MAX_SENSORS)
      {
        Serial.println("Too many sensors found. Ignoring additional devices.");
        break;
      }

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
  byte pos = 0;
  for( byte i = 0; i < 8; i++) 
  {
    outstr[pos++] = '0';
    outstr[pos++] = 'x';
    outstr[pos++] = toHexChar((address[i] >> 4) & 0x0F);
    outstr[pos++] = toHexChar(address[i] & 0x0F);

    if (i < 7)
    {
      outstr[pos++] = ',';
    }
  }
  outstr[pos] = '\0';
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
    wdt_reset();

    char * sensorAddr = sensorAddressToString(tempAddr, connected_sensor_ids[s].address);
    Serial.print("Sensor #");
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
    Serial.println(temp_of_sensor);
    
    measurement_values[s] = temp_of_sensor * 100;    
  } 
}

void sendToServer()
{
 if (client.connected())
 {
  client.stop();
 }

 if (client.connect(TARGET_SERVER, 8001)) 
 {
  int responseStatus = -2;
  bool statusLineDone = false;
  char statusLine[40];
  byte statusLineIdx = 0;

  client.print("POST ");
  client.print("/api/measurements/batch");
  client.print(" HTTP/1.1\r\n");
  client.print("Host: ");
  client.print(TARGET_SERVER);
  client.print("\r\n");
  client.print("Authorization: Basic aHNtOmd6aDdyJWRzdmplX2FwaQ==\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Connection: close\r\n");
  client.print("Content-Length: ");
  client.print(jsonPayloadLength());
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
  client.print("]}");
  Serial.println("done.");

  unsigned long waitStart = millis();
  while ((millis() - waitStart) < SERVER_RESPONSE_TIMEOUT_MS)
  {
    wdt_reset();

    while (client.available())
    {
      char c = client.read();
      Serial.print(c);

      if (!statusLineDone)
      {
        if (c == '\r')
        {
          // ignore
        }
        else if (c == '\n')
        {
          statusLine[statusLineIdx] = '\0';
          if (strncmp(statusLine, "HTTP/1.", 7) == 0)
          {
            responseStatus = atoi(statusLine + 9);
          }
          statusLineDone = true;
        }
        else if (statusLineIdx < sizeof(statusLine) - 1)
        {
          statusLine[statusLineIdx++] = c;
        }
      }

      waitStart = millis();
    }

    if (!client.connected())
    {
      break;
    }

    delay(10);
  }

  client.stop();

  last_http_status = responseStatus;
  if (responseStatus >= 200 && responseStatus < 300)
  {
    successful_send_count++;
    consecutive_send_failures = 0;
  }
  else
  {
    failed_send_count++;
    consecutive_send_failures++;
  }
 }
 else
 {
  last_http_status = -3;
  failed_send_count++;
  consecutive_send_failures++;
  Serial.println("connection to server failed");
 }
}
