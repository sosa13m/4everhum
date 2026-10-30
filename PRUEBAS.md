# Pruebas reales de 4EVERHUM

Marca cada punto con `[x]` y anota lo raro al lado. Si una detección sale mal,
pulsa **Guardar toma .wav…** y guarda el archivo: con esa toma se puede ajustar
el motor con datos reales.

## 1. Standalone (sin FL Studio)

- [ ] `4EVERHUM.exe` abre y se ve el diseño SÓTANO (logo, cartel rojo, hoja con cinta)
- [ ] En **Options → Audio/MIDI Settings** elegí mi micrófono como entrada
- [ ] Desmarqué **Mute audio input** y el medidor MIC se mueve al hablar
- [ ] Grabar → tararear 4–6 notas → Detener: aparecen notas en la hoja
- [ ] Las notas coinciden con lo que tarareé (comparar con un piano o una app afinadora)

## 2. FL Studio

- [ ] FL encuentra **4EVERHUM** al escanear plugins
- [ ] El plugin carga en un slot del mixer con el micrófono como entrada
- [ ] El tempo del cartel coincide con el del proyecto
- [ ] Grabando con el beat sonando (con audífonos) las notas caen a tiempo
- [ ] **Arrastrar** la melodía al piano roll del canal de Purity funciona
- [ ] Las notas en el piano roll de FL tienen los mismos nombres que en el plugin (C5 = Do central)
- [ ] Guardar el proyecto, cerrarlo y abrirlo: las notas y las perillas siguen ahí

## 3. Perillas (sobre la misma toma, sin regrabar)

- [ ] **Sensibilidad** cambia cuántas notas salen
- [ ] **Nota mínima** quita notas cortitas sueltas
- [ ] **Estabilidad** junta una nota larga que salía partida
- [ ] **Octava** sube/baja la melodía y el arrastre sale en la octava elegida
- [ ] **Filtro de ruido** quita notas graves fantasma si el beat se filtra al micrófono

## 4. Casos difíciles (anotar cómo le fue)

| Caso | ¿Bien? | Notas |
|---|---|---|
| Melodía lenta con notas largas | | |
| Melodía rápida (trap/drill) | | |
| Notas repetidas ("da da da") | | |
| Voz muy grave | | |
| Silbido | | |
| Con ruido de fondo (ventilador, calle) | | |

## 5. Tonalidad, escala y grid (Sprint 3)

- [ ] El sello rojo muestra una tonalidad que tiene sentido para lo que tarareé
- [ ] Con un beat en tonalidad conocida, elegirla en **Tonalidad** ajusta bien las notas
- [ ] Una nota cantada un poco desafinada cae en la nota correcta de la escala
- [ ] **Sin escala** devuelve las alturas tal como las canté
- [ ] **Cuantizar 1/16** al 100%: las notas quedan en el grid del piano roll de FL
- [ ] **Fuerza** al 50%: las notas se acercan al grid pero conservan algo de groove
- [ ] Tresillos (**1/8T**) con un ritmo tresillado
- [ ] Grabando **con FL sonando**: el estado dice "alineadas desde el compás N" y al
      soltar el clip al inicio de ese compás cada nota cae donde la canté
- [ ] Grabando con FL detenido: la primera nota cae en el tiempo 1
- [ ] El `.mid` arrastrado se llama `4EVERHUM_<tonalidad>_<bpm>bpm_...`

## 6. Edición de notas (Sprint 4)

- [ ] Clic en una nota la marca; arrastrarla la mueve en tiempo (pegada al grid) y en altura
- [ ] Arrastrar el borde derecho alarga o acorta la nota
- [ ] Doble clic en un espacio vacío agrega una nota (cae en la escala)
- [ ] Clic derecho o Supr borra la nota
- [ ] DESHACER / REHACER (y Ctrl+Z / Ctrl+Y) funcionan
- [ ] El sello dice EDITADO A MANO y AUTOMÁTICO vuelve a la melodía detectada
- [ ] Al arrastrar con el asa ARRASTRAR AL PIANO ROLL, FL recibe la versión editada
- [ ] Guardar el proyecto de FL, cerrarlo y abrirlo: la edición sigue ahí
- [ ] La ventana se puede agrandar desde la esquina y el piano roll crece

## 7. Alta precisión (Sprint 5)

- [ ] Con **Alta precisión** activada (por defecto) una voz grave o rasposa no salta de octava
- [ ] Desactivarla y volver a activarla re-analiza la toma al instante
- [ ] Una toma larga (1–2 minutos) se analiza en pocos segundos

## 8. Temas (Sprint 6)

- [ ] El selector de arriba a la derecha cambia entre los 5 temas al instante
- [ ] Cada tema muestra su logo y sus tipografías, y todo se lee bien
- [ ] Al cerrar y volver a abrir el plugin, se mantiene el tema elegido

## 9. Instalador (1.0.0)

- [ ] `instalar.bat` pide administrador una vez y termina con "Listo."
- [ ] FL encuentra 4EVERHUM después de *Find plugins*
- [ ] "4EVERHUM" aparece en el menú Inicio y abre la app sola
- [ ] En una terminal nueva, `hum2midi --ayuda` funciona
- [ ] Reinstalar encima (con FL cerrado) reemplaza la versión anterior
- [ ] `desinstalar.bat` quita el plugin, la app, el acceso directo y la entrada del PATH
