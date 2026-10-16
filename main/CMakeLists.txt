idf_component_register(SRCS "Ejercicio2.c" "filtro.c" "main.c" "ejercicio1.c" "ejercicio3.c"
                    INCLUDE_DIRS "."
                    REQUIRES led esp_timer pulsador esp_driver_i2c bme280)