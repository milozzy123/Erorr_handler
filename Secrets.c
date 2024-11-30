#define ADDRESS     "tcp://192.168.0.105:1883"      // MQTT broker URL
#define CLIENTID    "RaspberryPiClient"             // Unique client ID
#define GetTopic    "GetErrorMsg"                   // MQTT topic for recieving
#define SendTopic   "SendErrorMsg"                  // MQTT topic for sending
#define QOS         1                               // Quality of Service level
#define TIMEOUT     10000L                          // Timeout in milliseconds