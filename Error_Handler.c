#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <MQTTClient.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#define ADDRESS "tcp://192.168.0.105:1883"
#define CLIENTID "RaspberryPiClient"
#define SENDTOPIC "send"
#define RECIEVETOPIC "recieve"
#define QOS 1
#define TIMEOUT 200L

// Globale variabelen
volatile int message_received = 0;
char received_message[256];
pthread_mutex_t lock;
int SEV_Code;
char naam[100];
char Error_Code[100];
char ExtraFile[200];
char send_msg[256];
bool extrafile = false;

//Linked list aanmaken
struct tbl
{
    char Err_Code[100];
    char Err_Tekst[200];
    struct tbl *next;
};

struct tbl *head = NULL;
struct tbl *current = NULL;

// Functieprototypes
void initClient(MQTTClient *client);
int connectToBroker(MQTTClient *client, MQTTClient_connectOptions *conn_opts);
void subscribeToTopic(MQTTClient *client, const char *topic);
int messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message);
void insert_first(char *Err_Code, char *Err_Tekst);
void insert_next(struct tbl *list, char *Err_Code, char *Err_Tekst);
int search_list(struct tbl **list, char *Err_Code);
void find_code(char *Err_Code);
void format_msg(char *received_message);
void print_list();

//Initialiseer de MQTT-client
void initClient(MQTTClient *client)
{
    MQTTClient_create(client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(*client, client, NULL, messageArrived, NULL);
}

//Verbind met de broker
int connectToBroker(MQTTClient *client, MQTTClient_connectOptions *conn_opts)
{
    conn_opts->keepAliveInterval = 20;
    conn_opts->cleansession = 1;

    int rc = MQTTClient_connect(*client, conn_opts);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        printf("Verbinding met broker mislukt, foutcode %d\n", rc);
    }
    else
    {
        printf("Succesvol verbonden met broker.\n");
    }
    return rc;
}

//Abonneer op een topic
void subscribeToTopic(MQTTClient *client, const char *topic)
{
    int rc = MQTTClient_subscribe(*client, topic, QOS);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        printf("Abonneren op topic '%s' mislukt, foutcode %d\n", topic, rc);
        exit(EXIT_FAILURE);
    }
    printf("Succesvol geabonneerd op topic: %s\n", topic);
}

//Format een bericht
void format_msg(char *received_message)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char *time_str = asctime(tm);
    time_str[strcspn(time_str, "\n")] = '\0';

    char output[256] = "";
    strncpy(output, received_message + 1, strlen(received_message) - 2);
    output[strlen(received_message) - 2] = '\0';

    char *token = strtok(output, ";");
    if (token != NULL)
    {
        SEV_Code = atoi(token);
    }
    token = strtok(NULL, ";");
    if (token != NULL)
    {
        strncpy(naam, token, sizeof(naam));
    }
    token = strtok(NULL, ";");
    if (token != NULL)
    {
        strncpy(Error_Code, token, sizeof(Error_Code));
    }
    token = strtok(NULL, ";");
    if (token != NULL)
    {
        strncpy(ExtraFile, token, sizeof(ExtraFile));
        extrafile = true;
    }
    else
    {
        extrafile = false;
        printf("ExtraFile is niet aanwezig.\n");
    }

    if (SEV_Code > 4)
    {
        SEV_Code = 4;
    }

    find_code(Error_Code);

    char Tekst[256];
    if (extrafile == true && current != NULL)
    {
        snprintf(Tekst, sizeof(Tekst), current->Err_Tekst, ExtraFile);
    }
    else if (current != NULL)
    {
        strncpy(Tekst, current->Err_Tekst, sizeof(Tekst));
    }

    snprintf(send_msg, sizeof(send_msg), "<%s;%d;%s;%s;%s>", time_str, SEV_Code, naam, current->Err_Code, Tekst);
    printf("%s\n", send_msg);
}

//Bericht ontvangen
int messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    pthread_mutex_lock(&lock);
    snprintf(received_message, sizeof(received_message), "%.*s", message->payloadlen, (char *)message->payload);
    message_received = 1;
    pthread_mutex_unlock(&lock);

    printf("Bericht ontvangen op topic '%s': %s\n", topicName, received_message);
    format_msg(received_message);

    MQTTClient *client = (MQTTClient *)context;

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = send_msg;
    pubmsg.payloadlen = strlen(send_msg);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(*client, SENDTOPIC, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        printf("Publiceren mislukt, foutcode %d\n", rc);
    }
    else
    {
        rc = MQTTClient_waitForCompletion(*client, token, TIMEOUT);
        printf("Bericht met token %d verzonden, status %d\n", token, rc);
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

//Het eerste element in de linked list zetten
void insert_first(char *Err_Code, char *Err_Tekst)
{
    struct tbl *lk = (struct tbl *)malloc(sizeof(struct tbl));
    strcpy(lk->Err_Code, Err_Code);
    strcpy(lk->Err_Tekst, Err_Tekst);
    lk->next = head;
    head = lk;
}
//De rest van de elementen in de linked list zetten
void insert_next(struct tbl *list, char *Err_Code, char *Err_Tekst)
{
    struct tbl *lk = (struct tbl *)malloc(sizeof(struct tbl));
    strcpy(lk->Err_Code, Err_Code);
    strcpy(lk->Err_Tekst, Err_Tekst);
    lk->next = NULL;
    list->next = lk;
}
//Een element in de linked list zoeken
int search_list(struct tbl **list, char *Err_Code)
{
    struct tbl *temp = head;
    while (temp != NULL)
    {
        if (strcmp(temp->Err_Code, Err_Code) == 0)
        {
            *list = temp;
            return 1;
        }
        temp = temp->next;
    }
    return 0;
}
//Alle variabelen van het gezochte element uit de linked list halen
void find_code(char *Err_Code)
{
    if (search_list(&current, Err_Code) == 1)
    {
        printf("Data gevonden voor %s\n", Err_Code);
    }
    else
    {
        printf("Error code niet gevonden.\n");
    }
}
//De lijst printen
void print_list()
{
    struct tbl *temp = head;
    printf("Gelinkte lijst inhoud:\n");
    while (temp != NULL)
    {
        printf("Error Code: %s, Error Tekst: %s\n", temp->Err_Code, temp->Err_Tekst);
        temp = temp->next;
    }
}

int main()
{
    printf("Choose your language: EN, NL, FR:\n");//Een bericht aan de gebruiker voor de taal
    char *taal;
    fgets(taal, sizeof(taal), stdin);//Zet de variebele gelijk met de input

    char FileName[100];
    snprintf(FileName, sizeof(FileName), "/home/milo/everything/Error_msg_%s.txt", taal);//Update de filenaam met de gekozen taal

    FILE *fp = fopen(FileName, "r");//Open de file
    if (fp == NULL)
    {
        printf("Fout: bestand %s kan niet worden geopend.\n", FileName);
        return 0;
    }
    //Lees het hele bestand in
    char buffer[256];
    bool first = true;
    while (fgets(buffer, sizeof(buffer), fp))
    {
        if (buffer[0] == '#')
            continue;

        char Err_Code[10], Err_Tekst[50];
        sscanf(buffer, "%7s\t%50[^\n]", Err_Code, Err_Tekst);

        if (first)
        {
            insert_first(Err_Code, Err_Tekst);//Zet het eerste element in de linked list
            current = head;
            first = false;
        }
        else
        {
            insert_next(current, Err_Code, Err_Tekst);//Zet de elementen in de linked list
            current = current->next;
        }
    }
    //Verbinding maken met de mqtt broker
    pthread_mutex_init(&lock, NULL);
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    initClient(&client);
    if (connectToBroker(&client, &conn_opts) != MQTTCLIENT_SUCCESS)
    {
        return EXIT_FAILURE;
    }
    subscribeToTopic(&client, RECIEVETOPIC);
    printf("Wachten op berichten...\n");
    while (1)
    {
        sleep(1);
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    pthread_mutex_destroy(&lock);

    return 0;
}
