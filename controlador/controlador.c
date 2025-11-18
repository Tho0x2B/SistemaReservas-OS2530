
/*************************************************************************************************************
 *                                   PONTIFICIA UNIVERSIDAD JAVERIANA                                        *
 *                     Departamento de Ingenieria de Sistemas – Sistemas Operativos                          *
 *                                                                                                           *
 * --------------------------------------------------------------------------------------------------------- *
 * Autor       : Thomas Leal, Carolina Ujueta, Diego Melgarejo, Juan Motta                                   *
 * Fecha       : 14/11/2025                                                                                  *
 * Materia:    Sistemas Operativos                                                                           *
 * Profesor:   John Jairo Corredor                                                                      *
 * Fichero:    controlador.c                                                                                 *
 *************************************************************************************************************/

/***************************************** Headers **********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>    
#include <fcntl.h>       
#include <unistd.h>     

#include "controlador.h"

int servidor_inicializar(controlador_t *ctrl)
{
    int h, i;

    /* ---- Validacion de puntero ---- */
    if (ctrl == NULL) {
        fprintf(stderr, "Error: controlador nulo en servidor_inicializar().\n");
        return -1;
    }

    /* ---- Inicializar Mutex ---- */
    if (pthread_mutex_init(&ctrl->mutex, NULL) != 0) {
        perror("mutex_init");
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
        perror("pthread_create (ctrl->hilo_reloj)");
        close(ctrl->fifo_fd);
        ctrl->fifo_fd = -1;
        return -1;
    }

    /* ---- Crear hilo para atencion de agentes (lectura del FIFO) ---- */
    if (pthread_create(&(ctrl->hilo_agentes), NULL, servidor_hilo_agentes, (void *) ctrl) != 0) {
        perror("pthread_create (hilo_agentes)");
        /* Si falla este hilo, cancelamos el de reloj y limpiamos. */
        ctrl->simulacion_activa = 0;
        pthread_cancel(ctrl->hilo_reloj);
        pthread_join(ctrl->hilo_reloj, NULL);
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

    /* ---- Destruir Mutex ---- */
    pthread_mutex_destroy(&ctrl->mutex);

    /* ---- Cerrar FIFO ---- */
    if (ctrl->fifo_fd != -1) {
        close(ctrl->fifo_fd);
        ctrl->fifo_fd = -1;
    }

    /* ---- Eliminar archivo FIFO ---- */
    if (ctrl->pipe_entrada[0] != '\0') {
        unlink(ctrl->pipe_entrada);
    }

    
     /* GENERACION DEL REPORTE FINAL (reporte_final.txt)*/
     
    FILE *fp = fopen("reporte_final.txt", "w");
    if (fp == NULL) {
        perror("Error creando reporte_final.txt");
    } else {
        fprintf(fp, "================ REPORTE FINAL DEL SISTEMA DE RESERVAS ================\n\n");

        /* --- Calcular Horas Pico y Horas Valle --- */
        int max_ocupacion = -1;
        int min_ocupacion = 999999;
        int h;

        /* Paso 1: Encontrar los valores maximo y minimo de ocupacion */
        for (h = ctrl->hora_ini; h < ctrl->hora_fin; h++) {
            int ocupacion = ctrl->horas[h].ocupacion_actual;
            if (ocupacion > max_ocupacion) max_ocupacion = ocupacion;
            if (ocupacion < min_ocupacion) min_ocupacion = ocupacion;
        }

        /* Paso 2: Escribir Horas Pico (A) */
        fprintf(fp, "a. Horas pico (Mayor ocupacion: %d personas):\n", max_ocupacion);
        for (h = ctrl->hora_ini; h < ctrl->hora_fin; h++) {
            if (ctrl->horas[h].ocupacion_actual == max_ocupacion) {
                fprintf(fp, "   - %02d:00\n", h);
            }
        }
        fprintf(fp, "\n");

        /* Paso 3: Escribir Horas con menor numero de personas (B) */
        fprintf(fp, "b. Horas valle (Menor ocupacion: %d personas):\n", min_ocupacion);
        for (h = ctrl->hora_ini; h < ctrl->hora_fin; h++) {
            if (ctrl->horas[h].ocupacion_actual == min_ocupacion) {
                fprintf(fp, "   - %02d:00\n", h);
            }
        }
        fprintf(fp, "\n");

        /* Paso 4: Estadisticas de solicitudes (C, D, E) */
        fprintf(fp, "c. Cantidad de solicitudes negadas        : %d\n", ctrl->solicitudes_negadas);
        fprintf(fp, "d. Cantidad de solicitudes aceptadas      : %d\n", ctrl->solicitudes_ok);
        fprintf(fp, "e. Cantidad de solicitudes reprogramadas  : %d\n", ctrl->solicitudes_reprogramadas);

        fprintf(fp, "\n=======================================================================\n");
        
        printf("\n[SISTEMA] Reporte generado exitosamente en 'reporte_final.txt'.\n");
        fclose(fp);
    }
}

void *servidor_hilo_reloj(void *ctrl)
{
    controlador_t *c = (controlador_t *) ctrl;

    while (c->simulacion_activa && c->hora_actual < c->hora_fin) {

        /* ---- Esperar el equivalente a una hora de simulacion ---- */
        sleep(c->segundos_por_hora);

        /* ---- Proteger cambio de hora con Mutex ---- */
        pthread_mutex_lock(&c->mutex);
        
        /* ---- Avanzar hora de simulacion ---- */
        c->hora_actual++;

        printf("\n[RELOJ] Hora de simulacion: %d:00 (Ocupacion: %d/%d)\n", 
               c->hora_actual, 
               c->horas[c->hora_actual].ocupacion_actual,
               c->aforo_maximo);

        pthread_mutex_unlock(&c->mutex);
    }

    // Cuando termina el horario, cerramos la simulacion
    if (c->hora_actual >= c->hora_fin) {
        printf("[RELOJ] Fin del dia alcanzado. Cerrando sistema...\n");
        c->simulacion_activa = 0;
        // Escribimos un 'end' en el pipe para desbloquear el hilo de agentes si esta esperando
        write(c->fifo_fd, "end", 3); 
    }

    return NULL;
}


/* **********************************************************************************************************
 * servidor_hilo_agentes                                               *
 * **********************************************************************************************************/
void *servidor_hilo_agentes(void *arg)
{
    controlador_t *ctrl = (controlador_t *) arg;

    char readbuf[MAX_LONG_MENSAJE];
    char msg_resp[MAX_LONG_MENSAJE];
    char pipe_resp[MAX_LONG_NOMBRE_PIPE];
    int  read_bytes;
    int  fd_resp;

    /* Punteros para strtok */
    char *tipo_msg, *p1, *p2, *p3, *p4, *p5;

    /* ---- Bucle principal de atencion de agentes ---- */
    while (ctrl->simulacion_activa) {

        /* Limpiar buffer */
        memset(readbuf, 0, sizeof(readbuf));

        /* ---- Bloquea esperando datos desde el FIFO ---- */
        read_bytes = read(ctrl->fifo_fd, readbuf, sizeof(readbuf) - 1);
        
        if (read_bytes <= 0) {
            // Si es error real o EOF inesperado
            if (read_bytes < 0 && errno != EINTR) {
                // Si el simulador sigue activo, es un error. Si no, es cierre normal.
                 if(ctrl->simulacion_activa) perror("[AGENTES] read(FIFO)");
            }
            continue;
        }

        /* ---- Termina la cadena y normaliza: quita salto de linea ---- */
        readbuf[read_bytes] = '\0';
        if (readbuf[strlen(readbuf) - 1] == '\n') {
            readbuf[strlen(readbuf) - 1] = '\0';
        }

        /* Si recibe "end" (enviado por el reloj al finalizar), terminamos */
        if (strcmp(readbuf, "end") == 0) {
            break;
        }

        printf("[AGENTES] Recibido: \"%s\"\n", readbuf);

        /* ---- PARSEO DEL MENSAJE (usamos strtok sobre el buffer) ---- */
        tipo_msg = strtok(readbuf, ";");

        if (tipo_msg != NULL) {
            
            /* ================= CASO REGISTRO ================= */
            if (strcmp(tipo_msg, "REGISTRO") == 0) {
                p1 = strtok(NULL, ";"); // Nombre Agente
                p2 = strtok(NULL, ";"); // Pipe Respuesta

                if (p1 && p2) {
                    printf("[CTRL] Registrando Agente: %s\n", p1);
                    
                    pthread_mutex_lock(&ctrl->mutex);
                    int h_actual = ctrl->hora_actual;
                    pthread_mutex_unlock(&ctrl->mutex);

                    fd_resp = open(p2, O_WRONLY);
                    if (fd_resp != -1) {
                        snprintf(msg_resp, sizeof(msg_resp), "%d", h_actual);
                        write(fd_resp, msg_resp, strlen(msg_resp));
                        close(fd_resp);
                    }
                }
            }
            /* ================= CASO SOLICITUD ================= */
            else if (strcmp(tipo_msg, "SOLICITUD") == 0) {
                p1 = strtok(NULL, ";"); // Familia
                p2 = strtok(NULL, ";"); // Personas
                p3 = strtok(NULL, ";"); // Hora Inicio
                p4 = strtok(NULL, ";"); // Hora Fin
                p5 = strtok(NULL, ";"); // Pipe Respuesta

                if (p1 && p2 && p3 && p5) {
                    int num_pers = atoi(p2);
                    int h_ini    = atoi(p3);
                    strcpy(pipe_resp, p5);
                    
                    char texto_respuesta[128];
                    
                    /* --- LOGICA CRITICA --- */
                    pthread_mutex_lock(&ctrl->mutex);

                    // 1. Validar si la hora ya pasó
                    if (h_ini < ctrl->hora_actual) {
                        ctrl->solicitudes_negadas++; // [cite: 128]
                        sprintf(texto_respuesta, "NEGADA: Hora %d ya paso", h_ini);
                        printf("[CTRL] Rechazada %s (Extemporanea)\n", p1);
                    }
                    // 2. Validar si la hora está fuera de rango
                    else if (h_ini > ctrl->hora_fin) {
                         ctrl->solicitudes_negadas++; // [cite: 128]
                         sprintf(texto_respuesta, "NEGADA: Hora %d cierre", h_ini);
                         printf("[CTRL] Rechazada %s (Cierre)\n", p1);
                    }
                    // 3. Validar aforo
                    else {
                        // Revisamos la hora solicitada y la siguiente (reserva de 2h)
                        int cabe_h1 = (ctrl->horas[h_ini].ocupacion_actual + num_pers) <= ctrl->aforo_maximo;
                        int cabe_h2 = 1; 
                        if (h_ini + 1 < ctrl->hora_fin) {
                            cabe_h2 = (ctrl->horas[h_ini+1].ocupacion_actual + num_pers) <= ctrl->aforo_maximo;
                        }

                        if (cabe_h1 && cabe_h2) {
                            // ACEPTAR
                            ctrl->horas[h_ini].ocupacion_actual += num_pers;
                            if (h_ini + 1 < ctrl->hora_fin) {
                                ctrl->horas[h_ini + 1].ocupacion_actual += num_pers;
                            }
                            ctrl->solicitudes_ok++; // [cite: 129]
                            sprintf(texto_respuesta, "RESERVA OK: %d:00", h_ini);
                            printf("[CTRL] Aceptada %s (%d p) %d:00\n", p1, num_pers, h_ini);
                        } else {
                            // SIN CUPO
                            ctrl->solicitudes_negadas++; // [cite: 128]
                            sprintf(texto_respuesta, "NEGADA: Sin cupo");
                            printf("[CTRL] Rechazada %s (Aforo)\n", p1);
                        }
                    }

                    pthread_mutex_unlock(&ctrl->mutex);
                    /* --- FIN LOGICA CRITICA --- */

                    // Responder
                    fd_resp = open(pipe_resp, O_WRONLY);
                    if (fd_resp != -1) {
                        write(fd_resp, texto_respuesta, strlen(texto_respuesta));
                        close(fd_resp);
                    }
                }
            }
        }
    }

    return NULL;
}
