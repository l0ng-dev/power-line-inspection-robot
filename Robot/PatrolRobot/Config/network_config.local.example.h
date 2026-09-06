/** Copy this file to network_config.local.h and replace every example value. */
#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#define WIFI_AUTO_CONNECT       1U
#define WIFI_DEFAULT_SSID       "YOUR_WIFI_SSID"
#define WIFI_DEFAULT_PASSWORD   "YOUR_WIFI_PASSWORD"

#define MQTT_BROKER_HOST       "bemfa.com"
#define MQTT_BROKER_PORT       9501U
#define MQTT_BASE_TOPIC        "example_topic"
#define MQTT_K230_EVENT_TOPIC  "example_event"

#define MQTT_AUTO_CONNECT      1U
#define MQTT_TELEMETRY_INTERVAL_MS 5000U

#endif /* NETWORK_CONFIG_H */
