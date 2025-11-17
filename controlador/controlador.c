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

    /* ---- Validación de puntero ---- */
    if (ctrl == NULL) {
        fprintf(stderr, "Error: controlador nulo en servidor_inicializar().\n");
        return -1;
    }

    /* ---- Inicializaciones MINIMAS antes de cualquier return ---- */
    ctrl->reloj_iniciado = 0;
    ctrl->fifo_fd = -1;

    /* ---- Inicializar hora actual y bandera de simulación ---- */
    ctrl->hora_actual       = ctrl->hora_ini;
    ctrl->simulacion_activa = 1;

    /* ---- Inicializar estadísticas globales ---- */
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

    /* --------------------------------------------------------------------
       NOTA IMPORTANTE:
       Ya NO iniciamos el hilo de reloj aquí.
       Se iniciará en servidor_hilo_agentes(), al primer REGISTRO.
       -------------------------------------------------------------------- */

    /* ---- Crear hilo para atención de agentes ---- */
    if (pthread_create(&(ctrl->hilo_agentes), NULL,
                       servidor_hilo_agentes, (void *) ctrl) != 0) {

        perror("pthread_create (hilo_agentes)");
        ctrl->simulacion_activa = 0;

        /* Si el reloj ya había sido iniciado por accidente, lo limpiamos */
        if (ctrl->reloj_iniciado) {
            pthread_cancel(ctrl->hilo_reloj);
            pthread_join(ctrl->hilo_reloj, NULL);
        }

        if (ctrl->fifo_fd != -1) {
            close(ctrl->fifo_fd);
            ctrl->fifo_fd = -1;
        }

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
    
            if (c->hora_actual >= c->hora_fin) {
                /* Marcar fin para que otros hilos terminen limpiamente */
                c->simulacion_activa = 0;
                break;
            }
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
    
            /* ---- Bloquea esperando datos desde el FIFO nominal del servidor ---- */
            read_bytes = read(ctrl->fifo_fd, readbuf, sizeof(readbuf) - 1);
            if (read_bytes <= 0) {
                /* Si read devuelve 0 => otro extremo cerrado; salir si simulacion terminó */
                if (read_bytes == 0) {
                    /* El FIFO fue cerrado (probablemente en servidor_destruir) */
                    break;
                }
                if (read_bytes < 0) {
                    perror("[AGENTES] read(FIFO)");
                    /* Pequeño sleep para evitar busy-loop en caso de error transitorio */
                    sleep(1);
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
                fflush(stdout);
    
                /* ---- Caso: registro de agente: "REGISTRO;Nombre;/ruta/pipe" ---- */
                if (strncmp(readbuf, "REGISTRO;", 9) == 0) {
                    char nombre[64];
                    char pipe_agente_path[256];
    
                    /* parseo seguro */
                    if (sscanf(readbuf + 9, "%63[^;];%255s", nombre, pipe_agente_path) >= 1) {
    
                        printf("[AGENTES] Registro de agente '%s', pipe respuesta: %s\n",
                               nombre, pipe_agente_path);
                        /* --- Iniciar el reloj cuando llega el PRIMER registro --- */
                        if (!ctrl->reloj_iniciado) {
                        if (pthread_create(&(ctrl->hilo_reloj), NULL,
                           servidor_hilo_reloj, (void *)ctrl) != 0) {
                            perror("[AGENTES] pthread_create reloj");
                        } else {
                            ctrl->reloj_iniciado = 1;
                            printf("[AGENTES] Hilo de reloj INICIADO tras primer registro.\n");
                            fflush(stdout);
        }
    }
    
                        fflush(stdout);
    
                        /* Crear FIFO de respuesta si no existe */
                        if (mkfifo(pipe_agente_path, 0666) == -1) {
                            if (errno != EEXIST) {
                                perror("[AGENTES] mkfifo(pipe_agente)");
                                /* no podemos atender este agente ahora */
                                continue;
                            }
                        }
    
                        /* Abrir el FIFO de respuesta en modo escritura.
                           Puede ocurrir que el agente aún no haya hecho open( O_RDONLY ).
                           Para evitar bloqueo indefinido, hacemos reintentos por unos
                           segundos (timeout simple). */
                        int fd_resp = -1;
                        int tries = 0;
                        const int max_tries = 20; /* 20 * 100ms = 2s de espera total */
                        while (tries < max_tries) {
                            fd_resp = open(pipe_agente_path, O_WRONLY | O_NONBLOCK);
                            if (fd_resp != -1) break;
                            if (errno == ENXIO || errno == EWOULDBLOCK) {
                                /* nadie leyendo todavía, espera un poco */
                                usleep(100000); /* 100 ms */
                                tries++;
                                continue;
                            } else {
                                perror("[AGENTES] open(pipe_agente) error");
                                break;
                            }
                        }
    
                        /* si no pudimos abrir en modo NONBLOCK, intentamos bloquear brevemente */
                        if (fd_resp == -1) {
                            /* último intento en modo bloqueante (puede bloquear si no hay lector) */
                            fd_resp = open(pipe_agente_path, O_WRONLY);
                            if (fd_resp == -1) {
                                perror("[AGENTES] open(pipe_agente) ultimo intento fallo");
                            }
                        }
    
                        if (fd_resp != -1) {
                            /* escribir la hora actual al agente */
                            char msg[128];
                            int len = snprintf(msg, sizeof(msg), "HORA;%d\n", ctrl->hora_actual);
                            if (len > 0) {
                                ssize_t w = write(fd_resp, msg, (size_t)len);
                                if (w == -1) {
                                    perror("[AGENTES] write(pipe_agente)");
                                } else {
                                    printf("[AGENTES] Enviado a %s: %s", pipe_agente_path, msg);
                                    fflush(stdout);
                                }
                            }
                            close(fd_resp);
                        } else {
                            printf("[AGENTES] No se pudo abrir pipe de respuesta para agente %s\n", nombre);
                            fflush(stdout);
                        }
                    } else {
                        printf("[AGENTES] Formato de REGISTRO invalido: %s\n", readbuf);
                        fflush(stdout);
                    }
    
                } else if (strncmp(readbuf, "SOLICITUD;", 10) == 0) {
    
                          char familia[64];
                          int personas, hora_ini, hora_fin;
                          char pipe_agente[256];
    
                        /* Formato esperado:
                           SOLICITUD;Familia;personas;hora_ini;hora_fin;/tmp/resp_A1
                            */
                        int r = sscanf(readbuf + 10, "%63[^;];%d;%d;%d;%255s",familia, &personas, &hora_ini, &hora_fin, pipe_agente);
                    
                    if (r != 5) {
                    printf("[AGENTES] Formato SOLICITUD invalido: %s\n", readbuf);
                    fflush(stdout);
                    continue;
                       }
            
                    printf("[AGENTES] Solicitud recibida: familia=%s, personas=%d, h_ini=%d, h_fin=%d, pipe=%s\n", familia, personas, hora_ini, hora_fin, pipe_agente);
                    fflush(stdout);
    
    
                    int resultado = servidor_procesar_solicitud(ctrl, familia, personas, hora_ini, hora_fin);
    
        /* ===============================================================
           ABRIR FIFO DEL AGENTE PARA MANDAR RESPUESTA
           =============================================================== */
                    int fd_resp = open(pipe_agente, O_WRONLY | O_NONBLOCK);
                    if (fd_resp == -1) {
                        perror("[AGENTES] open(pipe_agente respuesta)");
                        continue;
                    }
    
                    char msg[256];
    
                    if (resultado == 0) {
                    /* ACEPTADA */
                        snprintf(msg, sizeof(msg), "RESP;OK;%d\n", hora_ini);
                }
                    else if (resultado == 1) {
            /* REPROGRAMADA */
                        snprintf(msg, sizeof(msg), "RESP;REPROG;%d\n", hora_ini + 1);
                }
                else {
            /* DENEGADA */
                    snprintf(msg, sizeof(msg), "RESP;DENEGADA;%d\n", hora_ini);
                    }
    
                    write(fd_resp, msg, strlen(msg));
                    close(fd_resp);
    
                    printf("[AGENTES] Respuesta enviada al agente: %s", msg);
                       fflush(stdout);
                }
                else {
                printf("[AGENTES] Mensaje desconocido: %s\n", readbuf);
                fflush(stdout);
                }
    
            }
            /* ---- Si es "end": termina el hilo ---- */
            else {
                printf("[AGENTES] Recibido comando de fin \"%s\". Cerrando hilo de agentes.\n",
                       readbuf);
                fflush(stdout);
    
                /* ---- Marca fin global de simulacion (opcional, segun diseño) ---- */
                ctrl->simulacion_activa = 0;
                break;
            }
        }
    
        return NULL;
    }
    
    
    int servidor_procesar_solicitud(controlador_t *ctrl, const char *familia, int personas,int hora_ini, int hora_fin)
    {
        /* Validar rango */
        if (hora_ini < ctrl->hora_ini || hora_fin > ctrl->hora_fin)
            return -1;
    
        /* Verificar aforo */
        if (ctrl->horas[hora_ini].ocupacion_actual + personas <= ctrl->aforo_maximo) {
    
            int idx = ctrl->horas[hora_ini].num_reservas;
            if (idx >= MAX_RESERVAS_POR_HORA)
                return -1;
    
            strcpy(ctrl->horas[hora_ini].reservas[idx].nombre_familia, familia);
            ctrl->horas[hora_ini].reservas[idx].hora_inicio  = hora_ini;
            ctrl->horas[hora_ini].reservas[idx].hora_fin     = hora_fin;
            ctrl->horas[hora_ini].reservas[idx].num_personas = personas;
    
            ctrl->horas[hora_ini].num_reservas++;
            ctrl->horas[hora_ini].ocupacion_actual += personas;
    
            ctrl->solicitudes_ok++;
    
            return 0;   /* RESERVA ACEPTADA */
        }
    
        /* Intentar reprogramación */
        if (hora_ini + 1 <= ctrl->hora_fin &&
            ctrl->horas[hora_ini + 1].ocupacion_actual + personas <= ctrl->aforo_maximo)
        {
            ctrl->solicitudes_reprogramadas++;
            return 1;   /* REPROGRAMADA */
        }
    
        ctrl->solicitudes_negadas++;
        return -1;      /* DENEGADA */
    }
