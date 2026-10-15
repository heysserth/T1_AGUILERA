#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "led.h"
#include "pines.h"
#include "ejercicio1.h"

static const char *TAG = "main";

#define CONFIG_SEYTR_PERIODO_MS 1000

static led_t s_led;

static void tarea_led(void *argumento)
{
    (void)argumento;

    TickType_t referencia = xTaskGetTickCount();
    const TickType_t intervalo = pdMS_TO_TICKS(CONFIG_SEYTR_PERIODO_MS / 2);

    int64_t tiempo_inicio = esp_timer_get_time();

    for (;;) {
        led_conmutar(&s_led);

        int64_t tiempo_us = esp_timer_get_time();
        int64_t resta = (tiempo_us - tiempo_inicio) / 1000;

        xTaskDelayUntil(&referencia, intervalo);

        ESP_LOGI(TAG, "Diferencia=%lld ms, LED=%s",
                 (long long)resta,
                 s_led.encendido ? "encendido" : "apagado");

        tiempo_inicio = tiempo_us;
    }
}

void run_ejercicio1(void)
{
    ESP_ERROR_CHECK(led_iniciar(&s_led, PIN_LED, LED_NIVEL_ENCENDIDO));

    ESP_LOGI(TAG, "TALLER #1 6.1 P1A · Salida digital y parpadeo estable");
    ESP_LOGI(TAG, "#0 encendido t=0.000 ms, origen de la medida");

    if (xTaskCreate(tarea_led, "led", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE("led", "No se pudo crear la tarea");
    }
}
