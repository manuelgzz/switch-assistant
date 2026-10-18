# Investigacion de estabilidad de RTSP

## Resumen

El sysmodule iniciaba RTSP y abría el servicio de captura `grc:d` durante el
arranque. En una Nintendo Switch afectada, perder la conectividad y abrir un
juego podia inhibir la consola. La prueba que eliminó por completo RTSP y la
captura resolvió el problema, mientras que el reporte MQTT del juego no era la
causa.

No se ha identificado todavía cuál componente interno de RTSP provoca el
conflicto: puede ser la apertura de `grc:d`, la captura de video, los hilos de
streaming o el listener de red. Por ello RTSP permanece disponible, pero se
desactiva de forma predeterminada y requiere aceptación explícita del riesgo.

## Comportamiento actual

La opción **System > RTSP (risky)** de la aplicación controla
`rtsp_enabled` en `sdmc:/switch/switch-ha/config.ini`.

```ini
rtsp_enabled=0
```

- `0` es el valor predeterminado. El sysmodule no llama a
  `screen_stream_start`, por lo que no inicia RTSP ni abre `grc:d`.
- `1` conserva el comportamiento RTSP original: el sysmodule ejecuta
  `screen_stream_start` al arrancar.
- Cambiar la opción requiere reiniciar la consola para que el sysmodule lea la
  nueva configuración.

La interfaz muestra la advertencia: `RTSP can affect games; enable at your own
risk.`

## Evidencia de la investigacion

1. Se reprodujo una inhibición de la consola al perder conectividad y abrir un
   juego, o al perder conectividad mientras ya había un juego en ejecución.
2. Se investigaron MQTT, el reporte del nombre del juego y el sondeo de
   procesos. Deshabilitar el seguimiento de juegos no resolvió el fallo.
3. Se creó una versión desde `origin/main` que eliminó el código RTSP, el
   listener, los hilos de streaming, la captura `grc:d` y la entidad MQTT de
   URL RTSP.
4. Con RTSP eliminado, la consola funcionó correctamente durante la misma
   prueba de conectividad y juegos.
5. La rama actual restaura el código RTSP original y añade únicamente el
   interruptor de configuración. La persona que realizó la prueba confirmó que
   esta variante funciona correctamente con RTSP desactivado.

## Archivos relevantes

| Archivo | Responsabilidad |
| --- | --- |
| `source/screen_stream.c` | Listener RTSP, protocolo RTSP/RTP y captura con `grc:d`. |
| `include/screen_stream.h` | Interfaz del módulo RTSP. |
| `sysmodule/source/main.c` | Arranque condicional de RTSP mediante `rtsp_enabled`. |
| `include/app_state.h` | Campo `AppConfig.rtsp_enabled`. |
| `source/config.c` | Valores predeterminados, lectura y persistencia de `rtsp_enabled`. |
| `source/ui.c` | Control y advertencia de RTSP en la aplicación. |

## Procedimiento de prueba seguro

1. Instalar una compilación con `rtsp_enabled=0`.
2. Reiniciar la consola y verificar juegos, MQTT y cambios de conectividad.
3. Activar **RTSP (risky)** solo para una prueba controlada.
4. Reiniciar y repetir la secuencia que antes provocaba la inhibición.
5. Si vuelve a ocurrir, desactivar RTSP desde la aplicación, reiniciar y
   conservar `sysmodule.log` y `sysmodule-heartbeat.txt` para análisis.

No se debe habilitar RTSP como valor predeterminado hasta que se aísle y
resuelva el conflicto.

## Trabajo pendiente

Para retomar RTSP sin introducir regresiones, realizar cambios pequeños y
probar cada uno en hardware:

1. Iniciar solo el listener RTSP, sin abrir `grc:d`.
2. Abrir `grc:d` únicamente al recibir una solicitud RTSP `PLAY`.
3. Detener la captura, cerrar `grc:d` y finalizar los hilos cuando el cliente
   envíe `TEARDOWN`, se desconecte o pierda la red.
4. Ejecutar los hilos RTSP a prioridad de fondo y comprobar que no compitan
   con juegos.
5. Registrar de forma acotada la creación del listener, la apertura de
   `grc:d`, el inicio de captura y el cierre de cada recurso.

Cada etapa debe comprobarse con un juego abierto, con y sin conectividad, antes
de avanzar a la siguiente.
