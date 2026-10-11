# MPC Melody Maker â€” fases 8 y 9

ImplementaciÃ³n preparada el 11 de octubre de 2026. La comprobaciÃ³n de uso y escucha queda para la sesiÃ³n del usuario, despuÃ©s de instalar ambas fases. Esta guÃ­a no acredita que el incidente de recursos estÃ© resuelto.

## Flujo principal

1. Usa una pista de instrumento con **MPC Melody Maker**. El plugin genera MIDI; necesita instrumentos receptores para producir sonido.
2. En **CREATE** selecciona KEY, SCALE y SPEED. Esta versiÃ³n trabaja con frases de **cuatro compases**; BARS queda fijado en 4.
3. Pulsa un pad. Cada toque crea una base nueva con melodÃ­a, acordes y bajo; el visor de ancho completo muestra las notas publicadas. La generaciÃ³n ocurre en segundo plano y puede tardar unos segundos.
4. Enciende o apaga MELODY, CHORDS y BASS para escuchar las partes que quieras. POWER silencia la reproducciÃ³n del plugin. PLAY/STOP se toman del transporte de la MPC.
5. SAVE guarda la creaciÃ³n. Toca una fila del banco para recuperarla. STAR cambia su favorito y FAVS filtra los favoritos. El scroll es tÃ¡ctil y tambiÃ©n seleccionable para el jogwheel.

Los parÃ¡metros muestran su selecciÃ³n con un marco rojo en la caja de valor; despuÃ©s de seleccionarlos puedes usar el jogwheel.

## VARIATIONS

- PAD elige la creaciÃ³n que quieres trabajar. Seleccionar un pad aquÃ­ no crea otra base.
- INTENSITY regula la cantidad de cambio. Cero conserva la frase cuando no cambias tonalidad ni escala.
- VARIATION elige Light/Dark, Rhythm o Masked.
- LIGHT / DARK controla el carÃ¡cter del operador de alturas. Rhythm conserva las alturas y cambia su ritmo.
- CANDIDATES permite 8â€“32 candidatos.
- VARY transforma la creaciÃ³n actual. NEW crea una base distinta. SAVE la guarda.
- BACK y FORWARD recorren hasta ocho creaciones del pad durante la sesiÃ³n. Generar desde una posiciÃ³n anterior abre una nueva continuaciÃ³n del historial.
- FIX PAD evita nuevas generaciones y variaciones por toque sobre ese pad hasta desactivarlo. Puedes seguir escuchÃ¡ndolo y guardÃ¡ndolo.

El proyecto conserva la creaciÃ³n actual de cada pad, su fijaciÃ³n y los contadores de semilla. El historial de ocho entradas es de sesiÃ³n y no se serializa completo. El banco guarda tambiÃ©n tipo de variaciÃ³n, carÃ¡cter, candidatos y cadena de efectos.

Rhythm exige velocidad, longitud y groove compatibles. Si no existe un patrÃ³n, conserva la creaciÃ³n y muestra NO_COMPATIBLE_RHYTHM; no toma ritmos de otra velocidad.

## MIDI FX

Una pÃ¡gina presenta tres inserts de la cadena. BACK/NEXT cambia de grupo y el indicador PAGE muestra el grupo actual y los disponibles. Se habilita otro grupo vacÃ­o al utilizar el anterior. ADD FX aÃ±ade el siguiente insert disponible; despuÃ©s selecciona su efecto.

Hay **12 inserts** en orden. Cada uno tiene ON/OFF, EFFECT, AMOUNT y TARGET (todas las partes, melodÃ­a, acordes o bajo). Un insert encendido con EFFECT = Off no transforma las notas.

Efectos: Swing, Gate, Velocity, Humanize, Repeat, Arp up, Arp down, Bass motif, Probability, Strum y Accent. Los arpegiadores y Strum actÃºan sobre acordes; Bass motif sobre bajo. Los demÃ¡s respetan TARGET. Apagar un insert recupera la frase sin esa transformaciÃ³n; no vuelve a generar la base.

## MIDI hacia otras pistas o equipos

Conserva el enrutamiento de tres pistas receptoras que ya funcionaba en tu proyecto. En **SETTINGS** cada parte tiene su propio canal, destino y puerto. Los valores de destino/puerto muestran el nÃºmero y su nombre ALSA cuando estÃ¡ disponible.

- **SUBSCRIBERS** mantiene las conexiones de suscripciÃ³n del puerto del plugin.
- **DISABLED** desactiva el envÃ­o de esa parte a ALSA.
- Un destino numÃ©rico con nombre envÃ­a al cliente y puerto seleccionados.
- **UNAVAILABLE** indica que esa direcciÃ³n no aparece actualmente en el sistema.

Los valores iniciales de canal siguen siendo 2, 3 y 4 para conservar tu configuraciÃ³n. Las pistas receptoras deben usar el canal correspondiente y el modo de monitorizaciÃ³n que ya tenÃ­as funcionando. Seleccionar una pista receptora no deberÃ­a ser necesario para escucharla si el enrutamiento y monitorizaciÃ³n del proyecto estÃ¡n bien configurados.

## EXPORT y piano roll

La pantalla **EXPORT** tiene botones MELODY, CHORDS, BASS y ALL 3. Cada botÃ³n exporta directamente la parte indicada. ALL 3 escribe el archivo combinado y, ademÃ¡s, un archivo independiente por parte.

Archivos en la MPC:

`private/exports/` dentro de la carpeta de instalación (o `TTMM_EXPORT_DIR`).

Nombres: `pad-01-melody.mid`, `pad-01-chords.mid`, `pad-01-bass.mid` y `pad-01-all.mid`, cambiando el nÃºmero segÃºn el pad.

Para llevarlos al piano roll, importa el archivo MIDI en la pista de destino desde el navegador de la MPC. Para tres pistas, importa cada archivo de parte en su pista correspondiente. El botÃ³n MIDI de CREATE usa la selecciÃ³n PARTS de EXPORT/SETTINGS.

**La inserciÃ³n automÃ¡tica en el piano roll propio o en otras pistas no estÃ¡ implementada.** Se exportan archivos; no se presenta esa operaciÃ³n como una copia automÃ¡tica al secuenciador.

## Alcance y recuperaciÃ³n

El motor conserva la biblioteca y el modelo principal de las Partes 1â€“8 mejorado. No se ha sustituido por Composition Next ni incorporado una red neuronal sin aprobaciÃ³n. Las 22 escalas proyectan la salida del motor, incluidos acordes y bajo; no equivalen a haber entrenado corpus nuevos de todos esos estilos.

La compilaciÃ³n ARM y la construcciÃ³n de las seis pantallas estÃ¡n realizadas. La escucha, comportamiento de los controles y aprobaciÃ³n de la experiencia en la MPC quedan pendientes de tu revisiÃ³n.

El paquete **PERSONAL** contiene datos musicales privados. No subirlo a GitHub ni distribuirlo. El cÃ³digo pÃºblico preparado estÃ¡ separado de esos datos. La protecciÃ³n de distribuciÃ³n de los nuevos assets no se considera terminada.

Antes de instalar, conserva una copia de tu instalación y guarda el proyecto de la MPC.
