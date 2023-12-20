#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#define LSM6DS33 0x6A // Dirección del LSM6DS33
#define conversion 16 //16 LSB/ºC
#define LED1 30 

float ref=0.0;

float lsm6ds33_get_temp(int fd) {
    short raw_data;
    unsigned char data_read[2];
    float temp;

    delayMicroseconds(500); // Se tiene que esperar 500 microsegundos

    // Leer los 16 bits del sensor
    data_read[0] = wiringPiI2CReadReg8(fd, 0x21); // OUT_TEMP_H
    data_read[1] = wiringPiI2CReadReg8(fd, 0x20); // OUT_TEMP_L

    // Concatenar los 2 bytes y procesar de acuerdo al datasheet
    raw_data = (data_read[0] << 8) | data_read[1];

    printf("data_read[0] = %d data_read[1] = %d \n", data_read[0], data_read[1]);

    temp = ((float)raw_data/16 +25); 

    printf("temp = %f data = %i\n", temp, raw_data);

    return temp;
}

void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
{
	int rc;

	printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
	if(reason_code != 0){
	
		mosquitto_disconnect(mosq);
	}


	rc = mosquitto_subscribe(mosq, NULL, "Rpi/temperature", 1);
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


/* Callback called when the client receives a message. */
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	printf("%s %d %s\n", msg->topic, msg->qos, (char *)msg->payload);
	double temperatura = atof(msg->payload); //atof convierte strings de chars en doubles
	double difftemp= temperatura - (double)ref;
	printf("La diferencia de temperatura es: %f\n",difftemp);
	if(difftemp >= 1){
		digitalWrite(LED1, HIGH);
	}
	else {
		digitalWrite(LED1, LOW);
	}
}


int main(int argc, char *argv[])
{
	struct mosquitto *mosq;
	int rc;
	int fd;
    fd = wiringPiI2CSetup(LSM6DS33);
    int data_config = 0b00010000; // Configuracion del acelerometro (low power, debe estar activado)

    wiringPiI2CWriteReg8(fd, 0x10, data_config); // Ajuste del registro de configuración

    wiringPiSetup();
    pinMode(LED1, OUTPUT);

    // Leer temperatura
    ref=lsm6ds33_get_temp(fd);

	/* Required before calling other mosquitto functions */
	mosquitto_lib_init();

	mosq = mosquitto_new(NULL, true, NULL);
	if(mosq == NULL){
		fprintf(stderr, "Error: Out of memory.\n");
		return 1;
	}

	mosquitto_connect_callback_set(mosq, on_connect);
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
