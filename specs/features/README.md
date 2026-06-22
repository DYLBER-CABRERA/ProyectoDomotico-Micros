# features/ — Specs por módulo (documentación viva)

Una spec por módulo **implementado**. Describen el comportamiento real del código (no el
deseado), y deben actualizarse en el mismo commit que cambie la API o el comportamiento.

| Módulo | Spec | Fase | Estado |
|--------|------|------|--------|
| LCD | [`lcd.md`](lcd.md) | 1 | ✅ |
| Teclado | [`teclado.md`](teclado.md) | 2 | ✅ |
| USART | [`usart.md`](usart.md) | 3 | ✅ |
| Alarma | [`alarma.md`](alarma.md) | 5 | 🟡 |
| Motor garaje | [`motor.md`](motor.md) | 6a | 🟡 |
| Temperatura | [`temperatura.md`](temperatura.md) | 6b | ✅ |
| Dimmer | [`dimmer.md`](dimmer.md) | 6c | ✅ |
| Horno | [`horno.md`](horno.md) | 7a | 🐞 |
| Sonido | [`sonido.md`](sonido.md) | 7b | ✅ |
| Mercado | [`mercado.md`](mercado.md) | 7c | ✅ |

Los módulos pendientes (RFID, acceso, SPI, EEPROM de UIDs) se especifican en
[`../fase-04-rfid-acceso/`](../fase-04-rfid-acceso/) hasta que se implementen; al
completarse se crearán sus specs aquí.
