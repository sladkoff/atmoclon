/*
 Name:		atmoclon.ino
 Created:	3/14/2016 10:08:53 PM
 Author:	leonid.koftun@gmail.com
*/

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
const long SLEEP_DURATION = 60000000; // One minute. Will need to make this customizable maybe...

// used for stopping time.
long watch_start, watch_end, last_watch;

// For publish method
char temp_publish_value[15];

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BME280 bme; // I2C

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

/*
Publishes a float value to broker. 
TODO: optimize? Variable width/precision.
*/
void publish_float(const char *topic, float value) {
	dtostrf(value, 2, 3, temp_publish_value);
	client.publish(topic, temp_publish_value);
}

void deep_sleep() {
	Serial.println("Going to sleep.");
	ESP.deepSleep(60000000);
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

	// Publish debug information
	publish_float("atmoclon/debug/wifi-time", wifi_setup_time);
	publish_float("atmoclon/debug/mqtt-time", mqtt_setup_time);

	// Publish BME values
	publish_float("atmoclon/temp", bme.readTemperature());
	publish_float("atmoclon/humidity", bme.readHumidity());
	publish_float("atmoclon/pressure", bme.readPressure());

	// Save energy
	Serial.println("All done.");
	deep_sleep();
}

void loop() {
	// Nothing to see here yet. TODO: AP mode will go here
}
