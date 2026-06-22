# specs/ — Especificaciones (Spec-Driven Development)

Esta carpeta es la **fuente de verdad del diseño** del sistema domótico. El código debe
ajustarse a estas specs; cuando cambien los requisitos, se actualiza primero la spec y
luego el código.

## Índice

| Archivo | Propósito |
|---|---|
| [`constitution.md`](constitution.md) | Principios y reglas no negociables del proyecto. **Leer primero.** |
| [`001-requisitos.md`](001-requisitos.md) | Requisitos derivados del enunciado + matriz de trazabilidad. |
| [`002-estado-y-roadmap.md`](002-estado-y-roadmap.md) | Qué está hecho, qué falta, defectos y plan por fases. |
| [`003-arquitectura.md`](003-arquitectura.md) | Arquitectura, mapa de pines, timers, ISR, memoria EEPROM, protocolo serial. |
| [`features/`](features/) | Una spec por módulo implementado (documentación viva del comportamiento real). |
| [`fase-04-rfid-acceso/`](fase-04-rfid-acceso/) | Spec + plan + tasks de la fase pendiente (RFID/acceso). |
| [`defects/`](defects/) | Defectos abiertos con su análisis y plan de corrección. |

## Cómo usar estas specs (para humanos y agentes)

1. Antes de programar, leer `constitution.md` y la spec del área a tocar.
2. Cada spec sigue el formato: **Objetivo → Requisitos → Diseño/API → Pines/registros →
   Comportamiento → Criterios de aceptación**.
3. Al implementar, marcar en `002-estado-y-roadmap.md` el avance y cerrar defectos en
   `defects/`.
4. Para la fase pendiente, trabajar en el orden de `fase-04-rfid-acceso/tasks.md`.

## Convención de estados

✅ Implementado y verificado · 🟡 Implementado con observaciones/defecto ·
⏳ Pendiente · 🐞 Defecto abierto
