#ifndef DHTSENSOR_H
#define DHTSENSOR_H

#include <Arduino.h>
#include <DHT.h>

struct threshold //Dessa ska vi få från appen sen
    {
        float minTemp = 20.0;
        float maxTemp = 28.0;   
    };

    enum class Status //Status för temperature
    {
        NORMAL,
        HIGH_TEMP,
        LOW_TEMP,
        ERROR
    };


Status checkStatus(float temperature,const threshold& limits)
    {
        if (isnan(temperature)) // Kontrollera om avläsningen lyckades
        {
            return Status::ERROR;
        }
        else if (temperature < limits.minTemp) // Kontrollera om under minTemp
        {
            return Status::LOW_TEMP;
        }
        else if (temperature > limits.maxTemp) // Kontrollera om över maxTemp
        {
            return Status::HIGH_TEMP;
        }
        else
        {
            return Status::NORMAL; // Kontrollera om vi är mellan min - max
        
        }
    }

namespace DHTSensor
{

    uint8_t DHT_PIN = 5; //Ändra till rätt pin
    const uint8_t DHT_TYPE = DHT11; //Ändra vid byte om sensor
    DHT dht(DHT_PIN, DHT_TYPE);
    float temperature;
    float humidity;

    void initDHTSensor(uint8_t pin);
    void initDHTSensor();
    void readDHTSensor();
    void printDHTSensor();
    
    

    void initDHTSensor() //Starta sensorn
    {
        dht.begin();
    }

    void readDHTSensor() //Spara värdena i variabler
    {
        humidity = dht.readHumidity();
        temperature = dht.readTemperature();
    }

    void printDHTSensor() //Test för att se värdena
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