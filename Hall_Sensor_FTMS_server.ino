/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <math.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define FTMS_UUID "00001826-0000-1000-8000-00805f9b34fb"
#define INDOOR_BIKE_DATA_CHARACTERISTIC_UUID "00002ad2-0000-1000-8000-00805f9b34fb"
#define LED_BUILTIN 2

bool magStateOld;
int analogPin = 18;
int digitalPin = 19;
void setup()
{
    pinMode(LED_BUILTIN, OUTPUT); // initialize digital pin LED_BUILTIN as an output.
    setupBluetoothServer();
    setupHalSensor();
    magStateOld = digitalRead(digitalPin);
}

BLECharacteristic *pCharacteristic;
void setupBluetoothServer()
{
    Serial.begin(115200);
    Serial.println("Starting BLE work!");
    BLEDevice::init("IC Bike");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(FTMS_UUID);
    pCharacteristic = pService->createCharacteristic(
        INDOOR_BIKE_DATA_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE);

    pCharacteristic->setValue("Characteristic configured"); // Used for demonstration purposes.
    pService->start();
    // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // add this for backwards compatibility
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(FTMS_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void setupHalSensor()
{
    //    attachInterrupt(digitalPin, ISR, RISING); //attaching the interrupt
    pinMode(analogPin, INPUT);
    pinMode(digitalPin, INPUT);
    Serial.begin(9600);
}

//incrementRevolutions() used to synchronously update rev rather than using an ISR.
inline bool positiveEdge(bool state, bool& oldState)
{
    bool result = (state && !oldState);
    oldState = state;
    return result;
}  

double calculateRpmFromRevolutions(int revolutions, unsigned long revolutionsTime)
{
    double ROAD_WHEEL_TO_TACH_WHEEL_RATIO = 6.8;
    double instantaneousRpm = revolutions * 60000 / revolutionsTime / ROAD_WHEEL_TO_TACH_WHEEL_RATIO;
    Serial.printf("revolutionsTime: %d, rev: %d , instantaneousRpm: %2.9f \n",
                    revolutionsTime, revolutions, instantaneousRpm);
    return instantaneousRpm;
}

double calculateMphFromRpm(double rpm)
{
    double WHEEL_RADIUS = 0.00034; // in km
    double KM_TO_MI = 0.621371;

    double circumfrence = 2 * PI * WHEEL_RADIUS;
    double metricDistance = rpm * circumfrence;
    double imperialDistance = metricDistance * KM_TO_MI;
    double mph = imperialDistance * 60; // feet -> miles and minutes -> hours
//    Serial.printf("rpm: %2.2f, circumfrence: %2.2f, metricDistance %2.5f , imperialDistance: %2.5f, mph: %2.2f \n",
//                   rpm, circumfrence, metricDistance, imperialDistance, mph);
    return mph;
}

unsigned long distanceTime = 0;
double runningDistance = 0.0;
double calculateDistanceFromMph(unsigned long distanceTimeSpan, double mph)
{
    double incrementalDistance = distanceTimeSpan * mph / 60 / 60 / 1000;
//    Serial.printf("mph: %2.2f, distanceTimeSpan %d , incrementalDistance: %2.9f \n",
//                   mph, distanceTimeSpan, incrementalDistance);
    return incrementalDistance;
}

double tireValues[] = {0.005, 0.004, 0.012};                      //Clincher, Tubelar, MTB
double aeroValues[] = {0.388, 0.445, 0.420, 0.300, 0.233, 0.200}; //Hoods, Bartops, Barends, Drops, Aerobar
unsigned long caloriesTime = 0;
double runningCalories = 0.0;
double calculateCaloriesFromMph(unsigned long caloriesTimeSpan, double mph)
{
    double velocity = mph * 0.44704; // translates to meters/second
    double riderWeight = 72.6;       //165 lbs
    double bikeWeight = 11.1;        //Cannondale road bike
    int theTire = 0;                 //Clinchers
    double rollingRes = tireValues[theTire];
    int theAero = 1; //Bartops
    double frontalArea = aeroValues[theAero];
    double grade = 0;
    double headwind = 0;        // converted to m/s
    double temperaturev = 15.6; // 60 degrees farenheit
    double elevation = 100;     // Meters
    double transv = 0.95;       // no one knows what this is, so why bother presenting a choice?

    /* Common calculations */
    double density = (1.293 - 0.00426 * temperaturev) * exp(-elevation / 7000.0);
    double twt = 9.8 * (riderWeight + bikeWeight); // total weight in newtons
    double A2 = 0.5 * frontalArea * density;       // full air resistance parameter
    double tres = twt * (grade + rollingRes);      // gravity and rolling resistance

    // we calculate power from velocity
    double tv = velocity + headwind;      //terminal velocity
    double A2Eff = (tv > 0.0) ? A2 : -A2; // wind in face so you must reverse effect
    double powerv = (velocity * tres + velocity * tv * tv * A2Eff) / transv;

    /* Common calculations */
    double incrementalCalories = caloriesTimeSpan * powerv * 0.24; // simplified
    double wl = incrementalCalories / 32318.0;                     // comes from 1 lb = 3500 Calories
    return incrementalCalories;
}

void indicateRpmWithLight(int rpm)
{
    if (rpm > 1)
    {
        digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
    }
    else
    {
        digitalWrite(LED_BUILTIN, LOW); // turn the LED off by making the voltage LOW
    }
}

/* 
Fitness Machine Features Characteristic (Mandatory for FTMS)
Bit Number Definition 0=false, 1=true
0 - Average Speed Supported
1 - Cadence Supported
2 - Total Distance Supported
3 - Inclination Supported
4 - Elevation Gain Supported
5 - Pace Supported
6 - Step Count Supported
7 - Resistance Level Supported
8 - Stride Count Supported
9 - Expended Energy Supported
10 - Heart Rate Measurement Supported
11 - Metabolic Equivalent Supported
12 - Elapsed Time Supported
13 - Remaining Time Supported
14 - Power Measurement Supported
15 - Force on Belt and Power Output Supported
16 - User Data Retention Supported
17-31 - Reserved for Future Use

Indoor Bike Data characteristic
First bit refers to Characteristics bit, parentheses bit refers to corresponding Features Characteristic
More Data present (bit 0), see Sections 4.9.1.2 and 4.19, otherise Instantaneous Speed field present
Average Speed present (bit 1), see Section 4.9.1.3. - Average Speed Supported (bit 0)
Instantaneous Cadence present (bit 2), see Section 4.9.1.4. - Cadence Supported (bit 1)
Average Cadence present (bit 3), see Section 4.9.1.5. - Cadence Supported (bit 1)
Total Distance Present (bit 4), see Section 4.9.1.6. - Total Distance Supported (bit 2)
Resistance Level Present (bit 5), see Section 4.9.1.7. - Resistance Level Supported (bit 7)
Instantaneous Power Present (bit 6), see Section 4.9.1.8. - Power Measurement Supported (bit 14)
Average Power Present (bit 7), see Section 4.9.1.9. - Power Measurement Supported (bit 14)
Expended Energy Present (bit 8), see Sections 4.9.1.10, 4.9.1.11 and 4.9.1.12.- Expended Energy Supported (bit 9)
Heart Rate Present (bit 9, see Section 4.9.1.13. - Heart Rate Measurement Supported (bit 10)
Metabolic Equivalent Present (bit 10), see Section 4.9.1.14. - Metabolic Equivalent Supported (bit 11)
Elapsed Time Present (bit 11), see Section 4.9.1.15. - Elapsed Time Supported (bit 12)
Remaining Time Present (bit 12), see Section 4.9.1.16. - Remaining Time Supported (bit 13)
 */

void transmitFTMS(int rpm)
{
    pCharacteristic->setValue(rpm);
}

unsigned long elapsedTime = 0;
unsigned long intervalTime = 0;
int rev = 0;
void loop()
{
    intervalTime = millis() - elapsedTime;
    rev += (int)positiveEdge(digitalRead(digitalPin), magStateOld);

//    Serial.printf("outside rev: %d \n", rev);
    if (intervalTime > 1000)
    {
        Serial.printf("rev: %d \n", rev);
        double rpm = calculateRpmFromRevolutions(rev, intervalTime);
        Serial.printf("rpm: %2.2f \n", rpm);
        double mph = calculateMphFromRpm(rpm);
        Serial.printf("mph: %2.2f \n", mph);
        runningDistance += calculateDistanceFromMph(intervalTime, mph);
        runningCalories += calculateCaloriesFromMph(intervalTime, mph);

        Serial.printf("intervalTime: %d, elapsedTime: %d, RPM: %2.2, Revolutions %d , MPH: %2.2f, Distance: %2.2f, Calories: %2.2f \n", 
                      intervalTime, elapsedTime, rpm, rev, mph, runningDistance, runningCalories);
        indicateRpmWithLight(rpm);
        transmitFTMS(rpm);
        
        rev = 0;
        elapsedTime = millis();
    }
}
