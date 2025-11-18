/************************************************************************************************************
 *                               Pontificia Universidad Javeriana                                           *
 *                                   Facultad de Ingenieria                                                 *
 *                               Departamento de Ingenieria de Sistemas                                     *
 *                                                                                                          *
 *  Autor:      Thomas Leal Puerta                                                                          *
 *  Fecha:      16 de noviembre de 2025                                                                     *
 *  Materia:    Sistemas Operativos                                                                         *
 *  Profesor:   John Jairo Corredor, PhD                                                                    *
 *  Fichero:    main.c                                                                                      *
 *                                                                                                          *
 *  Objetivo:   Punto de entrada del servidor "Controlador de Reserva".  Este programa se encarga de        *
 *              procesar los argumentos de la linea de comandos, rellenar la estructura de configuracion    *
 *              del controlador y lanzar el servidor mediante las funciones implementadas en                *
 *              `controlador_reserva.c`.  Tras la inicializacion, espera a que los hilos del reloj y de     *
 *              atencion a agentes terminen para posteriormente liberar los recursos.                       *
 ************************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "controlador.h"

int main(int argc, char *argv[])
{
    controlador_t ctrl;

    /* ---- Variables auxiliares para argumentos ---- */
    int horaIni    = -1;
    int horaFin    = -1;
    int segHoras   = -1;
    int aforoTotal = -1;
    char pipeRecibe[MAX_LONG_NOMBRE_PIPE] = {0};

    /* ---- Procesar argumentos de la linea de comandos ----
     * Sintaxis esperada:
     *     ./controlador -i horaIni -f horaFin -s segHoras -t total -p pipeRecibe
     */
    int opt;
    while ((opt = getopt(argc, argv, "i:f:s:t:p:")) != -1) {
        switch (opt) {
        case 'i':
            horaIni = atoi(optarg);
            break;
        case 'f':
            horaFin = atoi(optarg);
            break;
        case 's':
            segHoras = atoi(optarg);
            break;
        case 't':
            aforoTotal = atoi(optarg);
            break;
        case 'p':
            strncpy(pipeRecibe, optarg, MAX_LONG_NOMBRE_PIPE - 1);
            pipeRecibe[MAX_LONG_NOMBRE_PIPE - 1] = '\0';
            break;
        default:
            fprintf(stderr,
                    "Uso: %s -i horaIni -f horaFin -s segHoras -t total -p pipeRecibe\n",
                    argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* ---- Verificar que todos los parametros obligatorios fueron suministrados ---- */
    if (horaIni == -1 || horaFin == -1 || segHoras == -1 ||
        aforoTotal == -1 || pipeRecibe[0] == '\0') {

        fprintf(stderr, "Error: faltan parametros obligatorios.\n");
        fprintf(stderr,
                "Uso: %s -i horaIni -f horaFin -s segHoras -t total -p pipeRecibe\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    /* ---- Validar rangos de los parametros ---- */
    if (horaIni < HORA_MINIMA_SIMULACION || horaIni > HORA_MAXIMA_SIMULACION ||
        horaFin < HORA_MINIMA_SIMULACION || horaFin > HORA_MAXIMA_SIMULACION ||
        horaFin < horaIni || segHoras <= 0 || aforoTotal <= 0) {

        fprintf(stderr, "Error: parametros invalidos.\n");
        fprintf(stderr,
                "Uso: %s -i horaIni -f horaFin -s segHoras -t total -p pipeRecibe\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    /* ---- Cargar los valores en la estructura del controlador ---- */
    ctrl.hora_ini          = horaIni;
    ctrl.hora_fin          = horaFin;
    ctrl.segundos_por_hora = segHoras;
    ctrl.aforo_maximo      = aforoTotal;

    /* Nombre del FIFO de entrada (pipeRecibe) -> campo pipe_entrada */
    strncpy(ctrl.pipe_entrada, pipeRecibe, MAX_LONG_NOMBRE_PIPE - 1);
    ctrl.pipe_entrada[MAX_LONG_NOMBRE_PIPE - 1] = '\0';

    /* ---- Inicializar el servidor (estructuras internas, FIFO y hilos) ---- */
    if (servidor_inicializar(&ctrl) != 0) {
        fprintf(stderr, "Error: no fue posible inicializar el servidor de reservas.\n");
        return EXIT_FAILURE;
    }

    /* ---- Bucle de espera del proceso principal ---- */
    while (ctrl.simulacion_activa) {
        sleep(1);
    }

    /* ---- Al llegar aqui, la simulacion ha terminado. Liberar recursos ---- */
    servidor_destruir(&ctrl);

    return EXIT_SUCCESS;
}
