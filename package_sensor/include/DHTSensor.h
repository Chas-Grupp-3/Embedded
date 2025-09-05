#ifndef DHTSENSOR_H
#define DHTSENSOR_H

#include <Arduino.h>
#include <DHT.h>

namespace DHTSensor
{

    uint8_t DHT_PIN = 5; //Change for the actual pin used 
    const uint8_t DHT_TYPE = DHT11;
    DHT dht(DHT_PIN, DHT_TYPE);
    float temperature;
    float humidity;

    void initDHTSensor(uint8_t pin);
    void initDHTSensor();
    void readDHTSensor();
    void printDHTSensor();
    
    void initDHTSensor(uint8_t pin)
    {
        DHTSensor::DHT_PIN = pin;
        dht.begin();
    }

    void initDHTSensor()
    {
        dht.begin();
    }

    void readDHTSensor()
    {
        temperature = dht.readTemperature();
        humidity = dht.readHumidity();
    }

    void printDHTSensor()
    {
        Serial.print("Temperature: ");
        Serial.print(temperature, 1);
        Serial.println("°C");
        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.println("%");
    }

}

#endif