#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <wiringPiSPI.h>
#include <mcp3004.h>

void readSerial(int fd,char *str,int *n) {
char inChar;
*n=0;
delay(200);
while(serialDataAvail(fd)) {
inChar=serialGetchar(fd);
str[*n]=inChar;
(*n)++;
 }
(*n)--;//Remove’\r’character
(*n)--;//Removeextracharduetowhile
str[*n]='\0';
}

uint32_t floatToHex(float value) {
    union {
        float input;
        uint32_t output;
    } data;

    data.input = value;

    return data.output;

}

// Función para concatenar dos valores uint32_t
uint64_t concatHexValues(uint32_t value1, uint32_t value2) {
    return ((uint64_t)value1 << 32) | value2;
}

int spiChanel=0;
int speed=100000;
int pinBase=100;
const long referenceMv = 3300;

//interpolación de la distancia a intervalos de 250mV
const int TABLE_ENTRIES = 12;
const int INTERVAL  = 250;
static int distance[12] = {150,140,130,100,60,50,40,35,30,25,20,15};

int interpolacion(int mV) {
  if (mV > INTERVAL * TABLE_ENTRIES - 1)      return distance[TABLE_ENTRIES - 1];
  else {
    int index = mV / INTERVAL;
    float frac = (mV % 250) / (float)INTERVAL;
    return distance[index] - ((distance[index] - distance[index + 1]) * frac);
  }
}

int lectura;

int main() {
	int fd;
	char msg[100];
	int msg_len;
	if((fd=serialOpen("/dev/serial0",9600))<0) {
	fprintf(stderr,"Unableto openserialdevice: %s\n",strerror(errno));
	return 1;
	}
	if(wiringPiSetup()==-1){
	fprintf(stdout,"UnabletostartwiringPi: %s\n",strerror(errno));
	return 1 ;
	}

	//// Configurar WiringPI
	wiringPiSetup();
	//// Configurar SPI
	wiringPiSPISetup(spiChanel, speed);
	//// Configurar mcp3004
	mcp3004Setup (pinBase, spiChanel);

	//Distancia
	int mV;
	float cm;
		lectura=analogRead(pinBase);
		mV = (lectura * referenceMv) / 1023;
		cm = interpolacion(mV);
		printf("mV=%d\n", mV);
		printf("distancia=%f\n\n", cm);

		//Temperatura

	serialPrintf(fd,"AT$T?\n");
	readSerial(fd,msg,&msg_len);

	float temp=atof(msg)/10;

    float float1 = temp; // Ejemplo de un número flotante
    float float2 = cm; // Segundo número flotante

    uint32_t hex1 = floatToHex(float1); // Valor hexadecimal del primer float
    uint32_t hex2 = floatToHex(float2); // Valor hexadecimal del segundo float

    uint64_t concatenatedValue = concatHexValues(hex1, hex2);

    printf("El valor float %.6f en hexadecimal es: 0x%016X\n", float1, hex1);
    printf("El valor float %.6f en hexadecimal es: 0x%016X\n", float2, hex2);
    printf("La concatenación de ambos valores hexadecimales es: 0x%016lX\n", concatenatedValue);

    serialPrintf(fd,"AT$SF=%016lX\n",concatenatedValue);

	if(msg_len)
		printf("Respuesta: %s(%d)\n",msg,msg_len);



return 0;
}
