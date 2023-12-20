
#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiringPiSPI.h>
#include <wiringPiI2C.h>
#include <mcp3004.h>
#include <wiringPi.h>
#include <pthread.h>

#define LED1 24 //BCM 19
#define LED3 21 //BCM 5

void on_connect_temp(struct mosquitto *mosq, void *obj, int reason_code)
{
	int rc;
	printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
	if(reason_code != 0){
		mosquitto_disconnect(mosq);
	}

	rc = mosquitto_subscribe(mosq, NULL, "example/temp", 1);
	if(rc != MOSQ_ERR_SUCCESS){
		fprintf(stderr, "Error subscribing: %s\n", mosquitto_strerror(rc));
		mosquitto_disconnect(mosq);
	}
}

void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;
	bool have_subscription = false;

	for(i=0; i<qos_count; i++){
		printf("on_subscribe: %d:granted qos = %d\n", i, granted_qos[i]);
		if(granted_qos[i] <= 2){
			have_subscription = true;
		}
	}
	if(have_subscription == false){
		fprintf(stderr, "Error: All subscriptions rejected.\n");
		mosquitto_disconnect(mosq);
	}
}



int i=0;
int indice;
double temp_j;
double temp_ant;
double temp_ref;
double temp[5];
double suma=0;
double media=0;
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	//printf("%s %d %s\n", msg->topic, msg->qos, (char *)msg->payload);
	temp_j= atof((char *)msg -> payload); //Convierte Char en Double
	int tamaño=sizeof(temp)/sizeof(temp[0]);
	indice=i%5;
	temp_ant=temp[indice];
	temp[indice]=temp_j;
	suma+=temp_j-temp_ant;
	if(i==0){
		temp_ref=temp_j;
	}
	if(i<4){
		media=temp_j;
	}
	else{
		media=suma/tamaño;
	}
	printf("\ntemperatura=%lf\n media=%lf\n",temp_j,media);
	i++;

	pinMode(LED1,OUTPUT); //// LED 1
	pinMode(LED3,OUTPUT); //// LED 3

	if (temp_j>=temp_ref+2){
		digitalWrite(LED1,HIGH);
		printf("\n La temperatura ha subido al menos 2 grados \n");
	}
	else{
		digitalWrite(LED1,LOW);
	}

	if (temp_j<=temp_ref-2){
		digitalWrite(LED3,HIGH);
		printf("\n La temperatura ha bajado al menos 2 grados \n");
	}
	else{
		digitalWrite(LED3,LOW);
	}

}


int main(int argc, char *argv[])
{
	wiringPiSetup();
	struct mosquitto *mosq;
	int rc;

	mosquitto_lib_init();

	mosq = mosquitto_new(NULL, true, NULL);
	if(mosq == NULL){
		fprintf(stderr, "Error: Out of memory.\n");
		return 1;
	}

	mosquitto_connect_callback_set(mosq, on_connect_temp);
	mosquitto_subscribe_callback_set(mosq, on_subscribe);
	mosquitto_message_callback_set(mosq, on_message);

	rc = mosquitto_connect(mosq, "test.mosquitto.org", 1883, 60);
	if(rc != MOSQ_ERR_SUCCESS){
		mosquitto_destroy(mosq);
		fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
		return 1;
	}

	mosquitto_loop_forever(mosq, -1, 1);

	mosquitto_lib_cleanup();
	return 0;
}

