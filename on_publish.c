/*
 * This example shows how to publish messages from outside of the Mosquitto network loop.
 */

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

#define LSM6DS33 0x6a //// dirección del dispositivo global

//DEFINICIóN DE VARIABLES GLOBALES
int fd;
#define spiChanel 0
#define speed 50000
#define pinBase 100
float t;


void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
{
	printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
	if(reason_code != 0){
		mosquitto_disconnect(mosq);
	}
}

void on_publish(struct mosquitto *mosq, void *obj, int mid)
{
	printf("Message with mid %d has been published.\n", mid);
}


int get_temperature(int fd)
{
	 /* Prevent a storm of messages - this pretend sensor works at 1Hz */
	short int raw_data;
	unsigned char data_read[2];


	/// leer los 16 bits del sensor
	data_read[0] = wiringPiI2CReadReg8(fd,0x21); // OUT_TEMP_H
	data_read[1] = wiringPiI2CReadReg8(fd,0x20); // OUT_TEMP_L

	/// concatenar los 2 bytes, y procesar de acuerdo al datasheet

	delayMicroseconds(500);

	raw_data =(data_read[0]<<8)|(data_read[1]) ;

	t = (raw_data/16+25 ); // Realizar correciones de lectura de  sensor T LSB/ºC

	return t;
}


int interpolacion(int mV) {
	const int TABLE_ENTRIES = 12;
	const int INTERVAL  = 250;
	static int distance[12] = {150,140,130,100,60,50,40,35,30,25,20,15};
	if (mV > INTERVAL * TABLE_ENTRIES - 1)      return distance[TABLE_ENTRIES - 1];
	else {
	int index = mV / INTERVAL;
	float frac = (mV % 250) / (float)INTERVAL;
	return distance[index] - ((distance[index] - distance[index + 1]) * frac);
  }
}

int get_distance(void)
{
	int fs=10; //Nos dicen que muestreemos a 10Hz
	float T=1/fs;
	delay(T);
	int lectura;
	int mV;
	int cm;
	const long referenceMv = 3300;



	lectura=analogRead(pinBase);
	mV = (lectura * referenceMv) / 1023;
	cm = interpolacion(mV);

	return cm;
}



void publish_sensor_data_temp(struct mosquitto *mosq)
{
	char payload_temp[20];
	int rc;

	get_temperature(fd);

	snprintf(payload_temp, sizeof(payload_temp), "%lf", t);

	printf("Temperature= %2.f\n",t);

	rc = mosquitto_publish(mosq, NULL, "example/temp", strlen(payload_temp), payload_temp, 2, false);
	if(rc != MOSQ_ERR_SUCCESS){
		fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
	}
}

void publish_sensor_data_dist(struct mosquitto *mosq)
{
	char payload_dist[20];
	int dist;
	int rc;

	dist = get_distance();

	snprintf(payload_dist, sizeof(payload_dist), "%d", dist);

	rc = mosquitto_publish(mosq, NULL, "example/dist", strlen(payload_dist), payload_dist, 2, false);
	if(rc != MOSQ_ERR_SUCCESS){
		fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
	}
}

void *publish_temp_thread (void *arg){

	struct mosquitto *mosq=(struct mosquitto *)arg;
	while (1) {
			publish_sensor_data_temp(mosq);
			delay(1000);
	}
	return NULL;
}

void *publish_distance_thread (void *arg){

	struct mosquitto *mosq=(struct mosquitto *)arg;
	while (1) {
			publish_sensor_data_dist(mosq);
			delay(100);
	}
	return NULL;

}


int main(int argc, char *argv[])
{
	struct mosquitto *mosq;
	int rc;
	int *pstatus;

	if (wiringPiSetup()	==	-1){
        fprintf(stderr, "Error al inicializar WiringPi\n");
        return 1;
	}


	if (wiringPiSPISetup(spiChanel, speed)	==	-1){
		fprintf(stderr, "Error al inicializar SPI\n");
        return 1;
	}

	mcp3004Setup (pinBase, spiChanel);

	fd=wiringPiI2CSetup(LSM6DS33);
	int data_config = 0b01010001; /// configuramos acelerómetro
	wiringPiI2CWriteReg8(fd, 0x10, data_config); /// configuramos acelerómetro

	pthread_t temp_thread, dist_thread;

	mosquitto_lib_init();

	mosq = mosquitto_new(NULL, true, NULL);
	if(mosq == NULL){
		fprintf(stderr, "Error: Out of memory.\n");
		return 1;
	}

	mosquitto_connect_callback_set(mosq, on_connect);
	mosquitto_publish_callback_set(mosq, on_publish);

	rc = mosquitto_connect(mosq, "test.mosquitto.org", 1883, 60);
	if(rc != MOSQ_ERR_SUCCESS){
		mosquitto_destroy(mosq);
		fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
		return 1;
	}

	rc = mosquitto_loop_start(mosq);
	if(rc != MOSQ_ERR_SUCCESS){
		mosquitto_destroy(mosq);
		fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
		return 1;
	}


		//Creamos hilos y les pasamos las instancias de mosquito a cada uno

		if(pthread_create(&temp_thread, NULL, publish_temp_thread, (void *)mosq)>0){
			fprintf(stderr,"Fallo al crear temp\n");
		}
		if(pthread_create(&dist_thread, NULL, publish_distance_thread, (void *)mosq)>0){
			fprintf(stderr,"Fallo al crear temp\n");
		}
		while(1){
			if(pthread_join(temp_thread, (void **) &pstatus)>0){
				fprintf(stderr,"Fallo del Join temp\n");
			}
			if(pthread_join(dist_thread, (void **) &pstatus)>0){
				fprintf(stderr,"Fallo del Join dist\n");
			}
		}

	mosquitto_lib_cleanup();
	return 0;
}

