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
#include <math.h>

#define PWM_PIN 23

int value = 140;
void on_connect_pwm(struct mosquitto *mosq, void *obj, int reason_code)
{
	int rc;
	printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
	if(reason_code != 0){
		mosquitto_disconnect(mosq);
	}

	rc = mosquitto_subscribe(mosq, NULL, "Rpi/Direc", 1);
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

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	//printf("%s %d %s\n", msg->topic, msg->qos, (char *)msg->payload);
	double giro=atof((char *)msg -> payload);
	double value= 140;
        if (giro == 1) {
            value = round(9 * (1.05) + value);
            pwmWrite(PWM_PIN, value);
            printf("Valor actual: %d\n", value);
            delay(150);
        } else if (giro == 2) {
            value = round(9 * (-1.05) + value);
            pwmWrite(PWM_PIN, value);
            printf("Valor actual: %d\n", value);
            delay(150);
        } else {
            // No se presionaron los interruptores, mantener el valor actual
            pwmWrite(PWM_PIN, value);
            printf("Valor actual: %d\n", value);
        }
}


int main(int argc, char *argv[])
{
    wiringPiSetup();
    pinMode(PWM_PIN, PWM_OUTPUT);
    pwmSetMode(PWM_MODE_MS);
    pwmSetClock(192);
    pwmSetRange(2250);
    pwmWrite(PWM_PIN, value);

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

