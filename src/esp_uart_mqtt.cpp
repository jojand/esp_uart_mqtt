#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "Streaming.h"

//#define SERIAL_DEBUG

#define FAST_LOOP       200
#define SLOW_LOOP		10000

#define INFO_HEADER     "[INFO] "
#define ERROR_HEADER    "[ERROR] "
#define DEBUG_HEADER    "[DEBUG] "
#define MQTT_HEADER     "[MQTT] "
#define RF_SETUP_TOPIC	"rf/config"

#define WIFI_SSID       	"***"
#define WIFI_PASSWORD   	"***"
#define MQTT_CLIENT_NAME	"esp_uart_mqtt"

#define SERIAL_BAUD     57600

#define MQTT_SERVER     "192.168.1.200"
#define MQTT_PORT       1883

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
    delay(10);
#ifdef SERIAL_DEBUG
    Serial << endl;
    Serial << endl;
    Serial << INFO_HEADER << "Connecting to: " << WIFI_SSID;
#endif

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
#ifdef SERIAL_DEBUG
        Serial.print(".");
#endif
    }

#ifdef SERIAL_DEBUG
    Serial << endl << INFO_HEADER << "WiFi connected, ip: " << WiFi.localIP()
            << endl;
#endif
}

void reconnect() {
    while (!client.connected()) {
#ifdef SERIAL_DEBUG
        Serial.print("Attempting MQTT connection...");
#endif
        // Create a random client ID
        String clientId = MQTT_CLIENT_NAME;
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
#ifdef SERIAL_DEBUG
        Serial.print("Attempting MQTT connection...");
#endif
        if (client.connect(clientId.c_str())) {
#ifdef SERIAL_DEBUG
            Serial.println("connected");
#endif
            client.subscribe(RF_SETUP_TOPIC);
        } else {
#ifdef SERIAL_DEBUG
            Serial.print("failed, rc=");
#endif
            Serial.print(client.state());
#ifdef SERIAL_DEBUG
            Serial.println(" try again in 5 seconds");
#endif
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
	char message[length+1];
	for (unsigned int i = 0; i < length; i++) {
		message[i] = payload[i];
	}
	message[length] = '\0';
	Serial << MQTT_HEADER << topic << " " << message << "*" << "\n";
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial.swap();
    setup_wifi();
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(callback);


    if (!client.connected()) {
        reconnect();
    }
    client.loop();
}

void parseAndPublish() {
    char buffer[100];
    Serial.setTimeout(500);
    size_t bytesRead = Serial.readBytesUntil('\n', buffer, sizeof(buffer));
    buffer[bytesRead] = '\0';
#ifdef SERIAL_DEBUG
    Serial << DEBUG_HEADER << buffer << endl;
#endif

    if ((bytesRead != 0) && (buffer[bytesRead-1] == '*'))  {

        String inMessage(buffer);

        if (inMessage.startsWith(MQTT_HEADER, 0)) {
            String mqttMessage = inMessage.substring(String(MQTT_HEADER).length());
#ifdef SERIAL_DEBUG
            Serial << DEBUG_HEADER << mqttMessage << endl;
#endif
            int spacePos = mqttMessage.indexOf(' ');
            String mqttTopic = mqttMessage.substring(0, spacePos);
            String mqttValue = mqttMessage.substring(spacePos+1, mqttMessage.length() -1);
#ifdef SERIAL_DEBUG
            Serial << INFO_HEADER << "Publishing topic: " << mqttTopic << ", value: " << mqttValue << endl;
#endif
            client.publish(mqttTopic.c_str(), mqttValue.c_str(), mqttValue.length());
        }
    } else {
#ifdef SERIAL_DEBUG
        Serial << ERROR_HEADER << "Error reading serial!" << endl;
#endif
    }
}

void fastLoop() {
    static unsigned long timeLoop = 0;
    client.loop();

    if ((millis() - timeLoop) > FAST_LOOP) {
        timeLoop = millis();

        if (!client.connected()) {
            reconnect();
        }

        if (Serial.available()) {
            parseAndPublish();
        }
    }
}

void slowLoop() {
    static unsigned long timeLoop = 0;

    if ((millis() - timeLoop) >= SLOW_LOOP) {
        timeLoop = millis();
        String topic = "esp/heartbeat";
        String message(millis());
#ifdef SERIAL_DEBUG
        Serial << INFO_HEADER << "Publishing heartbeat: " << millis() << endl;
#endif
        client.publish(topic.c_str(), message.c_str());
    }
}

void loop() {
    fastLoop();
    slowLoop();
}
