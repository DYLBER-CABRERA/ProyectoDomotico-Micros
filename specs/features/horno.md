# Feature — Horno remoto

- **Fase**: 7a · **Estado**: ✅ (DEF-001 corregido) · **Archivos**: `horno.cpp`, `horno.h`

## Objetivo
Encender el horno de forma remota indicando temperatura objetivo y tiempo, con apagado
automático al terminar.

## Pin
LED rele del horno en **PH5 (D8)**, vía las macros `HORNO_DDR`/`HORNO_PORT`. (DEF-001
corregido: antes el `.cpp` manejaba PE5 e invadía la alarma de incendio.)

## API
`horno_init()`, `horno_encender(temp,minutos)`, `horno_apagar()`, `horno_actualizar()`
(1 al terminar la cuenta), `horno_estado()`, `horno_segundos_restantes()`,
`horno_temp_objetivo()`. Comando serial `HORNO:temp,min`.

## Comportamiento
`horno_encender` guarda la temperatura objetivo, convierte minutos a segundos y enciende
el LED. La cuenta regresiva avanza por conteo de vueltas (`VUELTAS_POR_SEGUNDO=4000`,
calibrable) en `horno_actualizar()`, llamado desde `loop()`. Al llegar a 0 apaga y reporta
`HORNO:FIN`.

## Defecto relacionado
✅ **DEF-001 corregido**: el módulo usa PH5 mediante las macros del header. Verificar al
compilar que encender el horno ya no afecta la alarma de incendio zona 2 (INT5).

## Criterios de aceptación
`HORNO:180,25` responde `OK:HORNO,180,25`, enciende el LED en D8/PH5 y al expirar envía
`HORNO:FIN`, sin afectar la alarma de incendio.
