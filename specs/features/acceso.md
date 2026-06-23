# Acceso (Fase 4)

## Dependencias
- `motor` (garaje por servo, Timer4/OC4A/PH3)

## Recursos de hardware

| Elemento | Pin | Puerto | Descripción |
|----------|-----|--------|-------------|
| Imán/relé puerta principal | D41 | PG0 | Salida digital. 1 = abierto, 0 = cerrado |
| Garaje | D6 | PH3 (OC4A) | Servo por PWM Timer4 (delegado en `motor.cpp`) |

## API

```c
void acceso_init();
void acceso_abrir_principal();
void acceso_cerrar_principal();
void acceso_abrir_garaje();
void acceso_cerrar_garaje();
void acceso_concedido_adulto();
void acceso_concedido_hijo();
void acceso_denegado();
```

## Comportamiento

- `acceso_init()`: configura PG0 como salida, inicialmente en 0 (puerta cerrada).
- `acceso_abrir_principal()`: PG0 = 1 (activa imán, abre puerta).
- `acceso_cerrar_principal()`: PG0 = 0 (desactiva imán, cierra puerta).
- `acceso_abrir_garaje()`: `garaje_abrir()` → servo a posición abierto.
- `acceso_cerrar_garaje()`: `garaje_cerrar()` → servo a posición cerrado.
- `acceso_concedido_adulto()`: abre puerta principal (imán).
- `acceso_concedido_hijo()`: abre puerta principal (imán) — el control de cupos lo
  maneja `rfid.cpp`.
- `acceso_denegado()`: no realiza acción física (el mensaje serial lo envía `rfid.cpp`).

## Notas

- La puerta se cierra automáticamente cuando se retira la tarjeta (lógica en
  `rfid_verificar()` estado `PROCESANDO` → llama a `acceso_cerrar_principal()`).
- El garaje no se cierra automáticamente; se controla con `GARAJE:CERRAR` o `*10#`.
