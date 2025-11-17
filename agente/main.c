------------------------------------------------------------------------- */
/*                           Funcion principal del agente                      */
/* -------------------------------------------------------------------------- */
#include "agente.h"

int main(int argc, char *argv[])
{
    char nombre[64] = "";
    char archivo[128] = "";
    char pipe_srv[128] = "";
    char pipe_resp[128];

    /* --------------------- PARSEO DE ARGUMENTOS --------------------- */
    int opt;
    while ((opt = getopt(argc, argv, "s:a:p:")) != -1) {
        switch (opt) {
        case 's': strcpy(nombre, optarg); break;
        case 'a': strcpy(archivo, optarg); break;
        case 'p': strcpy(pipe_srv, optarg); break;
        default:
            fprintf(stderr, "Uso: %s -s nombre -a archivo -p pipeSrv\n", argv[0]);
            exit(1);
        }
    }

    if (nombre[0] == '\0' || archivo[0] == '\0' || pipe_srv[0] == '\0') {
        fprintf(stderr, "Faltan parámetros. Uso: %s -s nombre -a archivo -p pipeSrv\n", argv[0]);
        exit(1);
    }

    /* ------------------ CREAR PIPE DE RESPUESTA ------------------ */
    snprintf(pipe_resp, sizeof(pipe_resp), "/tmp/resp_%s", nombre);
    mkfifo(pipe_resp, 0666);

    /* ------------------ REGISTRO CON EL CONTROLADOR ------------------ */
    if (registrar_agente(nombre, pipe_srv, pipe_resp) < 0) {
        fprintf(stderr, "No se pudo registrar el agente.\n");
        exit(1);
    }

    /* Leer hora enviada por el controlador */
    int fd_resp = open(pipe_resp, O_RDONLY);
    if (fd_resp < 0) {
        perror("open pipe respuesta");
        exit(1);
    }

    char buffer[MAXLINE];
    int hora_actual = 0;

    if (read(fd_resp, buffer, sizeof(buffer)) > 0) {
        hora_actual = atoi(buffer);
        printf("Agente %s registrado. Hora actual = %d\n", nombre, hora_actual);
    }
    close(fd_resp);

    /* ------------------ ABRIR ARCHIVO CSV ------------------ */
    FILE *fp = fopen(archivo, "r");
    if (!fp) {
        perror("fopen archivo solicitudes");
        unlink(pipe_resp);
        exit(1);
    }

    /* ------------------ BUCLE PRINCIPAL ------------------ */
    char linea[MAXLINE];
    char familia[64];
    int hora, personas;

    while (fgets(linea, sizeof(linea), fp)) {

        if (sscanf(linea, "%[^,],%d,%d", familia, &hora, &personas) != 3) {
            continue;
        }

        if (hora < hora_actual) {
            printf("Agente %s IGNORA solicitud (%s %d) porque hora < hora_sim (%d)\n",
                   nombre, familia, hora, hora_actual);
            continue;
        }

        enviar_solicitud(familia, personas, hora, pipe_srv, pipe_resp);

        fd_resp = open(pipe_resp, O_RDONLY);
        if (fd_resp < 0) {
            perror("open pipe respuesta");
            break;
        }

        memset(buffer, 0, sizeof(buffer));
        read(fd_resp, buffer, sizeof(buffer));
        close(fd_resp);

        printf("Agente %s recibió respuesta: %s\n", nombre, buffer);

        sleep(2);
    }

    /* ------------------ TERMINAR ------------------ */
    printf("Agente %s termina.\n", nombre);

    fclose(fp);
    unlink(pipe_resp);

    return 0;
}
