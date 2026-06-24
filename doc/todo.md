# RF
[X] Cuando la alarma está armada y se abre la puerta principal o la del garaje, esta espera 10 segundos antes de encenderse
[~] Cuando la alarma está armada y se intenta abrir una ventana, esta se enciende
[V] La alarma se dispara cuando hay un incendio, independientemente de si está armada.
[V] La alarma envía una señal por USART para reportar intrusos e incendios de manera remota
[V] El armado y desarmado es permitido solo con código de seguridad
[ ] Se puede realizar la gestión de RFIDs, y esta es permitida solo con código de seguridad
[~] Si está armada, la alarma se activa luego de un número de intentos fallidos
[ ] Las puertas y ventanas se acceden por RFID
[X] Hay un LED que representa si la puerta principal está abierta o no
[V] Hay un LED que representa qué tan abierta está la puerta del garaje
[ ] Los RFIDs de hijos tienen un número de ingresos que se decrementa por acceso a la sala de juegos y se actualiza con un código de seguridad
[V] Se controla la iluminación de manera dimerizada por teclado
[V] Se actualiza en tiempo real la temperatura leída por ADC
[V] Se enciende el calefactor cuando se baja de 18 °C y se apaga cuando se sube de 20 °C (Histéresis)
[V] Se enciende el ventilador cuando se sube de 26 °C y se apaga cuando se baja de 24 °C (Histéresis)
[~] Se envía de manera remota, por USART, la instrucción de encender el horno con una temperatura (en °C) y tiempo (en segundos) determinados
[V] Se envía de manera remota, por USART, la instrucción de encender el equipo de sonido, o de controlar su volumen
[X] Hay una opción para visualizar los IDs de los productos de la lista de mercado 
[V] En la lista de mercado, se sobreescribe el número de ejemplares a comprar por producto, cada vez que se ingrese un valor nuevo por teclado
[X] En la lista de mercado, hay una opción con validación para refrescarla (poner cero ejemplares para todos los productos) por teclado
[V] La lista se puede consultar de manera remota por USART
[V] La lista se guarda en memoria no volátil
[ ] Los RFIDs registrados se guardan en memoria no volátil
[X] El código de seguridad se puede modificar al saberlo

# RNF
[ ] No se utiliza librerías que involucren polling ni la librería para servomotor
[ ] Se utilizan interrupciones cada vez que es posible, y las rutinas asociadas se mantienen cortas utilizando banderas de estado