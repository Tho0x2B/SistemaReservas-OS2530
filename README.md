# SistemaReservas-OS2530
Simulador concurrente de reservas para el parque Berlín usando procesos, hilos POSIX y pipes nominales. Proyecto de Sistemas Operativos 2025-30 (PUJ).

---

## **¿Qué hace el sistema?**

* El **controlador** recibe solicitudes usando un pipe FIFO.
* Cada **agente** se registra con su nombre y un pipe propio para recibir respuestas.
* Cada agente lee un archivo CSV con líneas del tipo:

```
Familia,Hora,Personas
```

* El agente manda la solicitud y espera la respuesta del controlador.
* El controlador revisa:

  * si la hora es válida,
  * si hay cupos,
  * si toca reprogramar,
  * o si toca negar.

La simulación avanza con un **hilo reloj** que mueve la “hora actual” del sistema cada ciertos segundos.

---

## **Estructura básica**

```
agente_reserva.c
agente_reserva.h
controlador.c
controlador.h
```

Cada componente puede compilarse por separado usando un Makefile o manualmente con gcc.

---

## **Ejecución**

### Controlador:

```
./controlador -p /tmp/pipe_controlador
```

### Agente:

```
./agente_reserva -s NombreAgente -a archivo.csv -p /tmp/pipe_controlador
```

El agente crea un pipe propio para las respuestas con el nombre:

```
/tmp/resp_<nombre>
```

---

## **Formato de mensajes**

El protocolo es sencillo y basado en texto:

### Del agente al servidor:

```
REGISTRO;NombreAgente;/tmp/resp_Nombre
SOLICITUD;Familia;Personas;HoraInicio;HoraFin;/tmp/resp_Nombre
```

### Del servidor al agente:

```
ACEPTADA;Hora
REPROGRAMADA;HoraNueva
DENEGADA
```

---

## **Archivos de entrada (CSV)**

Ejemplo típico:

```
Zuluaga,8,10
Dominguez,8,4
Rojas,10,10
```

---

## **Notas importantes**

* Cada reserva dura **2 horas**.
* El agente **no** puede enviar solicitudes para horas menores a la hora actual del sistema.
* Cuando el archivo se acaba, el agente muestra un mensaje y termina.
* El pipe del agente se elimina (`unlink`) al final.

---
