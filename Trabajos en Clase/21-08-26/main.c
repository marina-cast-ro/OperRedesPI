#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>

#define CHIP_PATH "/dev/gpiochip4" // En Pi 5 es gpiochip4. En Pi 4 o anteriores usa "/dev/gpiochip0"
#define GPIO_LINE 17               // GPIO 17 (Pin físico 11)

int main(void) {
	FILE *archivo = fopen("registro.txt", "w");
    struct gpiod_chip *chip;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    struct gpiod_request_config *req_cfg;
    struct gpiod_line_request *request;
    enum gpiod_line_value val;
    unsigned int offset = GPIO_LINE;

    // 1. Abrir el chip GPIO
    chip = gpiod_chip_open(CHIP_PATH);
    if (!chip) {
        // Fallback para Pi 4 o versiones anteriores
        chip = gpiod_chip_open("/dev/gpiochip0");
        if (!chip) {
            perror("Error al abrir chip GPIO");
            return 1;
        }
    }

    // 2. Configurar la línea como entrada
    settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);

    line_cfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings);

    // 3. Configurar la petición
    req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "KY-038-v2");

    // 4. Solicitar la línea
    request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

    // Liberar estructuras intermedias que ya no se ocupan
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);

    if (!request) {
        perror("Error al solicitar linea GPIO");
        gpiod_chip_close(chip);
        return 1;
    }

    printf("Escuchando el sensor KY-038 con libgpiod v2 (Ctrl+C para salir)...\n");

    // 5. Bucle de lectura
    while (1) {
        val = gpiod_line_request_get_value(request, GPIO_LINE);

        if (val == GPIOD_LINE_VALUE_ACTIVE) {
            printf("¡Sonido detectado! (1)\n");
			fprintf(archivo, "Sonido detectado! (1).\n");
        } else {
			printf("¡Sonido no detectado! (0)\n");
			fprintf(archivo, "Sonido no detectado! (0).\n");
		}

        usleep(100000); // 100 ms
    }

    // Limpieza al salir
    gpiod_line_request_release(request);
    gpiod_chip_close(chip);
	fclose(archivo);
    return 0;
}