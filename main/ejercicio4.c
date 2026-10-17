#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "led.h"
#include "pines.h"
#include "ejercicio4.h"

static const char *TAG = "consola";

#define UART_PORT         UART_NUM_0
#define BUF_SIZE          256
#define LINEA_MAX_LEN     64

/* Instancia del LED usando el componente oficial del profesor */
static led_t s_led;

/* Define el formato estandar que debe tener cualquier función de comando.
permitiendo almacenar funciones dentro de la tabla de comandos*/
typedef void (*comando_fn_t)(char *args);

typedef struct {
    const char   *nombre;
    const char   *ayuda;
    comando_fn_t  ejecutar;
} comando_t;

/* Declaración previa de las funciones de comando. Reciben un char y no devuelven nada */
static void cmd_ayuda(char *args);
static void cmd_estado(char *args);
static void cmd_led(char *args);

/* Despacho sobre tabla (Requisito C4) */
static const comando_t TABLA_COMANDOS[] = {
    { "ayuda",  "Muestra esta lista de comandos",                cmd_ayuda  },
    { "estado", "Consulta el estado general del sistema",        cmd_estado },
    { "led",    "Controla el LED (<on|off> o <1|0>)",            cmd_led    },
};
#define NUM_COMANDOS (sizeof(TABLA_COMANDOS) / sizeof(TABLA_COMANDOS[0]))

/* Reutilización de la función en_ms() del profesor para trazas de tiempo */
static const char *en_ms(int64_t us, char *texto, size_t tamano)
{
    if (us < 0) us = 0;
    snprintf(texto, tamano, "%lld.%03lld", 
             (long long)(us / 1000), (long long)(us % 1000));
    return texto;
}

static void cmd_ayuda(char *args)
{
    (void)args;
    printf("\nComandos disponibles:\n");
    for (size_t i = 0; i < NUM_COMANDOS; i++) {
        printf("  %-8s - %s\n", TABLA_COMANDOS[i].nombre, TABLA_COMANDOS[i].ayuda);
    }
    printf("\n");
}

static void cmd_estado(char *args)
{
    (void)args;
    char tiempo_str[24];
    en_ms(esp_timer_get_time(), tiempo_str, sizeof(tiempo_str));

    printf("\n[ESTADO DEL SISTEMA]\n");
    printf("  Tiempo de actividad : %s ms\n", tiempo_str);
    printf("  Puerto consola      : UART0 a 115200 baudios\n");
    printf("  Tick de FreeRTOS    : %d Hz\n\n", configTICK_RATE_HZ);
}

static void cmd_led(char *args)
{
    if (args == NULL || strlen(args) == 0) {
        printf("Uso: led <on|off>\n");
        return;
    }
    if (strcmp(args, "on") == 0 || strcmp(args, "1") == 0) {
        led_encender(&s_led);
        printf("LED encendido en GPIO %d\n", (int)PIN_LED);
    } else if (strcmp(args, "off") == 0 || strcmp(args, "0") == 0) {
        led_apagar(&s_led);
        printf("LED apagado en GPIO %d\n", (int)PIN_LED);
    } else {
        printf("Parametro desconocido '%s'. Use 'on' u 'off'.\n", args);
    }
}

/* Despacho sobre tabla */
static void despachar_linea(char *linea)
{
    while (*linea == ' ' || *linea == '\t') linea++;

    /* Línea vacía: no hace nada y no produce fallos */
    if (*linea == '\0') {
        return;
    }

    char *cmd = strtok(linea, " \t");
    char *args = strtok(NULL, "");

    for (size_t i = 0; i < NUM_COMANDOS; i++) {
        if (strcmp(cmd, TABLA_COMANDOS[i].nombre) == 0) {
            TABLA_COMANDOS[i].ejecutar(args);
            return;
        }
    }

    /* Comando desconocido */
    printf("Comando no reconocido: '%s'. Escriba 'ayuda' para ver los disponibles.\n", cmd);
}

/* Tarea de recepción robusta por UART */
static void tarea_consola(void *arg)
{
    (void)arg;
    uint8_t ch;
    char buffer[LINEA_MAX_LEN];
    size_t indice = 0;
    bool linea_descartada = false;

    printf("\n=== Consola serie por UART0 lista ===\n");
    cmd_ayuda(NULL);
    printf("> ");
    fflush(stdout);

    for (;;) {
        /* Lectura bloqueante por driver UART de ESP-IDF */
        int len = uart_read_bytes(UART_PORT, &ch, 1, pdMS_TO_TICKS(50));
        if (len <= 0) {
            continue;
        }

        /* Fin de línea: \r (monitor de ESP-IDF) o \n */
        if (ch == '\r' || ch == '\n') {
            if (linea_descartada) {
                printf("\nError: linea demasiado larga descartada (max %d caracteres).\n> ", LINEA_MAX_LEN - 1);
                linea_descartada = false;
                indice = 0;
            } else if (indice > 0) {
                buffer[indice] = '\0';
                printf("\n");
                despachar_linea(buffer);
                indice = 0;
                printf("> ");
            } else {
                printf("\n> ");
            }
            fflush(stdout);
            continue;
        }

        /* Descarte automático si supera el límite de longitud */
        if (indice >= (LINEA_MAX_LEN - 1)) {
            linea_descartada = true;
            continue;
        }

        /* Acumular y eco local */
        if (!linea_descartada) {
            buffer[indice++] = (char)ch;
            putchar(ch);
            fflush(stdout);
        }
    }
}

void run_ejercicio4(void)
{
    /* Inicializar el componente del LED */
    ESP_ERROR_CHECK(led_iniciar(&s_led, PIN_LED, LED_NIVEL_ENCENDIDO));

    /* 1. Configuración de parámetros de la UART0 */
    const uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));

    /* 2. Mantener pines nativos de la placa (según paso 1 de la práctica) */
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    /* 3. Instalar el driver de UART en el sistema */
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0));

    /* 4. Lanzar la tarea de recepción */
    const BaseType_t res = xTaskCreate(tarea_consola, "consola", 4096, NULL, 5, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "No hay memoria para crear la tarea de consola");
        return;
    }

    ESP_LOGI(TAG, "Consola UART0 inicializada a 115200 baudios.");
}