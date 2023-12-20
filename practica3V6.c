////////////////////////////////////////////////////////////////////////////

////ESQUEMA DEL CÓDIGO////
	//Definimos los aparatos que usaremos
	//Función leo_angulo
		//Lectura inicial de la temperatura
		//Definimos las variables que vamos a usar
		//Calibramos durante 5s
			//Leemos los datos y los tratamos
			//Filtro pasabaja
			//Media cada 5 medidas (media de la medida 1 a la 5, después de la 6 a la 10,...)
			//Realizamos la media de estas medias para obtener el resultado final
		//Sensar y actuar
			//Leemos los datos y los tratamos
			//Aplicamos el offset
			//Calculamos los ángulos
			//Codificamos estas medidas para actuar sobre el servo
			//Leemos la temperatura
				//Si baja un grado respecto la inicial -> Apagamos 1 LED
				//Si baja otro grado -> Apagamos el otro LED
				//Si sube un grado o mas -> Parpadea 1 LED
			//Botón 1 reinicia el sistema y se recalibra
			//Botón 2 apaga el sistema
		//Main
		
////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <math.h>


#define LSM6DS33 0x6a //// dirección del dispositivo global
#define LED1 24 //BCM 19
#define LED2 22 //BCM 6
#define LED3 21 //BCM 5
#define SWITCH1 30 //BCM 0
#define SWITCH2 31 //BCM 1
#define SERVO 1 //BCM 18


	

void leo_angulo (int fd)
{
	////LECTURA INICIAL DE LA TEMPERATURA
	
	short int raw_data;
	unsigned char data_read[2];
	float temp1;
	float temp2;
	float diftemp;
	
	/// leer los 16 bits del sensor
	data_read[0] = wiringPiI2CReadReg8(fd,0x21); // OUT_TEMP_H
	data_read[1] = wiringPiI2CReadReg8(fd,0x20); // OUT_TEMP_L
	 
	/// concatenar los 2 bytes, y procesar de acuerdo al datasheet
	 
	delayMicroseconds(500);
	 
	raw_data =(data_read[0]<<8)|(data_read[1]) ;

	temp1 = (raw_data/16+25 ); // Realizar correciones de lectura de  sensor T LSB/ºC
				 
				 
	pinMode(SWITCH1,INPUT); //// Botón SW1
	pinMode(LED1,OUTPUT); //// LED 1
	pinMode(LED2,OUTPUT); //// LED 2
	pinMode(LED3,OUTPUT); //// LED 3
	

	
	const int n = 100; ////NUMERO MUESTRAS
	
	//// DEFINIMOS LAS VARIABLES PARA GUARAR LOS DATOS DEL ACELERÓMETRO Y GIRÓSCOPO
	
	///variables para la leectura de datos
	
	unsigned char giroscopo_data_X[2];
	unsigned char giroscopo_data_Y[2];
	unsigned char giroscopo_data_Z[2];
	unsigned char acelerometro_data_X[2];
	unsigned char acelerometro_data_Y[2];
	unsigned char acelerometro_data_Z[2];
	
	///Dato donde se guardará cada medida individual

	short int giroscopo_n_X;
	short int giroscopo_n_Z;
	short int giroscopo_n_Y;
		
	short int acelerometro_n_X;
	short int acelerometro_n_Y;
	short int acelerometro_n_Z;
		
	///// DEFINIMOS LOS ÁNGULOS

	double pitch;
	double roll;
	double yaw;
	
	////DEFINIMOS VARIABLES QUE UTILIZAREMOS PARA EL CÁLCULO DEL OFFSET
	
	///Para leer el giroscopo y el acelerometro

	float giroscopo_offset_X;
	float giroscopo_offset_Y;
	float giroscopo_offset_Z;
	float acelerometro_offset_X;
	float acelerometro_offset_Y;
	float acelerometro_offset_Z;
	
	///Los valores finales de offset para cada aparato y eje
	
	float giroscopo_offset_X_final;
	float giroscopo_offset_Y_final;
	float giroscopo_offset_Z_final;
	float acelerometro_offset_X_final;
	float acelerometro_offset_Y_final;
	float acelerometro_offset_Z_final;
	
	///aspectos de filtrado.
	
	float acelerometro_X_filt;
	float acelerometro_Y_filt;
	float acelerometro_Z_filt;
	float giroscopo_X_filt;
	float giroscopo_Y_filt;
	float giroscopo_Z_filt;
	
	float alpha=0.2; ////Factor de suavizado del filtro

	
	///variables para realizar la media
	
	float acelerometro_X_media;
	float acelerometro_Y_media;
	float acelerometro_Z_media;
	float giroscopo_X_media;
	float giroscopo_Y_media;
	float giroscopo_Z_media;
	
	float acelerometro_X_sum;
	float acelerometro_Y_sum;
	float acelerometro_Z_sum;
	float giroscopo_X_sum;
	float giroscopo_Y_sum;
	float giroscopo_Z_sum;
	
	float giroscopo_offset_X_1;
	float giroscopo_offset_Y_1;
	float giroscopo_offset_Z_1;
	float acelerometro_offset_X_1;
	float acelerometro_offset_Y_1;
	float acelerometro_offset_Z_1;
	
	int n_medias=0;
	
	////DATOS FINALES YA TRATADOS
	
	float Ax;
	float Ay;
	float Az;

	////DEFINIMOS VARIABLES DE MUESTREO
	
	int fs=100; ////Frecuencia de muestreo
	float T=1/fs; ////periodo
	
	
	double pi=M_PI;

	
	unsigned long t1=millis(); ////Tiempo inicial
	

	while (1) {
		
				////CALCULO DEL OFFSET////
				
				giroscopo_offset_X_1=0;
				giroscopo_offset_Y_1=0;
				giroscopo_offset_Z_1=0;
				acelerometro_offset_X_1=0;
				acelerometro_offset_Y_1=0;
				acelerometro_offset_Z_1=0;
		
				////Empezamos apagando todo
				digitalWrite(LED1,LOW);
				digitalWrite(LED2,LOW);
				digitalWrite(LED3,LOW);						
				int contador=0;
				int duracion=5000;
				
				if(contador==0){
					
					printf("Estamos calibrando, no mueva el LSM6DS33, espere 5 segundos...\n\n");
					digitalWrite(LED3,HIGH);
					
				}
				
				while ((millis()-t1)<duracion){
					
						delay(T); 
						
						////Introducimos un delay igual a la frecuencia de muestreo
						////Calculamos los offsets
						
						giroscopo_data_X[1] = wiringPiI2CReadReg8(fd,0x22); // OUTX_L_G
						giroscopo_data_X[0] = wiringPiI2CReadReg8(fd,0x23); // OUTX_H_G
						giroscopo_data_Y[1] = wiringPiI2CReadReg8(fd,0x24); // OUTY_L_G
						giroscopo_data_Y[0] = wiringPiI2CReadReg8(fd,0x25); // OUTY_H_G
						giroscopo_data_Z[1] = wiringPiI2CReadReg8(fd,0x26); // OUTZ_L_G
						giroscopo_data_Z[0] = wiringPiI2CReadReg8(fd,0x27); // OUTZ_H_G
						acelerometro_data_X[1] = wiringPiI2CReadReg8(fd,0x28); // OUTX_L_XL
						acelerometro_data_X[0] = wiringPiI2CReadReg8(fd,0x29); // OUTX_H_XL
						acelerometro_data_Y[1] = wiringPiI2CReadReg8(fd,0x2A); // OUTY_L_XL
						acelerometro_data_Y[0] = wiringPiI2CReadReg8(fd,0x2B); // OUTY_H_XL
						acelerometro_data_Z[1] = wiringPiI2CReadReg8(fd,0x2C); // OUTZ_L_XL
						acelerometro_data_Z[0] = wiringPiI2CReadReg8(fd,0x2D); // OUTZ_H_XL
					
						////Concatenamos
						giroscopo_offset_X =(giroscopo_data_X[0]<<8)|(giroscopo_data_X[1]) ;
						giroscopo_offset_Y =(giroscopo_data_Y[0]<<8)|(giroscopo_data_Y[1]) ;
						giroscopo_offset_Z =(giroscopo_data_Z[0]<<8)|(giroscopo_data_Z[1]) ;
						acelerometro_offset_X =(acelerometro_data_X[0]<<8)|(acelerometro_data_X[1]) ;
						acelerometro_offset_Y =(acelerometro_data_Y[0]<<8)|(acelerometro_data_Y[1]) ;
						acelerometro_offset_Z =(acelerometro_data_Z[0]<<8)|(acelerometro_data_Z[1]) ;
						
						////Tratamos los datos según se especifica en el datasheet
						
						giroscopo_offset_X=giroscopo_offset_X*8.75/1000;
						giroscopo_offset_Y=giroscopo_offset_Y*8.75/1000;
						giroscopo_offset_Z=giroscopo_offset_Z*8.75/1000;
						acelerometro_offset_X=acelerometro_offset_X*0.061*9.8/1000;
						acelerometro_offset_Y=acelerometro_offset_Y*0.061*9.8/1000;
						acelerometro_offset_Z=acelerometro_offset_Z*0.061*9.8/1000;
					
						////Filtro pasa baja					
						
						giroscopo_X_filt =alpha*(giroscopo_offset_X)+(1-alpha)*giroscopo_offset_X_1;
						giroscopo_Y_filt =alpha*(giroscopo_offset_Y)+(1-alpha)*giroscopo_offset_Y_1;
						giroscopo_Z_filt =alpha*(giroscopo_offset_Z)+(1-alpha)*giroscopo_offset_Z_1;
						acelerometro_X_filt =alpha*(acelerometro_offset_X)+(1-alpha)*acelerometro_offset_X_1;
						acelerometro_Y_filt =alpha*(acelerometro_offset_Y)+(1-alpha)*acelerometro_offset_Y_1;
						acelerometro_Z_filt =alpha*(acelerometro_offset_Z)+(1-alpha)*acelerometro_offset_Z_1;								
						
						
						////Si no hemos tomado aún 5 medidas, sumamos la medida en un variable
						
						if (contador%5!=0){
						
							giroscopo_X_sum+=giroscopo_X_filt;
							giroscopo_Y_sum+=giroscopo_Y_filt;
							giroscopo_Z_sum+=giroscopo_Z_filt;
							acelerometro_X_sum+=acelerometro_X_filt;
							acelerometro_Y_sum+=acelerometro_Y_filt;
							acelerometro_Z_sum+=acelerometro_Z_filt;
							
						}
						
						////Si en esa variable ya tenemos 5 medidas, entonces realizamos la media de estas y ponemos a 0 la variable que guarda la suma de las medidas.
						////Esta media se guarda también en una suma de medias sobre la cual se realizará la media para obtener los resultados finales
						
						else{
							
							giroscopo_X_sum+=giroscopo_X_filt;
							giroscopo_Y_sum+=giroscopo_Y_filt;
							giroscopo_Z_sum+=giroscopo_Z_filt;
							acelerometro_X_sum+=acelerometro_X_filt;
							acelerometro_Y_sum+=acelerometro_Y_filt;
							acelerometro_Z_sum+=acelerometro_Z_filt;
							
							giroscopo_X_media+=giroscopo_X_sum/5;
							giroscopo_Y_media+=giroscopo_Y_sum/5;
							giroscopo_Z_media+=giroscopo_Z_sum/5;
							acelerometro_X_media+=giroscopo_X_sum/5;
							acelerometro_Y_media+=acelerometro_Y_sum/5;
							acelerometro_Z_media+=acelerometro_Z_sum/5;
							
							giroscopo_X_sum=0;
							giroscopo_Y_sum=0;
							giroscopo_Z_sum=0;
							acelerometro_X_sum=0;
							acelerometro_Y_sum=0;
							acelerometro_Z_sum=0;
							
							n_medias++;
							
						}
						
						////Guardamos la medida que acabamos de realizar como la medida del paso anterior para aplicarlo en el filtro
						
						giroscopo_offset_X_1 =giroscopo_X_filt;
						giroscopo_offset_Y_1 =giroscopo_Y_filt;
						giroscopo_offset_Z_1 =giroscopo_Z_filt;
						acelerometro_offset_X_1 =acelerometro_X_filt;
						acelerometro_offset_Y_1 =acelerometro_Y_filt;
						acelerometro_offset_Z_1 =acelerometro_Z_filt;
						contador++;
						
						////Calculamos el resultado final haciendo la media de las medias
				
						giroscopo_offset_X_final=giroscopo_X_media/n_medias;
						giroscopo_offset_Y_final=giroscopo_Y_media/n_medias;
						giroscopo_offset_Z_final=giroscopo_Z_media/n_medias;
						acelerometro_offset_X_final=acelerometro_X_media/n_medias;
						acelerometro_offset_Y_final=acelerometro_Y_media/n_medias;
						acelerometro_offset_Z_final=acelerometro_Z_media/n_medias;	
						
					}
						
						////EMPEZAMOS A SENSAR Y ACTUAR
						
							for(int i=0;i<n;i++){
								
								////LEER DATOS G/A (EJES, DIRECCIONES H/L) Y CONCATENAR.
								
								////leer los 16 bits del sensor
								
								giroscopo_data_X[1] = wiringPiI2CReadReg8(fd,0x22); // OUTX_L_G
								giroscopo_data_X[0] = wiringPiI2CReadReg8(fd,0x23); // OUTX_H_G
								giroscopo_data_Y[1] = wiringPiI2CReadReg8(fd,0x24); // OUTY_L_G
								giroscopo_data_Y[0] = wiringPiI2CReadReg8(fd,0x25); // OUTY_H_G
								giroscopo_data_Z[1] = wiringPiI2CReadReg8(fd,0x26); // OUTZ_L_G
								giroscopo_data_Z[0] = wiringPiI2CReadReg8(fd,0x27); // OUTZ_H_G
								acelerometro_data_X[1] = wiringPiI2CReadReg8(fd,0x28); // OUTX_L_XL
								acelerometro_data_X[0] = wiringPiI2CReadReg8(fd,0x29); // OUTX_H_XL
								acelerometro_data_Y[1] = wiringPiI2CReadReg8(fd,0x2A); // OUTY_L_XL
								acelerometro_data_Y[0] = wiringPiI2CReadReg8(fd,0x2B); // OUTY_H_XL
								acelerometro_data_Z[1] = wiringPiI2CReadReg8(fd,0x2C); // OUTZ_L_XL
								acelerometro_data_Z[0] = wiringPiI2CReadReg8(fd,0x2D); // OUTZ_H_XL
								
								//// concatenar los 2 bytes, y procesar de acuerdo al datasheet
								
								giroscopo_n_X =(giroscopo_data_X[0]<<8)|(giroscopo_data_X[1]);
								giroscopo_n_Y =(giroscopo_data_Y[0]<<8)|(giroscopo_data_Y[1]);
								giroscopo_n_Z =(giroscopo_data_Z[0]<<8)|(giroscopo_data_Z[1]);
								acelerometro_n_X =(acelerometro_data_X[0]<<8)|(acelerometro_data_X[1]);
								acelerometro_n_Y =(acelerometro_data_Y[0]<<8)|(acelerometro_data_Y[1]);
								acelerometro_n_Z =(acelerometro_data_Z[0]<<8)|(acelerometro_data_Z[1]);
									
								////Tratamos los datos según el datasheet	
											
								giroscopo_n_X=giroscopo_n_X*8.75/1000;
								giroscopo_n_Y=giroscopo_n_Y*8.75/1000;
								giroscopo_n_Z=giroscopo_n_Z*8.75/1000;
								acelerometro_n_X=acelerometro_n_X*0.061*9.8/1000;
								acelerometro_n_Y=acelerometro_n_Y*0.061*9.8/1000;
								acelerometro_n_Z=acelerometro_n_Z*0.061*9.8/1000;
								
								////Aplicamos el offset
								
								giroscopo_n_X=(giroscopo_n_X-giroscopo_offset_X_final);
								giroscopo_n_Y=(giroscopo_n_Y-giroscopo_offset_Y_final);
								giroscopo_n_Z=(giroscopo_n_Z-giroscopo_offset_Z_final);
								acelerometro_n_X=(acelerometro_n_X-acelerometro_offset_X_final);
								acelerometro_n_Y=(acelerometro_n_Y-acelerometro_offset_Y_final);
								acelerometro_n_Z=(acelerometro_n_Z-acelerometro_offset_Z_final);
								
								////Guardamos la variable final
							
								Ax=acelerometro_n_X;
								Ay=acelerometro_n_Y;
								Az=acelerometro_n_Z; 
								
							
								//// APLICAR FORMULAS PARA OBTENER ANGULOS (CABECEO, ALABEO,GIRO).
								
								pitch= 180/pi*atan((Ax)/(sqrt(pow(Ay,2)+pow(Az,2))));
								roll= 180/pi*atan((Ay)/(sqrt(pow(Ax,2)+pow(Az,2))));
								yaw= 180/pi*atan((Az)/(sqrt(pow(Ax,2)+pow(Ay,2))));							
											
								//// SE CODIFICA LA POSICIÓN DEL ANGULO EN EL SERVO (POR EJEMPLO ALABEO. SE ASIGNA VALOR Y RANGO (VER TRANSPARENCIAS/GUIÓN).
								////Definimos variables para el control del servo
	
								int rango=2400;			
								int divisor=160; 			
								int value;
								value=pitch+180;

								////Procedemos a la configuración del servo
								
								pinMode(SERVO,PWM_OUTPUT); //// PWM OUTPUT
								pwmSetMode(PWM_MODE_MS); //// MODO PWM
								pwmSetRange(rango); 
								pwmSetClock(divisor);
								pwmWrite(SERVO, value);
								
								//// HEURÍSTICA DE:
								
								 ///1) LECTURA TEMPERATURA.
								 
								 // leer los 16 bits del sensor
								 data_read[0] = wiringPiI2CReadReg8(fd,0x21); // OUT_TEMP_H
								 data_read[1] = wiringPiI2CReadReg8(fd,0x20); // OUT_TEMP_L
								 
								 // concatenar los 2 bytes, y procesar de acuerdo al datasheet
								 
								 delayMicroseconds(500);
								 
								 raw_data =(data_read[0]<<8)|(data_read[1]) ;

								 temp2 = (raw_data/16+25 ); //// Realizar correciones de lectura de  sensor T LSB/ºC
								 
								 diftemp=temp2-temp1;
								 
								 ////Definimos un vector de temperaturas para controlar su variación			 

								 ///2) BOTONES ASOCIADOS.
								 
								 ////En la primera iteración tenemos que encender los LEDs
								 
								 if(i==0){					
									 digitalWrite(LED1,HIGH);
									 digitalWrite(LED2,HIGH);
									 
								 }
								 
								 ////Si la temperatura baja un grado apagamos un LED
								 
								 else if(diftemp>=1 && diftemp<2){
														
									digitalWrite(LED2,LOW);						
								}
								
								////Si la temperatura sigue bajando apagamos otro LED
								
								else if(diftemp>2){
									
									digitalWrite(LED1,LOW);						
								}
								
								////Si la temperatura sube más de un grado hacemos parpadear un LED cada 300 iteraciones
								
								else if(diftemp<=-1 && i%300==0){
									
									digitalWrite(LED1,LOW);
									
								}
								else if(diftemp<=-1 && i%600==0){
									
									digitalWrite(LED1,HIGH);
									
								}
									
 
								 printf("-------------------------\nTemperatura inicial=%f		 Temperatura actual=%f\n Variación de la temperatura en el dispositivo=%f\n\n\n",temp1,temp2,diftemp);
								 
								 ///3) MODO REINICIO (COMO SI FUERAN LAS PRÁCTICAS ANTERIORES).
								 
								 
								 ////si apretamos el botón 1 recalibramos y reiniciamos el programa
								 
								 if(digitalRead(SWITCH1)==LOW){
									 						 
									t1=millis();
									contador=0;			
									 break;
								 }

								////si apretamos el botón 2 apagamos el sistema

								 if(digitalRead(SWITCH2)==LOW){
									 
									digitalWrite(LED1,LOW);	
									digitalWrite(LED2,LOW);
									digitalWrite(LED3,LOW);	
									break;
									
								}	
								
								//// CONTROL DE RETARDO SEGÚN FRECUENCIA DE MUESTREO (CUIDADO QUE HAY DOS FRECUENCIAS DIFERENTES (TEMPERATURA Y A/G).
								delay(T);
							 }
							 
							if(digitalRead(SWITCH2)==LOW){
								
	 
								break;
									
							} 
					}
}



int main(){
	int fd;
	wiringPiSetup(); //// Setup WiringPi
	fd = wiringPiI2CSetup(LSM6DS33);//// id del device
	// Configurar el acelerómetro
	int data_config = 0b01010001; //// valores de configuración
	wiringPiI2CWriteReg8(fd, 0x10, data_config);
	// Configurar el giróscopo
	data_config = 0b0101000; //// valores de configuración
	wiringPiI2CWriteReg8(fd, 0x11, data_config);
	////Leer ángulo
	leo_angulo (fd);

	return 0;
}
