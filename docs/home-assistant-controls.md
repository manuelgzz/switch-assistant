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

Los controles usan `switch_ha/<id>/status`, un JSON retenido con `status=online`
o `offline`; los que requieren servicios opcionales también comprueban su disponibilidad.
Los sensores y sensores binarios no dependen de esa disponibilidad: conservan su
último estado retenido durante la suspensión o una desconexión. Es una última
lectura conocida, no una medición nueva mientras la consola duerme.

Desde fix6, `Operation Mode` lee `operation_mode` del mismo JSON. El último testamento
MQTT publica `Sleep` con `mode_source=mqtt_disconnect` al perderse la conexión.
Es una inferencia: una caída de red también puede mostrar Sleep. Con keepalive de
30 segundos, el broker normalmente detecta el silencio en unos 45 segundos desde
el último paquete. `Power state source` permite distinguir `live` de esa inferencia.
No se registra ningún participante en la suspensión de Horizon.

## Controles

| Entidad | Comando en `control/<entidad>` | Descripción |
| --- | --- | --- |
| `set_brightness` | número 0–100 | Brillo; conserva el sensor anterior |
| `screen` | `ON` / `OFF` | Retroiluminación; no suspende la consola |
| `set_volume` | número 0–100 | Volumen de la salida activa |
| `volume_up`, `volume_down` | `PRESS` | Pasos de 5 puntos porcentuales |
| `mute` | `ON` / `OFF` | Mute nativo de la salida, con alternativa de volumen cero/restauración |
| `refresh` | `PRESS` | Solicitar publicación de sensores |
| `get_logs` | `PRESS` | Diagnóstico y lista acotada de mensajes técnicos |
| `refresh_games` | `PRESS` | Escanear juegos y publicar selectores |

Los comandos generales `reboot`, `shutdown`, `get_logs`,
`volume_up`, `volume_down`, `refresh` y `refresh_games` también se aceptan en `/command`.
`command_status` informa el resultado de la llamada a la API y su código hexadecimal;
no confirma que una transición física haya terminado. La suspensión remota está deshabilitada tras un fallo en consola: `sleep` y
`standby` responden `sleep:disabled` y no llaman a APIs de energía. El permiso
`svcSleepSystem` se retiró del sysmodule. El discovery retenido de Sleep y del
control RTSP se limpia automáticamente al reconectar. El streaming RTSP y sus
sensores se conservan; solo se retiró su interruptor remoto.

No se implementa encendido desde apagado ni despertar mediante MQTT: si el sistema
no ejecuta el cliente o carece de red, no puede recibir la orden.

## Sensores

Se añaden uptime, reconexiones y duración de sesión MQTT; temperatura interna;
Wi-Fi en barras **0–3**, tipo de conexión; memoria **del proceso** en MiB; SD total;
almacenamiento interno libre/total; estado dock mediante `omm`; duración de la
sesión del juego; conexión por cable y batería de cada mando; estado, clientes
(actualmente 0 o 1) y resultado de captura RTSP.

La detección de juegos consulta el proceso de aplicación mediante `pm:dmnt`,
resuelve el Title ID y usa el catálogo solo para el nombre. Un juego ausente del
catálogo se publica como `Unknown game [Title ID]` y continúa reportándose como
en ejecución. `Game detection diagnostic` indica el método o el error de consulta.

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
python3 -m unittest discover -s tests -p 'test_*.py'
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
Joy-Con, consulta de almacenamiento, lanzamiento/cierre de juegos, rechazo de comandos de suspensión, streaming y acciones del overlay. Un binario compilado no acredita
estos resultados físicos.

## Trabajo restante

La integración de actualizaciones de Atmosphere y un flujo de descarga/verificación
no forma parte de esta primera entrega. Tampoco se añade instalación de firmware
Nintendo, Daybreak, un `media_player`/`remote` MQTT no admitido por Home Assistant,
ni un historial persistente de tiempo jugado. Deben diseñarse como fases separadas;
las entidades y comandos descritos arriba sí forman parte de esta implementación.


## Instalación de 1.12.1-fix1

Copiar el NRO descargado de Actions a `sdmc:/switch/switch-ha/switch-ha.nro`,
abrir **Switch Assistant** para instalar los componentes incrustados y reiniciar
la consola para que se cargue el sysmodule nuevo. Copiar el NRO sin abrirlo no
actualiza el proceso que ya está ejecutándose. Home Assistant elimina los dos
controles retirados cuando el sysmodule nuevo reconecta y limpia su discovery.

Las pruebas de audio usan dobles del servicio para verificar rutas de lectura,
pasos de hardware, límites, mute nativo, alternativa master y restauración. No
confirman la compatibilidad física de cada salida de audio o versión de firmware.


## Corrección de inicio de juegos: 1.12.1-fix2

El sysmodule reemplaza `__appInit`, por lo que debe inicializar explícitamente la
versión de firmware que libnx usa para seleccionar los comandos IPC. Se añade
esa inicialización antes de crear sesiones de servicios, también en el overlay.
Si la versión sigue siendo desconocida, el componente queda inactivo y la consulta
de juegos devuelve un error sin abrir `pm:dmnt`.

En libnx, `pmdmntGetApplicationProcessId` usa el comando 4 desde firmware 5.0.0 y
el comando 5 en firmware anterior. Con una versión sin inicializar, puede escoger
el comando 5; en firmware moderno ese comando es `HookToCreateApplicationProcess`.
Por eso no se debe consultar el proceso con una versión cero ni inventar una
versión por defecto. La relación con el fallo observado desde HOME requiere
confirmación en consola tras instalar el arreglo y reiniciar.

Instalar `1.12.1-fix2` siguiendo los pasos de instalación anteriores y reiniciar
completamente la consola antes de probar el inicio desde HOME. Si vuelve a fallar,
el código/Program ID de la pantalla de error permite distinguir sysmodule,
overlay u otro componente. Los selectores de lanzamiento desde el sysmodule
siguen deshabilitados.


## Nombre de juego y Close Game: 1.12.1-fix3

Se regenera el índice binario a partir de los bytes reales de `titles.txt` y se
impide la conversión automática de finales de línea de ese catálogo en Git.
Actions regenera el índice antes de comprobar y compilar los archivos incrustados.
El lector del catálogo recupera el nombre mediante una búsqueda en texto si una
entrada instalada apunta a una posición incorrecta. Las pruebas verifican todos
los offsets y el caso `01006A800016E000` → `Super Smash Bros.™ Ultimate`, tanto
con un índice correcto como con un índice generado con CRLF para un archivo LF.

Close Game usa ahora el PID devuelto por la consulta de aplicación actual, sin
depender del nombre, del índice ni de recorrer procesos. Si no hay aplicación o
falla la consulta, no envía ninguna orden de terminación. El resultado real de
`pmshellTerminateProcess` se publica en `command_status`; no se añade una llamada
privilegiada de terminación como alternativa.

El sensor Title ID ya reportaba el identificador correcto; no se cambia su
formato. Instalar el NRO nuevo abriendo Switch Assistant y reiniciar para actualizar
también el catálogo incrustado y el sysmodule. Guardar la partida antes de probar
Close Game, porque termina la aplicación.


## Cierre forzado retirado: 1.12.1-fix4

La llamada `pmshellTerminateProcess` de fix3 terminaba el proceso, lo que Horizon
puede presentar como «el programa se cerró a causa de un error». No equivalía a
solicitar una salida normal. Se retira ese código y el botón Close Game; su
configuración MQTT retenida se borra cuando el sysmodule nuevo reconecta.
Los comandos `close_game`, `close` y `exit_game`, tanto generales como en el
namespace de controles, responden `close_game:disabled_unsafe_termination` y no
llaman a ninguna API de terminación. El cierre normal desde el sysmodule queda
pendiente de un mecanismo compatible; no se oculta ni modifica el error de Horizon.

Instalar el NRO fix4 abriendo Switch Assistant y reiniciar. Los cambios de fix3
para nombres de juegos y el índice del catálogo se conservan. El apartado de
fix3 documenta el comportamiento anterior, sustituido por esta corrección.


## Suspensión y últimas lecturas: 1.12.1-fix5 (retirado)

**El listener PSC de esta versión fue retirado en fix6 tras el fallo reportado al
suspender. Esta sección documenta el cambio anterior; no debe reintroducirse.**

El detector anterior infería la suspensión a partir de la retroiluminación y no
podía distinguir correctamente una pantalla apagada de la suspensión real.
Se sustituye por un listener de notificaciones de energía `psc:m`, con un ID
privado `0xE3` y dependencias de sockets, red y filesystem. Solo los eventos de
preparación de suspensión cambian `power_sleeping`; apagar Screen no lo hace.
No se añade ninguna llamada para provocar la suspensión ni se vuelve a habilitar
el botón Sleep.

Antes de confirmar la preparación de suspensión a Horizon, el hilo MQTT:

1. Detiene las consultas de sensores para conservar las últimas lecturas.
2. Publica el estado de conexión `sleep` y disponibilidad de controles `offline`.
3. Publica `operation_mode=Sleep` con retención y QoS 1.
4. Espera brevemente el PUBACK de ese paquete y envía MQTT DISCONNECT.

El listener de energía espera como máximo 500 ms al hilo MQTT antes de confirmar
un evento leído correctamente; si falla la lectura o confirmación, se desregistra.
Un broker sin respuesta no debe bloquear la suspensión del sistema.
La publicación solo puede llegar si la conexión y el broker están operativos.
Si falla el listener, se desregistra y no se inventa un estado de suspensión.
`Power event listener` indica `ready` o `unavailable`, y el heartbeat registra
`power_events_ready`. Al reanudarse la consola se reconecta y publica todas las
lecturas otra vez.

Las pruebas ejercitan los constructores de discovery sin disponibilidad en
telemetría, la conservación de disponibilidad en controles, los eventos de energía,
el límite de espera y desregistro en caso de error. Un peer MQTT local verifica
los paquetes retenidos, el PUBACK exacto, DISCONNECT y que una desconexión ordinaria
no sobrescriba el modo ni los valores de batería/juego/audio. La recepción de eventos
reales y el ciclo de suspensión/reanudación necesitan validación en consola.


## Recuperación de suspensión: 1.12.1-fix6

La pantalla reportada muestra `0100000000000034`, el servicio fatal de Atmosphere,
y `std::abort() called`. La foto y el intervalo aproximado de 66 segundos no bastan
para identificar el primer error; el reporte `.bin` permitiría investigarlo.
El listener PSC añadido en fix5 tenía otras consultas IPC en el mismo hilo. Una
consulta de pantalla bloqueada antes de leer el evento puede impedir confirmar
la suspensión, aunque la espera MQTT posterior tenga un límite de 500 ms.

Para aislar esta regresión se retira completamente la participación en `psc:m`,
las dependencias de energía y sus confirmaciones. El hilo de retroiluminación
vuelve a ser un observador pasivo, sin obligaciones ante el gestor de energía.
Se conservan los sensores retenidos y las correcciones anteriores. El antiguo
sensor `Power event listener` se elimina mediante un payload de discovery vacío.

El modo Sleep pasa a ser una inferencia pasiva del estado MQTT, como se describe
arriba. Se confirma mediante pruebas el JSON del último testamento, su retención,
el keepalive y la publicación de estado vivo al reconectar; el programa ya no
incluye llamadas de registro/confirmación PSC. No se afirma haber reproducido ni
resuelto físicamente el fallo de la consola con pruebas de host.

Instalar el NRO abriendo Switch Assistant y reiniciar completamente, para retirar
el registro PSC del proceso anterior. Validar suspensión por al menos tres minutos
y reanudación. Si el fallo persiste, adjuntar el `.bin` correspondiente de
`sdmc:/atmosphere/fatal_errors/` para identificar la llamada que aborta.
