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

#define SERVO 1 //BCM 18

///SE SUSCRIBE A ACELEROMETRO/PWM

void on_connect_pwm(struct mosquitto *mosq, void *obj, int reason_code)
{
	int rc;
	printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
	if(reason_code != 0){
		mosquitto_disconnect(mosq);
	}

	rc = mosquitto_subscribe(mosq, NULL, "Rpi/Roll", 1);
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


int spiChanel=0;
int speed=50000;
int pinBase=100;
const long referenceMv = 3300;

//interpolación de la distancia a intervalos de 250mV
const int TABLE_ENTRIES = 12;
const int INTERVAL  = 250;
static int distance[12] = {150,140,130,100,60,50,40,35,30,25,20,15};

int interpolacion(int mV) {
	if (mV > INTERVAL * TABLE_ENTRIES - 1)      
		return distance[TABLE_ENTRIES - 1];
	else {
		int index = mV / INTERVAL;
		float frac = (mV % 250) / (float)INTERVAL;
		return distance[index] - ((distance[index] - distance[index + 1]) * frac);
  }
}


int cm;
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	//printf("%s %d %s\n", msg->topic, msg->qos, (char *)msg->payload);
	int rango=2400;
	int divisor=160;
	double roll=atof((char *)msg -> payload);
	double value=roll+180;
		//Calculamos la distancia

	int lectura=analogRead(pinBase);
	int mV = (lectura * referenceMv) / 1023;
	cm = interpolacion(mV);
	printf("mV=%d\n", mV);
	printf("distancia=%d\n\n", cm);

    
	if(cm < 70){
		pwmWrite(SERVO,0);
	}
	else{
		////Procedemos a la configuración del servo
		pinMode(SERVO,PWM_OUTPUT); //// PWM OUTPUT
		pwmSetMode(PWM_MODE_MS); //// MODO PWM
		pwmSetRange(rango);
		pwmSetClock(divisor);
		pwmWrite(SERVO, value);
	}
}


int main(int argc, char *argv[])
{
	wiringPiSetup();
	//// Configurar SPI
	wiringPiSPISetup(spiChanel, speed);
	//// Configurar mcp3004
	mcp3004Setup (pinBase, spiChanel);

	struct mosquitto *mosq;
	int rc;

	mosquitto_lib_init();

	mosq = mosquitto_new(NULL, true, NULL);
	if(mosq == NULL){
		fprintf(stderr, "Error: Out of memory.\n");
		return 1;
	}

	mosquitto_connect_callback_set(mosq, on_connect_pwm);
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

