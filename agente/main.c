#include "agente.h"

/*************************************************************************************************************
*            Pontificia Universidad Javeriana                                                                *
*                                                                                                            *
* Autor:     <TU NOMBRE>                                                                                     *
* Fecha:     16 de noviembre de 2025                                                                         *
* Materia:   Sistemas Operativos                                                                             *
* Profesor:  John Corredor Franco                                                                            *
* Objetivo:  Proceso CLIENTE/AGENTE que se comunica mediante FIFO con el                                    *
*            Controlador de Reserva del parque, enviando solicitudes leídas desde un archivo CSV.           *
*                                                                                                            *
**************************************************************************************************************
*                                                                                                            *
* HOW TO USE                                                                                                 *
*                                                                                                            *
* HOW TO COMPILE TO PRODUCE EXECUTABLE PROGRAM:                                                              *
*   Linux/macOS:          gcc agente.c agente_main.c -o agente                                               *
*                                                                                                            *
* HOW TO RUN THE PROGRAM:                                                                                    *
*   Linux/macOS:          ./agente -s nombreAgente -a archivo.csv -p /tmp/fifo_controlador                  *
*                                                                                                            *
* NOTAS DE USO:                                                                                              *
*   - El proceso CONTROLADOR debe estar ejecutándose y haber creado el FIFO de entrada indicado en -p.      *
*   - Cada agente crea su propio FIFO de respuesta en /tmp/resp_<nombreAgente>.                             *
*   - El controlador envía al registrarse la hora actual de simulación.                                     *
*   - El agente lee solicitudes del CSV y las envía si la hora >= hora_actual de simulación.                *
*************************************************************************************************************/

/************************************************************************************************************/
/*  int main(int argc, char *argv[])                                                                        */
/*                                                                                                          */
/*  Propósito:                                                                                               */
/*      - Parsear parámetros de línea de comandos (-s, -a, -p).                                             */
/*      - Crear FIFO de respuesta propio del agente.                                                        */
/*      - Registrarse ante el Controlador y leer la hora actual de simulación.                             */
/*      - Leer solicitudes desde un archivo CSV y enviarlas al Controlador.                                */
/*      - Leer la respuesta por el FIFO de respuesta y mostrarla por pantalla.                             */
/************************************************************************************************************/
int main(int argc, char *argv[])
{
    char nombre[64]      = "";
    char archivo[128]    = "";
    char pipe_srv[128]   = "";
    char pipe_resp[128];      /* FIFO de respuesta: /tmp/resp_<nombre> */

    /* --------------------- PARSEO DE ARGUMENTOS --------------------- */
    int opt;
    while ((opt = getopt(argc, argv, "s:a:p:")) != -1) {
        switch (opt) {
        case 's':
            strcpy(nombre, optarg);
            break;
        case 'a':
            strcpy(archivo, optarg);
            break;
        case 'p':
            strcpy(pipe_srv, optarg);
            break;
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
        unlink(pipe_resp);
        exit(1);
    }

    /* ---- Leer hora enviada por el controlador (una vez) ---- */
    int fd_resp = open(pipe_resp, O_RDONLY);
    if (fd_resp < 0) {
        perror("open pipe respuesta");
        unlink(pipe_resp);
        exit(1);
    }

    char buffer[MAXLINE];
    int  hora_actual = 0;
    ssize_t read_bytes;

    read_bytes = read(fd_resp, buffer, sizeof(buffer) - 1);
    if (read_bytes > 0) {
        buffer[read_bytes] = '\0';   /* patrón cliente FIFO: usar read_bytes y terminar en '\0' */
        hora_actual = atoi(buffer);
        printf("Agente %s registrado. Hora actual = %d\n", nombre, hora_actual);
    } else if (read_bytes < 0) {
        perror("read hora_actual");
        close(fd_resp);
        unlink(pipe_resp);
        exit(1);
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
    int  hora, personas;

    while (fgets(linea, sizeof(linea), fp)) {

        if (sscanf(linea, "%[^,],%d,%d", familia, &hora, &personas) != 3) {
            continue;
        }

        /* ---- Ignora solicitudes en horas ya pasadas ---- */
        if (hora < hora_actual) {
            printf("Agente %s IGNORA solicitud (%s %d) porque hora < hora_sim (%d)\n",
                   nombre, familia, hora, hora_actual);
            continue;
        }

        /* ---- Enviar solicitud al Controlador ---- */
        enviar_solicitud(familia, personas, hora, pipe_srv, pipe_resp);

        /* ---- Esperar respuesta en el FIFO de respuesta ---- */
        fd_resp = open(pipe_resp, O_RDONLY);
        if (fd_resp < 0) {
            perror("open pipe respuesta");
            break;
        }

        read_bytes = read(fd_resp, buffer, sizeof(buffer) - 1);
        if (read_bytes > 0) {
            buffer[read_bytes] = '\0';   /* sin memset, mismo patrón que el cliente de referencia */
            printf("Agente %s recibió respuesta: %s\n", nombre, buffer);
        } else if (read_bytes < 0) {
            perror("read respuesta");
        }

        close(fd_resp);

        /* ---- Pausa de 2 segundos entre solicitudes ---- */
        sleep(2);
    }

    /* ------------------ TERMINAR ------------------ */
    printf("Agente %s termina.\n", nombre);

    fclose(fp);
    unlink(pipe_resp);

    return 0;
}

