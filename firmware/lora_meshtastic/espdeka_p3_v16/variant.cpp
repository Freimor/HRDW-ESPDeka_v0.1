#include "configuration.h"

#ifdef ESPDEKA_P3_V16

#include <Arduino.h>

#include "driver/gpio.h"

#include "variant.h"

/**
 * LR1121 GPIO prep before SPI.begin (coprocessor v0.2.3 lessons).
 */
void earlyInitVariant()
{
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);

    pinMode(LORA_BUSY, INPUT_PULLUP);
    pinMode(LORA_DIO1, INPUT_PULLUP);
    pinMode(LORA_MISO, INPUT_PULLUP);

    (void)gpio_set_pull_mode((gpio_num_t)LORA_MISO, GPIO_PULLUP_ONLY);
    (void)gpio_set_pull_mode((gpio_num_t)LORA_BUSY, GPIO_PULLUP_ONLY);
}

#endif
