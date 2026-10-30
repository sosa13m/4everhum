# Cambios

## 1.0.0

Primera versión completa de **4EVERHUM**: tarareas, el plugin detecta la melodía
y la arrastras al piano roll de cualquier instrumento.

**Detección**
- Detección de altura con **pYIN** (modo Alta precisión, activado por defecto):
  sigue la melodía en el tiempo y evita saltos de octava. YIN clásico disponible.
- Perillas **Sensibilidad**, **Nota mínima** y **Estabilidad** con re-análisis
  instantáneo sobre la última toma, sin volver a grabar.
- **Filtro de ruido** automático (ignora, por ejemplo, el beat que se filtra por
  los audífonos).

**Música**
- **Tonalidad** detectada automáticamente, con código **Camelot** (o elegida a mano).
- **Escala**: las notas desafinadas caen en la nota correcta (mayor, menor, menor
  armónica, dórica, frigia, pentatónicas o sin escala).
- **Cuantizar** de 1/4 a 1/32 y tresillos, con **Fuerza** como el quantize de FL.
- **Octava** ±2.
- Grabando con el proyecto sonando, las notas quedan **alineadas a su compás**.

**Edición**
- Piano roll editable: mover, alargar, agregar, borrar, deshacer y rehacer.
- **Arrastrar al piano roll** desde un asa dedicada; el `.mid` lleva la
  tonalidad en el nombre.
- Las ediciones se guardan con el proyecto; **Automático** vuelve a lo detectado.

**Diseño**
- 5 temas de 4EVERTOOLS: **SÓTANO 4EVER**, **LINEA 4 EVER**, **4EVERTALE**,
  **LIBRETA NEGRA** y **4EVERPALOOZA**, con sus logos y tipografías.
- Ventana redimensionable.

**Extras**
- `hum2midi`: el mismo motor desde la terminal (WAV, AIFF, FLAC, MP3).
- Guardar la toma en `.wav` y el resultado en `.mid`.
- Instalador para Windows (`instalar.bat`) y desinstalador.
