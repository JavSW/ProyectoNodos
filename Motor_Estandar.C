#include <stdio.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <math.h>
#include <unistd.h>

#define SWITCH1 25
#define SWITCH2 29
#define PWM_PIN 1

int main() {
    wiringPiSetup();
    pinMode(PWM_PIN, PWM_OUTPUT);
    pwmSetMode(PWM_MODE_MS);
    pwmSetClock(192);
    pwmSetRange(2250);
    int value = 140;
    pwmWrite(PWM_PIN, value);

    pinMode(SWITCH1, INPUT);
    pullUpDnControl(SWITCH1, PUD_UP);
    pinMode(SWITCH2, INPUT);
    pullUpDnControl(SWITCH2, PUD_UP);

    while (1) {
        if (digitalRead(SWITCH1) == LOW) {
            value = round(9 * (1.05) + value);
            pwmWrite(PWM_PIN, value);
            printf("Valor actual: %d\n", value);
            delay(150);
        } else if (digitalRead(SWITCH2) == LOW) {
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

    return 0;
}
