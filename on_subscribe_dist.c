
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
#define LED2 22 //BCM 6

void on_connect_dist(struct mosquitto *mosq, void *obj, int reason_code)
{
	int rc;
	printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
	if(reason_code != 0){
		mosquitto_disconnect(mosq);
	}

	rc = mosquitto_subscribe(mosq, NULL, "example/dist", 1);
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
double dist_ref;
double diff;
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	pinMode(LED2,OUTPUT); //// LED 2
	//printf("%s %d %s\n", msg->topic, msg->qos, (char *)msg->payload);
	double dist=atof((char *)msg -> payload);
	if(i==0){
		dist_ref=dist;
		i++;
	}
	if (dist<dist_ref-15){
		digitalWrite(LED2,HIGH);
		diff=dist_ref-dist;
		printf("Se ha acercado %lf cm \n",diff);
	}
	else if (dist>dist_ref+15){
		digitalWrite(LED2,HIGH);
		diff=dist-dist_ref;
		printf("Se ha alejado %lf cm \n",diff);
	}
	else{
		digitalWrite(LED2,LOW);
	}
	printf("\ndist=%lf\n dist_ref=%lf \n",dist,dist_ref);
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

	mosquitto_connect_callback_set(mosq, on_connect_dist);
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

