#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <pthread.h>
#include <wiringPiSPI.h>
#include <mcp3004.h>

#define LSM6DS33 0x6A
#define conversion 16
#define SPI_CHANNEL 0      // Canal SPI del MCP3004
#define CLOCK_SPEED 10000 // Velocidad del reloj SPI (1 MHz)
#define PIN_BASE 100        // Base de pines para mcp3004Setup
#define TABLE_ENTRIES 12    // Número de entradas en la tabla de interpolación
#define INTERVAL 400        // Intervalo de interpolación en mV
static int distance[TABLE_ENTRIES] = {40, 30, 23, 16, 12, 10, 9, 8, 7, 6, 5, 4}; // Tabla de distancias en cm

struct ThreadData {
    struct mosquitto *mosq;
    int fd;
};

struct DistanceThreadData {
    struct mosquitto *mosq;
};

void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
{
    printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
    if (reason_code != 0) {
        mosquitto_disconnect(mosq);
    }
}

void on_publish(struct mosquitto *mosq, void *obj, int mid)
{
    printf("Message with mid %d has been published.\n", mid);
}

float lsm6ds33_get_temp(int fd) {
    short raw_data;
    unsigned char data_read[2];
    float temp;

    delayMicroseconds(500);

    data_read[0] = wiringPiI2CReadReg8(fd, 0x21);
    data_read[1] = wiringPiI2CReadReg8(fd, 0x20);

    raw_data = (data_read[0] << 8) | data_read[1];
    temp = ((float)raw_data / conversion + 25);

    return temp;
}

int getDistance(int mV) {
    if (mV > INTERVAL * (TABLE_ENTRIES - 1)) {
        return distance[TABLE_ENTRIES - 1];
    } else {
        int index = mV / INTERVAL;
        float frac = (mV % INTERVAL) / (float)INTERVAL;
        return distance[index] - ((distance[index] - distance[index + 1]) * frac);
    }
}

void *publish_temp_thread(void *data) {
    struct ThreadData *thread_data = (struct ThreadData *)data;

    while (1) {
        char payload[20];
        float temp;
        int rc;

        temp = lsm6ds33_get_temp(thread_data->fd);
        snprintf(payload, sizeof(payload), "%.2f", temp);
        printf("Temperature: %.2f\n", temp);

        rc = mosquitto_publish(thread_data->mosq, NULL, "Rpi/temperature", strlen(payload), payload, 2, false);
        if (rc != MOSQ_ERR_SUCCESS) {
            fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
        }

        delay(1000); // Adjust the delay as needed
    }

    return NULL;
}

void *publish_distance_thread(void *data) {
    struct DistanceThreadData *distance_data = (struct DistanceThreadData *)data;

    while (1) {
        char payload[20];
        int val, mV, cm;
        int rc;

        // Read distance values
        val = analogRead(PIN_BASE);
        mV = (val * 3300) / 1023;
        cm = getDistance(mV);

        // Publish distance to the topic
        snprintf(payload, sizeof(payload), "%d", cm);
        printf("Distance: %d cm\n", cm);

        rc = mosquitto_publish(distance_data->mosq, NULL, "Rpi/distance", strlen(payload), payload, 2, false);
        if (rc != MOSQ_ERR_SUCCESS) {
            fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
        }

        // Adjust the delay as needed
        delay(100);
    }

    return NULL;
}

int main() {
    struct mosquitto *mosq;
    int rc;
    int fd;
    pthread_t temp_thread_id, distance_thread_id;
    if (wiringPiSetup() == -1) {
        fprintf(stderr, "Error al inicializar WiringPi\n");
        return 1;
    }
    if (wiringPiSPISetup(SPI_CHANNEL, CLOCK_SPEED) == -1) {
        fprintf(stderr, "Error al inicializar SPI\n");
        return 1;
    }
    mcp3004Setup(PIN_BASE, SPI_CHANNEL);

    mosquitto_lib_init();

    mosq = mosquitto_new(NULL, true, NULL);
    if (mosq == NULL) {
        fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_publish_callback_set(mosq, on_publish);

    rc = mosquitto_connect(mosq, "test.mosquitto.org", 1883, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
        return 1;
    }

    rc = mosquitto_loop_start(mosq);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
        return 1;
    }

    fd = wiringPiI2CSetup(LSM6DS33);
    int data_config = 0b00010000;
    wiringPiI2CWriteReg8(fd, 0x10, data_config);

    struct ThreadData temp_thread_data = {mosq, fd};
    struct DistanceThreadData distance_thread_data = {mosq};

    // Create threads
    pthread_create(&temp_thread_id, NULL, publish_temp_thread, (void *)&temp_thread_data);
    pthread_create(&distance_thread_id, NULL, publish_distance_thread, (void *)&distance_thread_data);

    // Wait for threads to finish
    pthread_join(temp_thread_id, NULL);
    pthread_join(distance_thread_id, NULL);

    mosquitto_lib_cleanup();
    return 0;
}