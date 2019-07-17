#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "board_def.h"

//#define SENDER

// OLED config
OLED_CLASS_OBJ display(OLED_ADDRESS, OLED_SDA, OLED_SCL);

// Wifi config
#define WIFI_SSID "Aquaris U Plus - Marcel"
#define WIFI_PASSWORD "c1044697adb2"

// MQTT server
//const char *mqtt_server = "iot.eclipse.org";
//const char *mqtt_server = "test.mosquitto.org";
//const char *mqtt_server = "broker.mqtt-dashboard.com";
const char *mqtt_server = "desplega.com";
//const char *mqtt_server = "mqtt.desplega.com";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

#define MSG_LENGTH 16
void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    char str[MSG_LENGTH]; // Limit incoming message to 10 characters
    int i = 0;
    while (i < length && i < (MSG_LENGTH - 1))
    {
        str[i] = (char)payload[i];
        i++;
    }
    str[i] = '\0';

    Serial.print(str);
    Serial.println();

    // Show message in OLED
    display.clear();
    display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 24, "Msg[" + String(topic) + "]");
    display.drawString(display.getWidth() / 2, display.getHeight() / 2 + 8, String(str));
    display.display();
}

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (client.connect(clientId.c_str()))
        {
            Serial.println("connected");
            // Once connected, publish an announcement...
#ifdef SENDER
            client.publish("test", "Hi!");
#else
            // ... and resubscribe
            client.subscribe("test");
#endif
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 23
#define TEMPERATURE_PRECISION 9 // Lower resolution

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

int numberOfDevices; // Number of temperature devices found

DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (deviceAddress[i] < 16)
            Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
    }
}

void setup(void)
{
    pinMode(BUILTIN_LED, OUTPUT); // Initialize the BUILTIN_LED pin as an output

    // OLED config
    if (OLED_RST > 0)
    {
        pinMode(OLED_RST, OUTPUT);
        digitalWrite(OLED_RST, HIGH);
        delay(100);
        digitalWrite(OLED_RST, LOW);
        delay(100);
        digitalWrite(OLED_RST, HIGH);
    }

    Serial.println("VSC compilation");

    // Init OLED
    display.init();
    display.flipScreenVertically();
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    delay(2000);

    // Start serial port
    Serial.begin(115200);
    Serial.println("Dallas Temperature IC Control Library and WiFi Demo");

    // Init WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        display.clear();
        Serial.println("WiFi Connect Fail");
        display.drawString(display.getWidth() / 2, display.getHeight() / 2, "WiFi Connect Fail");
        display.display();
        delay(2000);
        esp_restart();
    }
    Serial.print("Connected: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP: ");
    Serial.println(WiFi.localIP().toString());
    display.clear();
    display.drawString(display.getWidth() / 2, display.getHeight() / 2, "IP:" + WiFi.localIP().toString());
    display.display();
    delay(2000);

    // Init MQTT
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    // Start up the Dallas Temperature library
    sensors.begin();

    // Grab a count of devices on the wire
    numberOfDevices = sensors.getDeviceCount(); // This function doesn't work :(
    //numberOfDevices = 2;

    // Display in OLED
    if (numberOfDevices > 0)
    {
        display.clear();
        display.drawString(display.getWidth() / 2 - 22, display.getHeight() / 2 - 32, "Devices: " + String(numberOfDevices));
        display.display();
    }

    // locate devices on the bus
    Serial.print("Locating devices...");

    Serial.print("Found ");
    Serial.print(numberOfDevices, DEC);
    Serial.println(" devices.");

    // report parasite power requirements
    Serial.print("Parasite power is: ");
    if (sensors.isParasitePowerMode())
        Serial.println("ON");
    else
        Serial.println("OFF");

    // Loop through each device, print out address
    for (int i = 0; i < numberOfDevices; i++)
    {
        // Search the wire for address
        if (sensors.getAddress(tempDeviceAddress, i))
        {
            Serial.print("Found device ");
            Serial.print(i, DEC);
            Serial.print(" with address: ");
            printAddress(tempDeviceAddress);
            Serial.println();

            Serial.print("Setting resolution to ");
            Serial.println(TEMPERATURE_PRECISION, DEC);

            // set the resolution to TEMPERATURE_PRECISION bit (Each Dallas/Maxim device is capable of several different resolutions)
            sensors.setResolution(tempDeviceAddress, TEMPERATURE_PRECISION);

            Serial.print("Resolution actually set to: ");
            Serial.print(sensors.getResolution(tempDeviceAddress), DEC);
            Serial.println();
        }
        else
        {
            Serial.print("Found ghost device at ");
            Serial.print(i, DEC);
            Serial.println(" but could not detect address. Check power and cabling");
        }
    }
}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress, int tIndex)
{
    // Get temperature
    float tempC = sensors.getTempC(deviceAddress);
    Serial.print("Temp C: ");
    Serial.print(tempC);

    // Print data to OLED
    if (tIndex == 0)
    {
        display.clear();
        display.drawString(display.getWidth() / 2 - 22, display.getHeight() / 2 - 32, "Devices: " + String(numberOfDevices));
        display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 4, "Temp 1: " + String(tempC) + " C");
    }
    else
    {
        display.drawString(display.getWidth() / 2, display.getHeight() / 2 + 16, "Temp 2: " + String(tempC) + " C");
        display.display();
    }
}

int uniqueLoop = 1;
void loop(void)
{
    // MQTT loop
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();

#ifdef SENDER
    long now = millis();
    if (now - lastMsg > 2000)
    {
        lastMsg = now;
        ++value;
        snprintf(msg, 50, "Hi! #%ld", (long int)value);
        Serial.print("Publish message: ");
        Serial.println(msg);
        client.publish("test", msg);

        // Toggle BUILTIN led
        if (value % 2 == 0)
        {
            digitalWrite(BUILTIN_LED, HIGH); // Turn the LED on
        }
        else
        {
            digitalWrite(BUILTIN_LED, LOW); // Turn the LED off
        }
    }
#endif

    // Get temperature and display it to OLED
    if (uniqueLoop)
    {
        // call sensors.requestTemperatures() to issue a global temperature
        // request to all devices on the bus
        Serial.print("Requesting temperatures...");
        sensors.requestTemperatures(); // Send the command to get temperatures
        Serial.println("DONE");

        // Loop through each device, print out temperature data
        for (int i = 0; i < numberOfDevices; i++)
        {
            // Search the wire for address
            if (sensors.getAddress(tempDeviceAddress, i))
            {
                // Output the device ID
                Serial.print("Temperature for device: ");
                Serial.println(i, DEC);

                // It responds almost immediately. Let's print out the data
                printTemperature(tempDeviceAddress, i); // Use a simple function to print out the data
            }
            //else ghost device! Check your power requirements and cabling
        }
        uniqueLoop = 0;
    }
}
