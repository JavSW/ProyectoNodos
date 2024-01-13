#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiringPiSPI.h>
#include <wiringPiI2C.h>
#include <wiringSerial.h>
#include <mcp3004.h>
#include <wiringPi.h>
#include <pthread.h>
#include <math.h>
#include <errno.h>

#define PWM_PIN 23

int value = 140;
int fd;
void on_connect_pwm(struct mosquitto *mosq, void *obj, int reason_code)
{
	int rc;
	printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
	if(reason_code != 0){
		mosquitto_disconnect(mosq);
	}

	rc = mosquitto_subscribe(mosq, NULL, "Rpi/Sens", 1);
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

	void readSerial(int fd, char *str, int *n) {
    char inChar;
    *n = 0;
    delay(200);
    while (serialDataAvail(fd)) {
        inChar = serialGetchar(fd);
        str[*n] = inChar;
        (*n)++;
    }
    (*n)--;  // Eliminar el carácter '\r'
    (*n)--;  // Eliminar el carácter extra debido al bucle
    str[*n] = '\0';
}

uint32_t floatToHex(float value) {
    union {
        float input;
        uint32_t output;
    } data;

    data.input = value;

    return data.output;
    
}

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	//printf("%s %d %s\n", msg->topic, msg->qos, (char *)msg->payload);
	double cond=atof((char *)msg -> payload);
	if (cond==1){


    // Lectura de temperatura

    char msg[100];
    int msg_len;

    serialPrintf(fd, "AT$T?\n");
    readSerial(fd, msg, &msg_len);

    float temp_float = atof(msg) / 10;
    printf("temp_entero=%f\n",temp_float);

    // Impresión de resultados

    uint32_t temp_float_hex=floatToHex(temp_float);
    
    printf("Temperatura: %08X\n", temp_float_hex);

    // Envío de datos por el puerto serial
    serialPrintf(fd, "AT$SF=%08X\n",temp_float_hex);
    	if(msg_len)
            printf("Respuesta: %s(%d)\n",msg,msg_len);

}

	}




int main(int argc, char *argv[])
{
    wiringPiSetup();
    
    if ((fd = serialOpen("/dev/serial0", 9600)) < 0) {
        fprintf(stderr, "No se puede abrir el dispositivo serial: %s\n", strerror(errno));
        return 1;
    }
    if (wiringPiSetup() == -1) {
        fprintf(stdout, "No se puede iniciar WiringPi: %s\n", strerror(errno));
        return 1;
    }

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

