/*****************************************************************************************************
 *                                   PONTIFICIA UNIVERSIDAD JAVERIANA                                *
 *                     Departamento de Ingeniería de Sistemas – Sistemas Operativos                  *
 *                                                                                                   *
 * ------------------------------------------------------------------------------------------------- *
 * Autor       : Carolina Ujueta                                                                     *
 * Fecha       : 27/10/2025                                                                          *
 *                                                                                                   *
 * Archivo     : agente_reserva.c                                                                    *
 *                                                                                                   *
 * Descripción : Este archivo contiene la implementación de las funciones utilizadas por los         *
 *               procesos Agentes de Reserva dentro del Proyecto del Sistema de Reservas.            *
 *               Los agentes son procesos clientes que:                                               *
 *                                                                                                   *
 *               1. Se registran ante el Controlador enviando su nombre y su pipe de respuesta.      *
 *               2. Leen un archivo CSV con solicitudes de reserva (familia, hora, personas).        *
 *               3. Envían solicitudes al controlador indicando la hora de inicio y fin.             *
 *               4. Reciben la respuesta del controlador por un FIFO dedicado.                        *
 *                                                                                                   *
 *               Este archivo implementa únicamente las funciones de envío de registro y solicitudes, *
 *               mientras que la lógica principal del agente se implementa en main.c.                *
 *                                                                                                   *
 *****************************************************************************************************
 * En este archivo están implementadas las funciones declaradas en agente_reserva.h                  *
 *****************************************************************************************************/

/************************************************* Headers **************************************************/
#include "agente_reserva.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/************************************************************************************************************
 *                                                                                                          *
 *  int registrar_agente(const char *nombre, const char *pipe_srv, const char *pipe_resp);                  *
 *                                                                                                          *
 *  Propósito: Enviar al controlador un mensaje indicando que este proceso agente ha iniciado y está listo. *
 *             Se envía el nombre del agente y el pipe donde debe recibir las respuestas.                   *
 *                                                                                                          *
 *  Parámetros: nombre     : nombre único del agente.                                                       *
 *              pipe_srv   : ruta del FIFO del controlador donde se envían mensajes.                        *
 *              pipe_resp  : ruta del FIFO donde este agente recibirá respuestas.                           *
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
 *  Propósito: Construir y enviar al controlador una solicitud de reserva.                                  *
 *             Cada solicitud incluye la familia, número de personas, hora de inicio y hora de fin.         *
 *             El controlador enviará una respuesta mediante el pipe de respuesta del agente.               *
 *                                                                                                          *
 *  Parámetros: familia     : nombre de la familia que desea reservar.                                      *
 *              personas    : cantidad de integrantes.                                                       *
 *              hora_inicio : hora de comienzo solicitada.                                                   *
 *              pipe_srv    : FIFO del controlador donde se escriben solicitudes.                           *
 *              pipe_resp   : FIFO del agente donde recibirá la respuesta.                                  *
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

    /* ---- Construcción del mensaje ---- */
    snprintf(msg, sizeof(msg),
             "SOLICITUD;%s;%d;%d;%d;%s\n",
             familia, personas, hora_inicio, hora_fin, pipe_resp);

    /* ---- Enviar mensaje ---- */
    write(fd, msg, strlen(msg));

    /* ---- Cerrar descriptor ---- */
    close(fd);

    return 0;
}
