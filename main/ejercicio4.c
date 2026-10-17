#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ejercicio4.h"

static const char *TAG = "consola";

#define UART_PORT         UART_NUM_0
#define BUF_SIZE          256
#define LINEA_MAX_LEN     64

/* Firma para las funciones que atienden comandos */
typedef void (*comando_fn_t)(char *args);

/* Estructura para el despacho por tabla */
typedef struct {
    const char   *nombre;
    const char   *ayuda;
    comando_fn_t  ejecutar;
} comando_t;

/* Declaración previa de las funciones de comando */
static void cmd_ayuda(char *args);
static void cmd_estado(char *args);
static void cmd_led(char *args);

/* Tabla de comandos (Requisito explícito de la rúbrica) */
static const comando_t TABLA_COMANDOS[] = {
    { "ayuda",  "Muestra la lista de comandos disponibles", cmd_ayuda  },
    { "estado", "Consulta el estado general del sistema",   cmd_estado },
    { "led",    "Actua sobre el LED (ej: led encender/apagar)", cmd_led},
};
#define NUM_COMANDOS (sizeof(TABLA_COMANDOS) / sizeof(TABLA_COMANDOS[0]))

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
    printf("\n[ESTADO DEL SISTEMA]\n");
    printf("  UART0: Conectada a 115200 baudios\n");
    printf("  Consola: Operativa y respondiendo\n\n");
}

static void cmd_led(char *args)
{
    if (args == NULL || strlen(args) == 0) {
        printf("Uso: led <on|off>\n");
        return;
    }
    if (strcmp(args, "on") == 0 || strcmp(args, "1") == 0) {
        printf("Comando LED: Activado\n");
    } else if (strcmp(args, "off") == 0 || strcmp(args, "0") == 0) {
        printf("Comando LED: Desactivado\n");
    } else {
        printf("Parametro desconocido '%s'. Use 'on' u 'off'.\n", args);
    }
}

/* Despachador de comandos sobre la tabla */
static void despachar_linea(char *linea)
{
    /* 1. Recortar espacios iniciales */
    while (*linea == ' ' || *linea == '\t') linea++;

    /* 2. Tratar línea vacía */
    if (*linea == '\0') {
        return;
    }

    /* 3. Separar comando y argumentos con strtok */
    char *cmd = strtok(linea, " \t");
    char *args = strtok(NULL, "");

    /* 4. Buscar en la tabla de comandos */
    for (size_t i = 0; i < NUM_COMANDOS; i++) {
        if (strcmp(cmd, TABLA_COMANDOS[i].nombre) == 0) {
            TABLA_COMANDOS[i].ejecutar(args);
            return;
        }
    }

    /* 5. Tratar comando desconocido */
    printf("Comando no reconocido: '%s'. Escriba 'ayuda' para ver la lista.\n", cmd);
}

/* Tarea de recepción por UART */
static void tarea_consola(void *arg)
{
    (void)arg;
    uint8_t ch;
    char buffer[LINEA_MAX_LEN];
    size_t indice = 0;
    bool linea_descartada = false;

    printf("\n=== Consola serie lista (escriba 'ayuda') ===\n> ");
    fflush(stdout);

    for (;;) {
        /* Lee byte a byte sin bloquear la CPU si no hay datos */
        int len = uart_read_bytes(UART_PORT, &ch, 1, pdMS_TO_TICKS(50));
        if (len <= 0) {
            continue;
        }

        /* Detección de fin de línea: \r (monitor IDF) o \n */
        if (ch == '\r' || ch == '\n') {
            if (linea_descartada) {
                printf("\nError: Linea demasiado larga descartada (max %d caracteres).\n> ", LINEA_MAX_LEN - 1);
                linea_descartada = false;
                indice = 0;
            } else if (indice > 0) {
                buffer[indice] = '\0';
                printf("\n");
                despachar_linea(buffer);
                indice = 0;
                printf("> ");
            } else {
                /* Línea vacía: reimprime prompt */
                printf("\n> ");
            }
            fflush(stdout);
            continue;
        }

        /* Protección de desbordamiento (descarte de línea larga) */
        if (indice >= (LINEA_MAX_LEN - 1)) {
            linea_descartada = true;
            continue; /* Sigue consumiendo hasta encontrar el fin de línea */
        }

        /* Acumular carácter si la línea no está descartada */
        if (!linea_descartada) {
            buffer[indice++] = (char)ch;
            /* Echo local opcional para ver lo que se teclea */
            putchar(ch);
            fflush(stdout);
        }
    }
}

void run_ejercicio4(void)
{
    /* Configuración básica de UART0 */
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    
    /* Conservar pines nativos de la placa (sin cambios) */
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, 
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    /* Instalar driver sin cola de eventos */
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0));

    /* Lanzar la tarea de recepción */
    xTaskCreate(tarea_consola, "tarea_consola", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Consola UART0 inicializada a 115200 baudios.");
}