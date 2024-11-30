#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <MQTTClient.h>
#include "Secrets.c"
char PAYLOAD[50] = "nee";

void SendMsg(char PAYLOAD[50]){
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    // Initialize the MQTT client
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    // Set connection options
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // Connect to the broker
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        return;
    }

    // Create the message
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = PAYLOAD;
    pubmsg.payloadlen = strlen(PAYLOAD);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    // Publish the message
    MQTTClient_deliveryToken token;
    MQTTClient_publishMessage(client, SendTopic, &pubmsg, &token);
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    printf("Message with delivery token %d delivered\n", token);

    // Disconnect from the broker
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}

int main(){
    char msg[50] = "Error msg here.";
    SendMsg(msg);
}