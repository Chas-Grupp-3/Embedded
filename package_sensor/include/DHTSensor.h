#ifndef DHTSENSOR_H
#define DHTSENSOR_H

#include <Arduino.h>
#include <DHT.h>

struct threshold //We will get these later from app
    {
        float minTemp = 20.0;
        float maxTemp = 28.0;   
    };

    enum class Status //Status for temperature
    {
        NORMAL,
        HIGH_TEMP,
        LOW_TEMP,
        ERROR
    };


Status checkStatus(float temperature,const threshold& limits)
    {
        if (isnan(temperature)) // Check if reading was successful
        {
            return Status::ERROR;
        }
        else if (temperature < limits.minTemp) // Check if below minTemp
        {
            return Status::LOW_TEMP;
        }
        else if (temperature > limits.maxTemp) // Check if above maxTemp
        {
            return Status::HIGH_TEMP;
        }
        else
        {
            return Status::NORMAL; // Check if we are inbetween min - max
        
        }
    }

namespace DHTSensor
{

    uint8_t DHT_PIN = 5; //Change to appropriate pin
    const uint8_t DHT_TYPE = DHT11; //DHT11 sensor. change to DHT22 if needed
    DHT dht(DHT_PIN, DHT_TYPE);
    float temperature;
    float humidity;

    void initDHTSensor(uint8_t pin);
    void initDHTSensor();
    void readDHTSensor();
    void printDHTSensor();
    
    

    void initDHTSensor() //Start the sensor
    {
        dht.begin();
    }

    void readDHTSensor() //Save the readings to variables
    {
        humidity = dht.readHumidity();
        temperature = dht.readTemperature();
    }

    void printDHTSensor() //print if needed, we are checking temp in src
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