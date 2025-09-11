#include <Arduino.h>
#include <DHTSensor.h>

threshold limits;
Status currentStatus = Status::NORMAL;

//Readings timing, we later set these for longer durations
unsigned long lastReading = 0;
unsigned long normalInterval = 10000;
unsigned long errorInterval = 2000;
unsigned long currentInterval = normalInterval;


void setup() 
{
  Serial.begin(9600);
  DHTSensor::initDHTSensor();
  delay(2000);
}

void loop() {
  unsigned long now = millis(); // Get the current time
  
  if (now - lastReading >= currentInterval) // Time to take a new reading
  {
    lastReading = now;
   
    DHTSensor::readDHTSensor();
    float temperature = DHTSensor::temperature;
    currentStatus = checkStatus(temperature, limits);

  if (currentStatus == Status::LOW_TEMP) //Too low temperature
    {
      Serial.println("Temperature is too low!");
      Serial.print("Temperature: ");
      Serial.print(temperature);
       currentInterval= errorInterval;
    }
  
  else if (currentStatus == Status::HIGH_TEMP) //Too high temperature
  {
    Serial.println("Temperature is too high!");
    Serial.println("Temperature: ");
    Serial.print(temperature);
    currentInterval= errorInterval;
  }
  
  else if (currentStatus == Status::ERROR) //Error reading from sensor (NaN)
  {
    Serial.println("\nError reading from DHT sensor!");
     currentInterval= errorInterval;
  }
  
  else //Within limits, normal temperature
  {
    Serial.println("\nTemperature is normal."); 
    Serial.println("Temperature: ");
    Serial.print(temperature);
    currentInterval = normalInterval;
  }
  
  }

}

