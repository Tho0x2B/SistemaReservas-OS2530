/*agente robusto para handshake y parsing de respuestas */
#include "agente.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

/*************************************************************************************************************
*            Pontificia Universidad Javeriana                                                                *
*                                                                                                            *
* Autor:     Thomas Leal Puerta                                                                              *
* Fecha:     16 de noviembre de 2025                                                                         *
* Materia:   Sistemas Operativos                                                                             *
* Profesor:  John Corredor Franco                                                                            *
* Objetivo:  Proceso CLIENTE/AGENTE que se comunica mediante FIFO con el                                     *
*            Controlador de Reserva del parque, enviando solicitudes leídas desde un archivo CSV.            *
*                                                                                                            *
**************************************************************************************************************
*                                                                                                            *
* HOW TO USE                                                                                                 *
*                                                                                                            *
* HOW TO COMPILE TO PRODUCE EXECUTABLE PROGRAM:                                                              *
*   Linux/macOS:          gcc agente.c agente_main.c -o agente                                               *
*                                                                                                            *
* HOW TO RUN THE PROGRAM:                                                                                    *
*   Linux/macOS:          ./agente -s nombreAgente -a archivo.csv -p /tmp/fifo_controlador                   *
*                                                                                                            *
* NOTAS DE USO:                                                                                              *
*   - El proceso CONTROLADOR debe estar ejecutándose y haber creado el FIFO de entrada indicado en -p.       *
*   - Cada agente crea su propio FIFO de respuesta en /tmp/resp_<nombreAgente>.                              *
*   - El controlador envía al registrarse la hora actual de simulación.                                      *
*   - El agente lee solicitudes del CSV y las envía si la hora >= hora_actual de simulación.                 *
**************************************************************************************************************/

/************************************************************************************************************
 *  int main(int argc, char *argv[])                                                                        *
 *                                                                                                          *
 *  Propósito:                                                                                              *
 *      - Parsear parámetros de línea de comandos (-s, -a, -p).                                             *
 *      - Crear FIFO de respuesta propio del agente.                                                        *
 *      - Registrarse ante el Controlador y leer la hora actual de simulación.                              *
 *      - Leer solicitudes desde un archivo CSV y enviarlas al Controlador.                                 *
 *      - Leer la respuesta por el FIFO de respuesta y mostrarla por pantalla.                              *
************************************************************************************************************/
/* función auxiliar para quitar O_NONBLOCK */
static int set_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    flags &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

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
            strncpy(nombre, optarg, sizeof(nombre)-1);
            nombre[sizeof(nombre)-1] = '\0';
            break;
        case 'a':
            strncpy(archivo, optarg, sizeof(archivo)-1);
            archivo[sizeof(archivo)-1] = '\0';
            break;
        case 'p':
            strncpy(pipe_srv, optarg, sizeof(pipe_srv)-1);
            pipe_srv[sizeof(pipe_srv)-1] = '\0';
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

    /* ------------------ CREAR Y ABRIR PIPE DE RESPUESTA (NO BLOCK) ------------------ */
    snprintf(pipe_resp, sizeof(pipe_resp), "/tmp/resp_%s", nombre);

    if (mkfifo(pipe_resp, 0666) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo pipe_resp");
            exit(1);
        }
    }

    /* Abrir en lectura NO bloqueante *antes* de enviar el registro */
    int fd_resp = open(pipe_resp, O_RDONLY | O_NONBLOCK);
    if (fd_resp < 0) {
        perror("open pipe_resp (O_RDONLY | O_NONBLOCK)");
        unlink(pipe_resp);
        exit(1);
    }

    /* ------------------ REGISTRO CON EL CONTROLADOR ------------------ */
    if (registrar_agente(nombre, pipe_srv, pipe_resp) < 0) {
        fprintf(stderr, "No se pudo registrar el agente.\n");
        close(fd_resp);
        unlink(pipe_resp);
        exit(1);
    }

    /* ---- Ahora que hemos enviado el registro, convertimos fd_resp a bloqueante
       para que read() espere la respuesta del servidor de forma natural. ---- */
    if (set_blocking(fd_resp) == -1) {
        perror("set_blocking(fd_resp)");
        /* no fatal, pero recomendable */
    }

    /* ---- Leer hora enviada por el controlador (una vez) ---- */
    char buffer[MAXLINE];
    int  hora_actual = 0;
    ssize_t read_bytes;

    /* read() bloqueante: esperará hasta que el servidor escriba */
    read_bytes = read(fd_resp, buffer, sizeof(buffer) - 1);
    if (read_bytes > 0) {
        buffer[read_bytes] = '\0';
        /* el formato esperado: "HORA;N\n" -> tomar número tras ';' */
        char *p = strchr(buffer, ';');
        if (p != NULL) {
            hora_actual = atoi(p + 1);
        } else {
            /* si viene solo número, fallback */
            hora_actual = atoi(buffer);
        }
        printf("Agente %s registrado. Hora actual = %d\n", nombre, hora_actual);
    } else if (read_bytes == 0) {
        fprintf(stderr, "read(fd_resp) devolvio 0 (FIFO cerrado) antes de recibir hora\n");
        close(fd_resp);
        unlink(pipe_resp);
        exit(1);
    } else {
        perror("read hora_actual");
        close(fd_resp);
        unlink(pipe_resp);
        exit(1);
    }

    /* ------------------ ABRIR ARCHIVO CSV ------------------ */
    FILE *fp = fopen(archivo, "r");
    if (!fp) {
        perror("fopen archivo solicitudes");
        close(fd_resp);
        unlink(pipe_resp);
        exit(1);
    }

    /* ------------------ BUCLE PRINCIPAL (reusar fd_resp) ------------------ */
    char linea[MAXLINE];
    char familia[64];
    int  hora, personas;

    while (fgets(linea, sizeof(linea), fp)) {

        if (sscanf(linea, "%63[^,],%d,%d", familia, &hora, &personas) != 3) {
            continue;
        }

        /* ---- Ignora solicitudes en horas ya pasadas ---- */
        if (hora < hora_actual) {
            printf("Agente %s IGNORA solicitud (%s %d) porque hora < hora_sim (%d)\n",
                   nombre, familia, hora, hora_actual);
            continue;
        }

        /* ---- Enviar solicitud al Controlador ---- */
        if (enviar_solicitud(familia, personas, hora, pipe_srv, pipe_resp) < 0) {
            fprintf(stderr, "Error al enviar solicitud para %s\n", familia);
            continue;
        }

        /* ---- Esperar respuesta en el FIFO de respuesta (fd_resp ya abierto y bloqueante) ---- */
        read_bytes = read(fd_resp, buffer, sizeof(buffer) - 1);
        if (read_bytes > 0) {
            buffer[read_bytes] = '\0';
            printf("Agente %s recibió respuesta: %s", nombre, buffer); /* buffer probablemente tiene \n */
            /* Si el mensaje del servidor contiene nueva hora (p.ej. "HORA;N") podrías actualizar hora_actual */
            char *p = strchr(buffer, ';');
            if (p != NULL && strncmp(buffer, "HORA;", 5) == 0) {
                hora_actual = atoi(p + 1);
            }
        } else if (read_bytes == 0) {
            /* El servidor cerró el FIFO: terminar */
            fprintf(stderr, "Agente %s: FIFO de respuesta cerrado por servidor. Saliendo.\n", nombre);
            break;
        } else {
            perror("read respuesta");
        }

        /* ---- Pausa de 2 segundos entre solicitudes ---- */
        sleep(2);
    }

    /* ------------------ TERMINAR ------------------ */
    printf("Agente %s termina.\n", nombre);

    /* limpiar */
    fclose(fp);
    close(fd_resp);
    unlink(pipe_resp);

    return 0;
}
