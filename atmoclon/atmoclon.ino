/*
 Name:		atmoclon.ino
 Created:	3/14/2016 10:08:53 PM
 Author:	leonid.koftun@gmail.com
*/

#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define SEALEVELPRESSURE_HPA (1013.25)

const char* ssid = "********";
const char* password = "********";
const char* mqtt_server = "********";
const int BLUE_LED = 2;
const int RED_LED = 0; // Don't use this led, since it causes read issues with the BME280... Maybe bad soldering?
const int MAX_WIFI_ATTEMPTS = 20;
const int MAX_MQTT_ATTEMPTS = 10;
const long SLEEP_DURATION = 20000000; // 60s. Will need to make this customizable maybe...
const char* DEVICE_ID_PREFIX = "TEST_ATMCLN";
const long CHIP_ID = ESP.getChipId();

// used for stopping time.
long watch_start, watch_end, last_watch;

// For publish method
char temp_publish_value[15];

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BME280 bme; // I2C
StaticJsonBuffer<512> jsonBuffer;

void start_watch() {
	watch_start = millis();
}
void stop_watch() {
	watch_end = millis();
	last_watch = watch_end - watch_start;
}

/*
Connects to wifi. 
If not connected after MAX_WIFI_ATTEMPTS, aborts and goes to sleep.
TODO: customizable ssid/pw.
*/
void setup_wifi() {
	int attempts = 0;
	Serial.print("Connecting to ");
	Serial.println(ssid);

	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		if (attempts > MAX_WIFI_ATTEMPTS) {
			Serial.println("Max WIFI attempts reached.");
			return deep_sleep();
		}
		delay(500);
		Serial.print(".");
		attempts++;
	}

	Serial.println("");
	Serial.println("WiFi connected!");
	Serial.println("Local IP: ");
	Serial.println(WiFi.localIP());
}

/*
Connects to mqtt broker.
If not connected after MAX_MWTT_ATTEMPTS, aborts and goes to sleep.
TODO: customizable mqtt settings
TODO: mqtt security
*/
void setup_mqtt() {
	client.setServer(mqtt_server, 1883);
	int attempts = 0;

	while (!client.connected()) {
		if (attempts > MAX_MQTT_ATTEMPTS) {
			Serial.println("Max MQTT attempts reached.");
			return deep_sleep();
		}
		if (client.connect("atmoclon-prime")) {
			Serial.println("Connected to broker.");
			return;
		}
		else {
			Serial.print("Failed to connect to MQTT broker. RC=");
			Serial.print(client.state());
			Serial.println("Retrying in 5 seconds");
			// Wait 5 seconds before retrying
			delay(5000);
		}
		attempts++;
	}
}

void deep_sleep() {
	Serial.println("Going to sleep.");
	ESP.deepSleep(SLEEP_DURATION);
}

void setup() {
	Serial.begin(115200);

	// Check BME 
	if (!bme.begin()) {
		// Write some error to mqtt
		Serial.println("Could not find a valid BME280 sensor, check wiring!");
		return deep_sleep();
	}

	// Setup WIFI
	start_watch();
	setup_wifi();
	stop_watch();
	long wifi_setup_time = last_watch;

	// Setup MQTT
	start_watch();
	setup_mqtt();
	stop_watch();
	long mqtt_setup_time = last_watch;

	// Convert long CHIP_ID to char sequence
	char device_id_suffix[12];
	ltoa(CHIP_ID, device_id_suffix, 10);
	// Concat device id prefix and suffix
	char device_uid[sizeof(DEVICE_ID_PREFIX) + sizeof(device_id_suffix)];
	sprintf(device_uid, "%s%s", DEVICE_ID_PREFIX, device_id_suffix);

	// create json 
	JsonObject& reading = jsonBuffer.createObject();
	//reading["timestamp"] = "";

	JsonObject& readingData = reading.createNestedObject("json");
	readingData["temp"] = bme.readTemperature();
	readingData["humidity"] = bme.readHumidity();
	readingData["pressure"] = bme.readPressure();

	JsonObject& device = reading.createNestedObject("device");
	device["id"] = device_uid;
	device["type"] = "atmospheric";
	device["description"] = "Atmoclon weather station";
	device["battery_status"] = "Unknown";

	JsonObject& debug = reading.createNestedObject("debug_info");
	debug["wifi_time"] = wifi_setup_time;
	debug["mqtt_time"] = mqtt_setup_time;


	// json to string
	const int length = reading.measureLength() + 1;
	char* json = new char[length];
	reading.printTo(json, length);
	Serial.println(json);

	// Publish json
	bool success = client.publish("devices/readings", json);

	// Save energy
	Serial.println("All done.");
	deep_sleep();
}

void loop() {
	// Nothing to see here yet. TODO: AP mode will go here
}
