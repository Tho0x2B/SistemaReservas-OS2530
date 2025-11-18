/************************************************************************************************************/
#ifndef __CONTROLADOR_H__
#define __CONTROLADOR_H__

/***************************************** Headers **********************************************************/
#include <pthread.h>

#define HORA_MINIMA_SIMULACION        7
#define HORA_MAXIMA_SIMULACION        19
#define MAX_HORAS_DIA                 24

#define MAX_LONG_NOMBRE_FAMILIA       64
#define MAX_LONG_NOMBRE_AGENTE        64
#define MAX_LONG_NOMBRE_PIPE          128
#define MAX_LONG_MENSAJE              256

#define MAX_RESERVAS_POR_HORA         128

/* ---- Tipos de respuesta ---- */
typedef enum {
    RESPUESTA_RESERVA_OK = 0,
    RESPUESTA_RESERVA_REPROGRAMADA,
    RESPUESTA_RESERVA_NEGADA_EXTEMP,
    RESPUESTA_RESERVA_NEGADA_SIN_CUPO,
    RESPUESTA_RESERVA_NEGADA_AFORO
} tipo_respuesta_t;

/* ---- Solicitud que envia el agente ---- */
typedef struct {
    char nombre_agente[MAX_LONG_NOMBRE_AGENTE];
    char nombre_familia[MAX_LONG_NOMBRE_FAMILIA];
    int  hora_solicitada;
    int  num_personas;
    char pipe_respuesta[MAX_LONG_NOMBRE_PIPE];
} solicitud_reserva_t;

/* ---- Reserva resultante ---- */
typedef struct {
    char nombre_familia[MAX_LONG_NOMBRE_FAMILIA];
    int  hora_inicio;
    int  hora_fin;
    int  num_personas;
} reserva_t;

/* ---- Respuesta del servidor ---- */
typedef struct {
    tipo_respuesta_t tipo;
    reserva_t        reserva;
    char             mensaje[MAX_LONG_MENSAJE];
} respuesta_reserva_t;

/* ---- Estado de una hora especifica ---- */
typedef struct {
    int hora;
    int ocupacion_actual;
    int aforo_maximo;

    int       num_reservas;
    reserva_t reservas[MAX_RESERVAS_POR_HORA];
} estado_hora_t;

/* ---- Estado global del Controlador ---- */
typedef struct {
    int hora_ini;
    int hora_fin;
    int hora_actual;
    int segundos_por_hora;
    int aforo_maximo;

    int solicitudes_negadas;
    int solicitudes_ok;
    int solicitudes_reprogramadas;

    estado_hora_t horas[MAX_HORAS_DIA + 1];
    
    pthread_t hilo_reloj, hilo_agentes;

    char pipe_entrada[MAX_LONG_NOMBRE_PIPE];
    int  fifo_fd;

    char simulacion_activa;

    /* --- AGREGADO: Mutex para sincronizacion --- */
    pthread_mutex_t mutex; 

} controlador_t;

/***************************************** Prototipos *******************************************************/

int  servidor_inicializar(controlador_t *ctrl);
void servidor_destruir(controlador_t *ctrl);

void *servidor_hilo_reloj   (void *arg);
void *servidor_hilo_agentes (void *arg);

#endif /* __CONTROLADOR_H__ */
