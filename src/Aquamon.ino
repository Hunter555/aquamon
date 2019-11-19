// This #include statement was automatically added by the Particle IDE.
#include <OneWire.h>


// This #include statement was automatically added by the Particle IDE.
#include <MQTT.h>
#include "DS18.h"

char server[] = "10.0.0.5";
void callback(char* topic, byte* payload, unsigned int length) {
}

MQTT client(server, 1883, callback);       // Initialize the MQTT client.


DS18 sensor(A1);
unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 1L * 60L * 1000L; // Post data every 1 minute.
#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
#define TEMP_RETRY_LIMIT 10
#define SERIESRESISTOR 560
#define WATERSENSORPIN A0
#define WATERRESISTIDEAL 250
int waterBuffer[10], temp, retryCount;
unsigned long int avgValue;
float waterLevel;
unsigned long lastSync = millis();

void setup() {

    Particle.publish("Starting up");
    Serial.begin(9600);
    pinMode(A0, INPUT);
    pinMode(A1, INPUT);
    Time.setFormat(TIME_FORMAT_ISO8601_FULL);
    Time.zone(10);
    
    Mesh.subscribe("current-power", updatePowerHandler);
}

void loop() {
    // If MQTT client is not connected then reconnect.
    if (!client.isConnected())
    {
      reconnect();
    } else {
        client.loop();  // Call the loop continuously to establish connection to the server.
    }
    if (millis() - lastSync > ONE_DAY_MILLIS) {
        // Request time synchronization from the Particle Device Cloud
        Particle.syncTime();
        lastSync = millis();
    }
    
    if (millis() - lastConnectionTime > postingInterval && Time.isValid())
    {
        mqttpublish();
    }
}

void reconnect() {
    
     Serial.println("Attempting MQTT connection");
     
     char clientID[8] = "aquamon";
     clientID[8] = '\0';
    
     if (client.connect(clientID))  {
         Particle.publish("Conn:"+ String(server) + " cl: " + String(clientID));
     } else
     {
         Particle.publish("Failed to connect, Trying to reconnect in 5 seconds");
         delay(5000);
     } 
}

void updatePowerHandler(const char *event, const char *data)
{
      Particle.publish("Recieved power update", data);
      //Serial.printlnf("event=%s data=%s", event, data ? data : "NULL");
    if (client.isConnected())
    {
        String powerTopic = String("home/powermon/status");
        //String payload = String::format("{ \"power\": %s }", data);
        Particle.publish("Publishing power data", data);
        client.publish(powerTopic,data);
    }
    else
    {
        Particle.publish("MQTT client not connected");
    }
}

void mqttpublish() {
    Particle.publish("Reading sensor values");
    String topic = String("home/aquamon/status");
    String data;
    retryCount = 0;
    while (!sensor.read() || retryCount < TEMP_RETRY_LIMIT)
    {
        // Once all sensors have been read you'll get searchDone() == true
        // Next time read() is called the first sensor is read again
        if (sensor.searchDone()) {
          Serial.println("No more addresses.");
        }
        delay(50);
        retryCount++;
    }
    int waterLevel = getWaterLevelReading();
    data = String("{\"time\": \"" + Time.format(Time.now(), TIME_FORMAT_DEFAULT) + "\"," +
                        "\"temperature\":" + String::format("%.1f,", sensor.celsius()) +
                        "\"water_level\":" + String(waterLevel) +
                        "}");
    Particle.publish("Sending payload", data);
    client.publish(topic,data);
    //int lightLevel = analogRead(LIGHTPIN); // Read from light sensor.
    lastConnectionTime = millis();
}

int getWaterLevelReading() {
    for (int i = 0; i < 10; i++){
        waterBuffer[i] = analogRead(WATERSENSORPIN);
        delay(10);
    }
    for(int i=0; i<9; i++)        //sort the analog from small to large
    {
        for(int j=i+1; j<10; j++)
        {
          if(waterBuffer[i] > waterBuffer[j])
          {
            temp = waterBuffer[i];
            waterBuffer[i] = waterBuffer[j];
            waterBuffer[j] = temp;
          }
        }
    }
    avgValue=0;
    for (int i=2; i<8; i++) {                     //take the average value of 6 center sample
        avgValue += waterBuffer[i];
    }
    avgValue = avgValue / 6;
    Particle.publish("Water level raw avg", String(avgValue));
    waterLevel = (4095 / (float)avgValue) -1;
    waterLevel = SERIESRESISTOR * waterLevel;
    Particle.publish("Water level reading", String(waterLevel));
    return waterLevel / WATERRESISTIDEAL * 100;
}
