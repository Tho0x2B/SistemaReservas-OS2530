/*****************************************************************************************************
 *                                   PONTIFICIA UNIVERSIDAD JAVERIANA                                *
 *                     Departamento de Ingenieria de Sistemas – Sistemas Operativos                  *
 *                                                                                                   *
 * ------------------------------------------------------------------------------------------------- *
 * Archivo     : agente.h                                                                            *
 * Autor       : Thomas Leal Puerta                                                                  *
 * Fecha       : 27/10/2025                                                                          *
 *                                                                                                   *
 * Descripcion : Archivo de cabecera para el modulo del Agente de Reserva.                           *
 *               Contiene la definicion de constantes, prototipos de funciones y los includes        *
 *               necesarios para la comunicacion entre agentes y el Controlador del Sistema de       *
 *               Reservas utilizando FIFOs (named pipes).                                            *
 *                                                                                                   *
 *****************************************************************************************************/

#ifndef __AGENTE_H__
#define __AGENTE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/************************************************* Headers **************************************************/
#include <stdio.h>

#define MAXLINE 256   /* Tamaño maximo de buffer para mensajes */

/************************************************* Prototipos ************************************************/

/*  
 * registrar_agente()
 * Envia al controlador un mensaje de registro con:
 *   - nombre del agente
 *   - pipe por donde recibira respuestas
 */
int registrar_agente(const char *nombre, const char *pipe_srv, const char *pipe_resp);

/*
 * enviar_solicitud()
 * Envia una solicitud de reserva en el formato:
 *   SOLICITUD;familia;personas;hora_inicio;hora_fin;pipe_respuesta
 */
int enviar_solicitud(const char *familia, int personas, int hora_inicio,
                     const char *pipe_srv, const char *pipe_resp);

/*
 * leer_respuesta()
 * Lee la respuesta enviada por el Controlador desde el FIFO propio del agente.
 */
int leer_respuesta(const char *pipe_resp, char *buffer, size_t tam);

/*
 * procesar_respuesta()
 * Imprime o maneja la respuesta recibida desde el servidor.
 */
void procesar_respuesta(const char *respuesta);

#endif /* __AGENTE_H__ */
