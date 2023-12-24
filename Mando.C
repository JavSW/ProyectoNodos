#include <mosquitto.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>

#define LSM6DS33 0x6A
#define LSM6DS33_CTRL1_XL 0x10
#define LSM6DS33_CTRL2_G  0x11
#define LSM6DS33_OUTX_L_XL 0x28
#define LSM6DS33_OUTY_L_XL 0x2A
#define LSM6DS33_OUTZ_L_XL 0x2C
#define alpha 0.2
#define CALIBRATION_DELAY_MS 10
#define SWITCH1 29
#define SWITCH2 28
#define SWITCH3 25

struct DatosAcelerometro {
    short accelX;
    short accelY;
    short accelZ;
    short gyroX;
    short gyroY;
    short gyroZ;
    double pitch;
    double roll;
    double yaw;
};

int lsm6ds33_fd;
unsigned char acelerometro_data_X[2];
unsigned char acelerometro_data_Y[2];
unsigned char acelerometro_data_Z[2];
short int acelerometro_n_X;
short int acelerometro_n_Y;
short int acelerometro_n_Z;
float acelerometro_offset_X_final;
float acelerometro_offset_Y_final;
float acelerometro_offset_Z_final;
int contador = 0;
int d = 0;

struct DatosHilo {
    struct mosquitto *mosq;
    struct DatosAcelerometro *datosAcel;
};

struct DatosAcelerometro datosAcel;

struct DatosPublicacionHilo {
    struct mosquitto *mosq;
    const char *canal;
    const char *mensaje;
};

void calibrarSensor() {
    acelerometro_offset_X_final = 0;
    acelerometro_offset_Y_final = 0;
    acelerometro_offset_Z_final = 0;
    int duracion = 5000;
    unsigned long t1 = millis();
    int n_medias = 0;

    while ((millis() - t1) < duracion) {
        delay(CALIBRATION_DELAY_MS);

        acelerometro_data_X[1] = wiringPiI2CReadReg8(lsm6ds33_fd, LSM6DS33_OUTX_L_XL);
        acelerometro_data_X[0] = wiringPiI2CReadReg8(lsm6ds33_fd, LSM6DS33_OUTX_L_XL + 1);
        acelerometro_data_Y[1] = wiringPiI2CReadReg8(lsm6ds33_fd, LSM6DS33_OUTY_L_XL);
        acelerometro_data_Y[0] = wiringPiI2CReadReg8(lsm6ds33_fd, LSM6DS33_OUTY_L_XL + 1);
        acelerometro_data_Z[1] = wiringPiI2CReadReg8(lsm6ds33_fd, LSM6DS33_OUTZ_L_XL);
        acelerometro_data_Z[0] = wiringPiI2CReadReg8(lsm6ds33_fd, LSM6DS33_OUTZ_L_XL + 1);

        acelerometro_n_X = (acelerometro_data_X[0] << 8) | acelerometro_data_X[1];
        acelerometro_n_Y = (acelerometro_data_Y[0] << 8) | acelerometro_data_Y[1];
        acelerometro_n_Z = (acelerometro_data_Z[0] << 8) | acelerometro_data_Z[1];

        acelerometro_n_X = acelerometro_n_X * 0.061 * 9.8 / 1000;
        acelerometro_n_Y = acelerometro_n_Y * 0.061 * 9.8 / 1000;
        acelerometro_n_Z = acelerometro_n_Z * 0.061 * 9.8 / 1000;

        acelerometro_n_X = (acelerometro_n_X - acelerometro_offset_X_final);
        acelerometro_n_Y = (acelerometro_n_Y - acelerometro_offset_Y_final);
        acelerometro_n_Z = (acelerometro_n_Z - acelerometro_offset_Z_final);

        if (contador % 5 != 0) {
            acelerometro_offset_X_final += acelerometro_n_X;
            acelerometro_offset_Y_final += acelerometro_n_Y;
            acelerometro_offset_Z_final += acelerometro_n_Z;
        } else {
            acelerometro_offset_X_final += acelerometro_n_X;
            acelerometro_offset_Y_final += acelerometro_n_Y;
            acelerometro_offset_Z_final += acelerometro_n_Z;
            acelerometro_offset_X_final /= 5;
            acelerometro_offset_Y_final /= 5;
            acelerometro_offset_Z_final /= 5;
            acelerometro_offset_X_final = 0;
            acelerometro_offset_Y_final = 0;
            acelerometro_offset_Z_final = 0;
            n_medias++;
        }

        contador++;
    }

    acelerometro_offset_X_final /= n_medias;
    acelerometro_offset_Y_final /= n_medias;
    acelerometro_offset_Z_final /= n_medias;
}

void configurarLSM6DS33() {
    lsm6ds33_fd = wiringPiI2CSetup(LSM6DS33);
    wiringPiI2CWriteReg8(lsm6ds33_fd, LSM6DS33_CTRL1_XL, 0b01010001);
    wiringPiI2CWriteReg8(lsm6ds33_fd, LSM6DS33_CTRL2_G, 0b01010001);
}

void leerDatosSensor(struct DatosAcelerometro *datosAcel) {
    acelerometro_data_X[1] = wiringPiI2CReadReg8(lsm6ds33_fd, LSM6DS33_OUTX_L_XL);
    acelerometro_data_X[0] = wiringPiI2CReadReg8(lsm6ds33_fd, LSM6DS33_OUTX_L_XL + 1);
    acelerometro_data_Y[1] = wiringPiI2CReadReg8(lsm6ds33_fd, LSM6DS33_OUTY_L_XL);
    acelerometro_data_Y[0] = wiringPiI2CReadReg8(lsm6ds33_fd, LSM6DS33_OUTY_L_XL + 1);
    acelerometro_data_Z[1] = wiringPiI2CReadReg8(lsm6ds33_fd, LSM6DS33_OUTZ_L_XL);
    acelerometro_data_Z[0] = wiringPiI2CReadReg8(lsm6ds33_fd, LSM6DS33_OUTZ_L_XL + 1);

    acelerometro_n_X = (acelerometro_data_X[0] << 8) | acelerometro_data_X[1];
    acelerometro_n_Y = (acelerometro_data_Y[0] << 8) | acelerometro_data_Y[1];
    acelerometro_n_Z = (acelerometro_data_Z[0] << 8) | acelerometro_data_Z[1];

    acelerometro_n_X = acelerometro_n_X * 0.061 * 9.8 / 1000;
    acelerometro_n_Y = acelerometro_n_Y * 0.061 * 9.8 / 1000;
    acelerometro_n_Z = acelerometro_n_Z * 0.061 * 9.8 / 1000;

    acelerometro_n_X = (acelerometro_n_X - acelerometro_offset_X_final);
    acelerometro_n_Y = (acelerometro_n_Y - acelerometro_offset_Y_final);
    acelerometro_n_Z = (acelerometro_n_Z - acelerometro_offset_Z_final);

    datosAcel->roll = 180 / M_PI * atan(acelerometro_n_Y / sqrt(pow(acelerometro_n_X, 2) + pow(acelerometro_n_Z, 2)));
}

void *hiloPublicacionAcelerometro(void *data) {
    struct DatosHilo *datosHilo = (struct DatosHilo *)data;

    while (1) {
        // Obtener datos del acelerómetro (simulado)
        leerDatosSensor(datosHilo->datosAcel);

        // Publicar el valor de roll en el canal MQTT
        char rollMensaje[50];
        sprintf(rollMensaje, "%f", datosHilo->datosAcel->roll);

        int rc = mosquitto_publish(datosHilo->mosq, NULL, "Rpi/Roll", strlen(rollMensaje), rollMensaje, 0, false);

        if (rc == MOSQ_ERR_SUCCESS) {
            printf("Dato publicado satisfactoriamente en el canal Rpi/Roll\n");
        } else {
            fprintf(stderr, "Error al publicar dato: %s\n", mosquitto_strerror(rc));
        }

        // Pausa antes de la próxima publicación
        delay(1000);
    }

    return NULL;
}

void *hiloPublicacionZeros(void *data) {
    struct DatosHilo *datosHilo = (struct DatosHilo *)data;

    while (1) {
        // Verificar el estado de los interruptores
        int switch1State = digitalRead(SWITCH1);
        int switch3State = digitalRead(SWITCH3);

        char direccion[2];

        if (switch1State == LOW) {
            // Switch 1 está presionado, publicar "1"
            snprintf(direccion, sizeof(direccion), "1");
        } else if (switch3State == LOW) {
            // Switch 3 está presionado, publicar "2"
            snprintf(direccion, sizeof(direccion), "2");
        } else {
            // Ningún interruptor está presionado, publicar "0"
            snprintf(direccion, sizeof(direccion), "0");
        }

        // Publicar la dirección en el canal "Rpi/Direc"
        int rc = mosquitto_publish(datosHilo->mosq, NULL, "Rpi/Direc", strlen(direccion), direccion, 0, false);

        if (rc == MOSQ_ERR_SUCCESS) {
            printf("Dato publicado en Rpi/Direc: %s\n", direccion);
        } else {
            fprintf(stderr, "Error al publicar dirección: %s\n", mosquitto_strerror(rc));
        }

        // Pausa antes de la próxima publicación
        delay(1000);
    }

    return NULL;
}

void *hiloPublicacionSens(void *data) {
    struct DatosHilo *datosHilo = (struct DatosHilo *)data;

    while (1) {
        // Check the state of SWITCH2
        int switch2State = digitalRead(SWITCH2);

        // Publish "1" if SWITCH2 is pressed, otherwise publish "0"
        const char *sensMensaje = (switch2State == LOW) ? "1" : "0";

        int rc = mosquitto_publish(datosHilo->mosq, NULL, "Rpi/Sens", strlen(sensMensaje), sensMensaje, 0, false);

        if (rc == MOSQ_ERR_SUCCESS) {
            printf("Dato publicado en Rpi/Sens: %s\n", sensMensaje);
        } else {
            fprintf(stderr, "Error al publicar dato: %s\n", mosquitto_strerror(rc));
        }

        // Pausa antes de la próxima publicación
        delay(1000);
    }

    return NULL;
}


void on_connect(struct mosquitto *mosq, void *obj, int reason_code) {
    printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
    if (reason_code != 0) {
        mosquitto_disconnect(mosq);
    }
}

int main() {
    // Configuración de WiringPi y pines...
    if (wiringPiSetup() == -1) {
        fprintf(stderr, "Error inicializando WiringPi\n");
        return 1;
    }
    pinMode(SWITCH1, INPUT);
    pullUpDnControl(SWITCH1, PUD_UP);
    pinMode(SWITCH2, INPUT);
    pullUpDnControl(SWITCH2, PUD_UP);
    pinMode(SWITCH3, INPUT);
    pullUpDnControl(SWITCH3, PUD_UP);

    // Inicialización de la biblioteca Mosquitto...
    mosquitto_lib_init();

    // Crear instancia de Mosquitto
    struct mosquitto *mosq = mosquitto_new(NULL, true, NULL);
    if (mosq == NULL) {
        fprintf(stderr, "Error: Sin memoria disponible.\n");
        return 1;
    }

    // Configurar la devolución de llamada de conexión
    mosquitto_connect_callback_set(mosq, on_connect);

    // Conectar al broker MQTT
    int rc = mosquitto_connect(mosq, "test.mosquitto.org", 1883, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
        return 1;
    }

    // Iniciar el bucle de red
    mosquitto_loop_start(mosq);

    // Inicializar y calibrar el sensor...
    lsm6ds33_fd = wiringPiI2CSetup(LSM6DS33);
    if (lsm6ds33_fd == -1) {
        fprintf(stderr, "Error inicializando LSM6DS33\n");
        return 1;
    }
    calibrarSensor();
    configurarLSM6DS33();

    // Estructura para el hilo de datos del acelerómetro
    struct DatosHilo datosHiloAcel;
    datosHiloAcel.mosq = mosq;
    datosHiloAcel.datosAcel = &datosAcel;

    // Hilo para la publicación de datos del acelerómetro
    pthread_t hiloPublicacionAcelerometroId;
    if (pthread_create(&hiloPublicacionAcelerometroId, NULL, hiloPublicacionAcelerometro, (void *)&datosHiloAcel) != 0) {
        fprintf(stderr, "Error creando el hilo de datos del acelerómetro\n");
        return 1;
    }

    // Estructura para el hilo de dirección cero
    struct DatosHilo datosHiloCero;
    datosHiloCero.mosq = mosq;
    datosHiloCero.datosAcel = &datosAcel;

    // Hilo para la publicación de dirección cero
    pthread_t hiloPublicacionCeroId;
    if (pthread_create(&hiloPublicacionCeroId, NULL, hiloPublicacionZeros, (void *)&datosHiloCero) != 0) {
        fprintf(stderr, "Error creando el hilo de dirección cero\n");
        return 1;
    }

    // Estructura para el hilo de datos de Sens
    struct DatosHilo datosHiloSens;
    datosHiloSens.mosq = mosq;
    datosHiloSens.datosAcel = &datosAcel;

    // Hilo para la publicación de datos de Sens
    pthread_t hiloPublicacionSensId;
    if (pthread_create(&hiloPublicacionSensId, NULL, hiloPublicacionSens, (void *)&datosHiloSens) != 0) {
        fprintf(stderr, "Error creando el hilo de datos de Sens\n");
        return 1;
    }

    // Bucle principal para leer datos del acelerómetro
    while (1) {
        leerDatosSensor(&datosAcel);
        printf("Roll: %f\n", datosAcel.roll);

        // Pausa antes de la próxima lectura
        delay(1000);
    }

    // Esperar a que los hilos terminen (esto nunca sucederá en este ejemplo)
    pthread_join(hiloPublicacionAcelerometroId, NULL);
    pthread_join(hiloPublicacionCeroId, NULL);
    pthread_join(hiloPublicacionSensId, NULL);

    // Desconectarse de MQTT y limpiar los recursos
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}
