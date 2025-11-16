/************************************************************************************************************
 *                               Pontificia Universidad Javeriana                                           *
 *                                   Facultad de Ingenieria                                                 *
 *                               Departamento de Ingenieria de Sistemas                                     *
 *                                                                                                          *
 *  Autor:      Thomas Leal Puerta                                                                          *
 *  Fecha:      16 de noviembre de 2025                                                                     *
 *  Materia:    Sistemas Operativos                                                                         *
 *  Profesor:   John Jairo Corredor, PhD                                                                    *
 *  Fichero:    controlador.c                                                                               *
 *                                                                                                          *
 *  Objetivo:   Implementar las funciones del servidor "Controlador de Reserva". En esta etapa se define    *
 *              la rutina de inicializacion, que configura el reloj de simulacion, las estructuras de datos *
 *              y los dos hilos principales: uno para la señal de reloj y otro para la atencion de agentes. *
 ************************************************************************************************************/

/***************************************** Headers **********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>    /* mkfifo */
#include <fcntl.h>       /* open   */
#include <unistd.h>      /* close, sleep */

#include "controlador.h"

int servidor_inicializar(controlador_t *ctrl)
{
    int h, i;

    /* ---- Validacion de puntero ---- */
    if (ctrl == NULL) {
        fprintf(stderr, "Error: controlador nulo en servidor_inicializar().\n");
        return -1;
    }

    /* ---- Inicializar hora actual y bandera de simulacion ---- */
    ctrl->hora_actual       = ctrl->hora_ini;
    ctrl->simulacion_activa = 1;

    /* ---- Inicializar estadisticas globales ---- */
    ctrl->solicitudes_negadas       = 0;
    ctrl->solicitudes_ok            = 0;
    ctrl->solicitudes_reprogramadas = 0;

    /* ---- Inicializar estructuras por hora ---- */
    for (h = 0; h <= MAX_HORAS_DIA; h++) {
        ctrl->horas[h].hora             = h;
        ctrl->horas[h].aforo_maximo     = ctrl->aforo_maximo;
        ctrl->horas[h].ocupacion_actual = 0;
        ctrl->horas[h].num_reservas     = 0;

        for (i = 0; i < MAX_RESERVAS_POR_HORA; i++) {
            ctrl->horas[h].reservas[i].nombre_familia[0] = '\0';
            ctrl->horas[h].reservas[i].hora_inicio       = 0;
            ctrl->horas[h].reservas[i].hora_fin          = 0;
            ctrl->horas[h].reservas[i].num_personas      = 0;
        }
    }

    /* ---- Crear el FIFO nominal ---- */
    if (mkfifo(ctrl->pipe_entrada, 0666) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo (pipe de entrada del servidor)");
            return -1;
        }
    }

    /* ---- Abrir el FIFO para lectura/escritura ---- */
    ctrl->fifo_fd = open(ctrl->pipe_entrada, O_RDWR);
    if (ctrl->fifo_fd == -1) {
        perror("open (pipe de entrada del servidor)");
        return -1;
    }

    /* ---- Crear hilo del reloj de simulacion ---- */
    if (pthread_create(&(ctrl->hilo_reloj), NULL, servidor_hilo_reloj, (void *) ctrl) != 0) {
        perror("pthread_create (hilo_reloj)");
        close(ctrl->fifo_fd);
        ctrl->fifo_fd = -1;
        return -1;
    }

    /* ---- Crear hilo para atencion de agentes (lectura del FIFO) ---- */
    if (pthread_create(&(ctrl->hilo_agentes), NULL, servidor_hilo_agentes, (void *) ctrl) != 0) {
        perror("pthread_create (hilo_agentes)");
        /* Si falla este hilo, cancelamos el de reloj y limpiamos. */
        ctrl->simulacion_activa = 0;
        pthread_cancel(hilo_reloj);
        pthread_join(hilo_reloj, NULL);
        close(ctrl->fifo_fd);
        ctrl->fifo_fd = -1;
        return -1;
    }

    return 0;
}

void servidor_destruir(controlador_t *ctrl)
{
    if (ctrl == NULL) return;

    /* ---- Marcar fin de simulacion ---- */
    ctrl->simulacion_activa = 0;

    /* ---- Esperar a que terminen los hilos (si fueron creados) ---- */
    pthread_join(ctrl->hilo_reloj,   NULL);
    pthread_join(ctrl->hilo_agentes, NULL);

    /* ---- Cerrar FIFO ---- */
    if (ctrl->fifo_fd != -1) {
        close(ctrl->fifo_fd);
        ctrl->fifo_fd = -1;
    }

    /* ---- Eliminar archivo FIFO ---- */
    if (ctrl->pipe_entrada[0] != '\0') {
        unlink(ctrl->pipe_entrada);
    }
}

void *servidor_hilo_reloj(void *ctrl)
{
    controlador_t *c = (controlador_t *) ctrl;

    while (c->simulacion_activa && c->hora_actual < c->hora_fin) {

        /* ---- Esperar el equivalente a una hora de simulacion ---- */
        sleep(c->segundos_por_hora);

        /* ---- Avanzar hora de simulacion ---- */
        c->hora_actual++;

        /* ---- Aqui se deberian actualizar entradas/salidas de familias,     */
        printf("[RELOJ] Hora de simulacion: %d\n", c->hora_actual);
        fflush(stdout);
    }

    return NULL;
}


/* **********************************************************************************************************
 *                                      servidor_hilo_agentes                                               *
 * **********************************************************************************************************/
void *servidor_hilo_agentes(void *arg)
{
    controlador_t *ctrl = (controlador_t *) arg;

    char readbuf[256];
    char end_str[5] = "end";
    int  end_process;
    int  stringlen;
    int  read_bytes;

    /* ---- Bucle principal de atencion de agentes ---- */
    while (ctrl->simulacion_activa) {

        /* ---- Bloquea esperando datos desde el FIFO ---- */
        read_bytes = read(ctrl->fifo_fd, readbuf, sizeof(readbuf) - 1);
        if (read_bytes <= 0) {
            /* Error o cierre del otro extremo; puedes decidir si sigues o sales */
            if (read_bytes < 0) {
                perror("[AGENTES] read(FIFO)");
            }
            continue;
        }

        /* ---- Termina la cadena y normaliza: quita salto de linea ---- */
        readbuf[read_bytes] = '\0';

        stringlen = (int) strlen(readbuf);

        if (stringlen > 0 && readbuf[stringlen - 1] == '\n') {
            readbuf[stringlen - 1] = '\0';
            stringlen--;
        }

        end_process = strcmp(readbuf, end_str);

        /* ---- Si NO es "end": procesa mensaje recibido ---- */
        if (end_process != 0) {

            printf("[AGENTES] Recibido desde FIFO: \"%s\" (len=%d)\n",
                   readbuf, stringlen);

            /* Mas adelante:
             *   - parsear readbuf -> solicitud_reserva_t
             *   - llamar servidor_procesar_solicitud(...)
             *   - abrir pipe_respuesta y escribir respuesta_reserva_t
             */
        }
        /* ---- Si es "end": termina el hilo ---- */
        else {
            printf("[AGENTES] Recibido comando de fin \"%s\". Cerrando hilo de agentes.\n",
                   readbuf);

            /* ---- Marca fin global de simulacion (opcional, segun diseño) ---- */
            ctrl->simulacion_activa = 0;

            /* El cierre real del FIFO y unlink se hace en servidor_destruir()   */
            break;
        }
    }

    return NULL;
}
