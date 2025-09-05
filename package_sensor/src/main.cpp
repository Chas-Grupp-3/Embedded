#include <Arduino.h>
#include <DHTSensor.h>



void setup() 
{
  Serial.begin(9600);
  DHTSensor::initDHTSensor(5);
  delay(2000);
}

void loop() {
  DHTSensor::readDHTSensor();
  DHTSensor::printDHTSensor();
  delay(5000);
}

