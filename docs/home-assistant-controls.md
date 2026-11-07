# Nuevos controles e integración MQTT

Esta primera integración toma como referencia las funciones de FaserF/switch-assistant,
revisadas frente a las API disponibles en nuestro devkitPro. Daybreak no se incluye.
La compilación y las pruebas de protocolo/discovery se pueden validar en Linux;
los servicios privilegiados y el overlay necesitan pruebas en una Switch con Atmosphere.

## Topics y migración

Cada consola necesita un `Client ID` distinto. Los comandos generales ahora se envían a:

```
switch_ha/<client_id_sanitizado>/command
```

Los botones de discovery se actualizan al reconectar. Las automatizaciones MQTT
escritas a mano deben dejar de publicar en `<discovery_prefix>/command`.
Ese topic compartido ya no se suscribe, para evitar controlar varias consolas a la vez.
Los comandos y acciones no deben publicarse con `retain: true`: se rechazan.
Los estados y discovery sí se retienen.

La disponibilidad usa `switch_ha/<id>/availability` con `online`/`offline` y
último testamento MQTT. Una pérdida de red ya no significa `Sleep`. Los servicios
opcionales añaden disponibilidad por servicio a sus entidades.

## Controles

| Entidad | Comando en `control/<entidad>` | Descripción |
| --- | --- | --- |
| `set_brightness` | número 0–100 | Brillo; conserva el sensor anterior |
| `screen` | `ON` / `OFF` | Retroiluminación; no suspende la consola |
| `set_volume` | número 0–100 | Volumen de la salida activa |
| `volume_up`, `volume_down` | `PRESS` | Pasos de 5 puntos porcentuales |
| `mute` | `ON` / `OFF` | Silenciar/restaurar el último volumen registrado en esta ejecución |
| `stream_enabled` | `ON` / `OFF` | Permitir captura RTSP; no persiste al reiniciar |
| `refresh` | `PRESS` | Solicitar publicación de sensores |
| `get_logs` | `PRESS` | Diagnóstico y lista acotada de mensajes técnicos |
| `refresh_games` | `PRESS` | Escanear juegos y publicar selectores |
| `close_game` | `PRESS` | Terminar el juego identificado; puede perder progreso no guardado |
| `sleep` | `PRESS` | Solicitar suspensión; requiere validar permisos y reanudación en consola |

Los comandos generales `reboot`, `shutdown`, `sleep`, `close_game`, `get_logs`,
`volume_up`, `volume_down`, `refresh` y `refresh_games` también se aceptan en `/command`.
`command_status` informa el resultado de la llamada a la API y su código hexadecimal;
no confirma que una transición física haya terminado. La suspensión por syscall
publica `sleep:accepted` antes de ejecutarla, sin inventar una confirmación.

No se implementa encendido desde apagado ni despertar mediante MQTT: si el sistema
no ejecuta el cliente o carece de red, no puede recibir la orden.

## Sensores

Se añaden uptime, reconexiones y duración de sesión MQTT; temperatura interna;
Wi-Fi en barras **0–3**, tipo de conexión; memoria **del proceso** en MiB; SD total;
almacenamiento interno libre/total; estado dock mediante `omm`; duración de la
sesión del juego; conexión por cable y batería de cada mando; estado, clientes
(actualmente 0 o 1) y resultado de captura RTSP.

La batería de mandos usa la escala nativa **0–4**, no porcentajes. En un par de
Joy-Con se publica el menor nivel; no equivale a dos mediciones individuales.
Las mediciones extendidas se actualizan cada cinco segundos. La duración del juego
empieza cuando este proceso detecta el título y se reinicia si cambia o deja de
observarse; no es un historial persistente ni identifica al usuario que juega.

El diagnóstico no exporta configuración, tokens, contraseñas ni texto de
notificaciones. Sus atributos incluyen servicios disponibles y mensajes técnicos
permitidos. No es una descarga completa de los archivos de logs.

## Juegos instalados

Pulsar **Refresh installed games** después de instalar o quitar juegos. El escaneo
es bajo demanda, con un máximo de 128 registros. La lista muestra el nombre del
catálogo local y el Title ID; títulos desconocidos usan `Game [Title ID]`.

Los selectores solicitan el lanzamiento mediante el servicio de applets y solo están
disponibles desde un contexto compatible de aplicación/library applet. En el sysmodule
(`AppletType_None`) la API no está inicializada: los selectores se marcan como no
disponibles. El lanzamiento en segundo plano queda pendiente de una ruta específica;
no se intenta inicializar un applet dentro del sysmodule ni se anuncia éxito falso.
El catálogo y sus contadores sí funcionan independientemente de esta limitación.

La capacidad multijugador no se adivina por el nombre. Se configura en:

```
sdmc:/switch/switch-ha/game-players.ini
```

Ejemplo de formato (sustituir por los IDs y capacidades reales):

```ini
0100000000000000=2
0100000000000001=4
```

Tras refrescar se crean selectores para 2+, 4+ y 8 jugadores cuando tienen opciones,
y sensores de cantidad. Los juegos sin metadatos aparecen en la lista general.

## Acciones del overlay

Abrir/cerrar el menú con **ZL + ZR + Minus**. Seleccionar con arriba/abajo,
ejecutar con A y cerrar con B. El overlay no bloquea la entrada del juego;
probar primero en HOME para evitar acciones simultáneas en una partida.

Los cuatro nombres se leen al iniciar desde:

```
sdmc:/switch/switch-ha/quick-actions.ini
```

```ini
1=Modo de juego
2=Pausar luces
3=Encender escritorio
4=Apagar luces
```

El sysmodule publica el evento de Home Assistant `Overlay action` con
`quick_action_1` a `quick_action_4`. Cada evento debe vincularse a una automatización
que ejecute la escena o servicio deseado. No se ejecutan servicios REST desde el
sysmodule ni se guardan credenciales adicionales en el overlay. La cola de archivo
contiene una sola acción pendiente: no es una cola duradera de múltiples eventos.

## Validación

En Linux:

```sh
python3 tests/test_mqtt_discovery.py
python3 tests/test_mqtt_commands.py
gcc -Wall -Wextra -Werror -fsanitize=address,undefined -pthread -Iinclude \
  tests/mqtt_wire_test.c -o /tmp/mqtt-wire-test
/tmp/mqtt-wire-test
```

Las pruebas ejecutan los constructores reales de discovery y verifican JSON,
ausencia de campos duplicados, aislamiento entre dispositivos y disponibilidad.
El test de sockets cubre fragmentación, paquetes contiguos, tamaño inválido y EOF.
La construcción de release sigue `.github/workflows/release.yml`.

Antes de publicar un release probar en consola: reconexión MQTT, dos consolas en el
mismo broker, brillo/pantalla en portátil y dock, volumen por salida, mandos y
Joy-Con, consulta de almacenamiento, lanzamiento/cierre de juegos, suspensión y
reanudación, streaming y acciones del overlay. Un binario compilado no acredita
estos resultados físicos.

## Trabajo restante

La integración de actualizaciones de Atmosphere y un flujo de descarga/verificación
no forma parte de esta primera entrega. Tampoco se añade instalación de firmware
Nintendo, Daybreak, un `media_player`/`remote` MQTT no admitido por Home Assistant,
ni un historial persistente de tiempo jugado. Deben diseñarse como fases separadas;
las entidades y comandos descritos arriba sí forman parte de esta implementación.
