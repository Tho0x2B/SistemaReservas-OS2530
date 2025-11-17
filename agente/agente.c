/*****************************************************************************************************
 *                                   PONTIFICIA UNIVERSIDAD JAVERIANA                                *
 *                     Departamento de Ingenieria de Sistemas – Sistemas Operativos                  *
 *                                                                                                   *
 * ------------------------------------------------------------------------------------------------- *
 * Autor       : Thomas Leal                                                                         *
 * Fecha       : 27/10/2025                                                                          *
 *                                                                                                   *
 * Archivo     : agente.c                                                                            *
 *                                                                                                   *
 * Descripcion : Este archivo contiene la implementacion de las funciones utilizadas por los         *
 *               procesos Agentes de Reserva dentro del Proyecto del Sistema de Reservas.            *
 *               Los agentes son procesos clientes que:                                              *
 *                                                                                                   *
 *               1. Se registran ante el Controlador enviando su nombre y su pipe de respuesta.      *
 *               2. Leen un archivo CSV con solicitudes de reserva (familia, hora, personas).        *
 *               3. Envian solicitudes al controlador indicando la hora de inicio y fin.             *
 *               4. Reciben la respuesta del controlador por un FIFO dedicado.                       *
 *                                                                                                   *
 *               Este archivo implementa unicamente las funciones de envio de registro y solicitudes,*
 *               mientras que la logica principal del agente se implementa en main.c.                *
 *                                                                                                   *
 *****************************************************************************************************
 * En este archivo estan implementadas las funciones declaradas en agente_reserva.h                  *
 *****************************************************************************************************/

/************************************************* Headers **************************************************/
#include "agente.h"
#include <errno.h>

int preparar_fifo_respuesta(const char *pipe_resp) {
    // Crear FIFO si no existe
    if (mkfifo(pipe_resp, 0666) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo pipe_resp");
            return -1;
        }
    }

    // Abrir el FIFO en modo lectura ANTES del registro
    int fd = open(pipe_resp, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("open pipe_resp");
        return -1;
    }

    return fd; // devolver descriptor para leer la respuesta
}


/************************************************************************************************************
 *                                                                                                          *
 *  int registrar_agente(const char *nombre, const char *pipe_srv, const char *pipe_resp);                  *
 *                                                                                                          *
 *  Proposito: Enviar al controlador un mensaje indicando que este proceso agente ha iniciado y esta listo. *
 *             Se envia el nombre del agente y el pipe donde debe recibir las respuestas.                   *
 *                                                                                                          *
 *  Parametros: nombre     : nombre unico del agente.                                                       *
 *              pipe_srv   : ruta del FIFO del controlador donde se envian mensajes.                        *
 *              pipe_resp  : ruta del FIFO donde este agente recibira respuestas.                           *
 *                                                                                                          *
 *  Retorno:    0 si el registro fue enviado correctamente.                                                 *
 *              -1 si ocurre un error al abrir o escribir en el pipe del controlador.                       *
 *                                                                                                          *
 ************************************************************************************************************/
int registrar_agente(const char *nombre, const char *pipe_srv, const char *pipe_resp)
{
    int fd;
    char msg[MAXLINE];

    /* ---- Abrir el PIPE del controlador para escritura ---- */
    fd = open(pipe_srv, O_WRONLY);
    if (fd < 0) {
        perror("open pipe controlador");
        return -1;
    }

    /* ---- Construir mensaje de registro ---- */
    snprintf(msg, sizeof(msg), "REGISTRO;%s;%s\n", nombre, pipe_resp);

    /* ---- Enviar registro ---- */
    write(fd, msg, strlen(msg));

    /* ---- Cerrar descriptor ---- */
    close(fd);

    return 0;
}

/************************************************************************************************************
 *                                                                                                          *
 *  int enviar_solicitud(const char *familia, int personas, int hora_inicio,                                *
 *                       const char *pipe_srv, const char *pipe_resp);                                      *
 *                                                                                                          *
 *  Proposito: Construir y enviar al controlador una solicitud de reserva.                                  *
 *             Cada solicitud incluye la familia, numero de personas, hora de inicio y hora de fin.         *
 *             El controlador enviara una respuesta mediante el pipe de respuesta del agente.               *
 *                                                                                                          *
 *  Parametros: familia     : nombre de la familia que desea reservar.                                      *
 *              personas    : cantidad de integrantes.                                                       *
 *              hora_inicio : hora de comienzo solicitada.                                                   *
 *              pipe_srv    : FIFO del controlador donde se escriben solicitudes.                           *
 *              pipe_resp   : FIFO del agente donde recibira la respuesta.                                  *
 *                                                                                                          *
 *  Retorno:    0 si el mensaje fue enviado correctamente.                                                  *
 *              -1 si ocurre un error al abrir el pipe del controlador.                                     *
 *                                                                                                          *
 ************************************************************************************************************/
int enviar_solicitud(const char *familia, int personas, int hora_inicio,
                     const char *pipe_srv, const char *pipe_resp)
{
    int fd;
    char msg[MAXLINE];

    /* ---- Abrir pipe del controlador ---- */
    fd = open(pipe_srv, O_WRONLY);
    if (fd < 0) {
        perror("open pipe controlador");
        return -1;
    }

    /* ---- Calcular hora de fin ---- */
    int hora_fin = hora_inicio + 2;

    /* ---- Construccion del mensaje ---- */
    snprintf(msg, sizeof(msg),
             "SOLICITUD;%s;%d;%d;%d;%s\n",
             familia, personas, hora_inicio, hora_fin, pipe_resp);

    /* ---- Enviar mensaje ---- */
    write(fd, msg, strlen(msg));

    /* ---- Cerrar descriptor ---- */
    close(fd);

    return 0;
}
