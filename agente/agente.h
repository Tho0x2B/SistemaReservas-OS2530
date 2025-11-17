/*****************************************************************************************************
 *                                   PONTIFICIA UNIVERSIDAD JAVERIANA                                *
 *                     Departamento de Ingeniería de Sistemas – Sistemas Operativos                  *
 *                                                                                                   *
 * ------------------------------------------------------------------------------------------------- *
 * Archivo     : agente_reserva.h                                                                    *
 * Autor       : Carolina Ujueta                                                                     *
 * Fecha       : 27/10/2025                                                                          *
 *                                                                                                   *
 * Descripción : Archivo de cabecera para el módulo del Agente de Reserva.                           *
 *               Contiene la definición de constantes, prototipos de funciones y los includes        *
 *               necesarios para la comunicación entre agentes y el Controlador del Sistema de       *
 *               Reservas utilizando FIFOs (named pipes).                                            *
 *                                                                                                   *
 *****************************************************************************************************/

#ifndef AGENTE_RESERVA_H
#define AGENTE_RESERVA_H

/************************************************* Headers **************************************************/
#include <stdio.h>

#define MAXLINE 256   /* Tamaño máximo de buffer para mensajes */

/************************************************* Prototipos ************************************************/

/*  
 * registrar_agente()
 * Envía al controlador un mensaje de registro con:
 *   - nombre del agente
 *   - pipe por donde recibirá respuestas
 */
int registrar_agente(const char *nombre, const char *pipe_srv, const char *pipe_resp);

/*
 * enviar_solicitud()
 * Envía una solicitud de reserva en el formato:
 *   SOLICITUD;familia;personas;hora_inicio;hora_fin;pipe_respuesta
 */
int enviar_solicitud(const char *familia, int personas, int hora_inicio,
                     const char *pipe_srv, const char *pipe_resp);

#endif /* AGENTE_RESERVA_H */
