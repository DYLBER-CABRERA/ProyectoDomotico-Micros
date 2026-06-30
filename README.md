# ProyectoDomotico — Sistema domótico (ATmega2560)

Firmware de un sistema domótico sobre **Arduino Mega 2560**, escrito en **registros
directos del AVR, sin librerías de Arduino**. Proyecto de Microprocesadores 2026-1.
Autores: **Dylber Cabrera**, **Juan David Ocampo**, **Alejandro Martínez**.

Controla seguridad (alarma de acceso + incendio + anti-intrusos, acceso por RFID con
3 lugares y contador de la sala de juegos) y confort (luz dimerizable, temperatura con
calefactor/ventilador, garaje por servo, horno, equipo de sonido y lista de mercado).

## Documentación

| Para... | Lee |
|---------|-----|
| **Modificar el código (agentes de IA y humanos)** | **[AGENTS.md](AGENTS.md)** ← empieza aquí |
| Entender el proyecto de forma didáctica | [GUIA_RAFA.md](GUIA_RAFA.md) |
| Comandos (teclado y serial) | [doc/COMANDOS_TECLADO.md](doc/COMANDOS_TECLADO.md) |
| Mapa de pines | [doc/MAPEO_PINES.md](doc/MAPEO_PINES.md) |
| Especificaciones / diseño (SDD) | [specs/](specs/) |

## Compilar y subir

Arduino IDE 2.x → abrir `ProyectoDomotico.ino` (toma `src/` e `include/` solo).
Placa: *Arduino Mega or Mega 2560*, procesador *ATmega2560*. Subir por el puerto COM
correspondiente. Serial a **9600 8N1**.

> La carpeta `build/` (`.hex`, `.elf`) son artefactos de compilación y están en
> `.gitignore` — no se versionan.
