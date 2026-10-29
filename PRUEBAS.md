# Pruebas reales de 4EVERHUM (antes del Sprint 3)

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

## 5. Lo que falta (Sprint 3, no es un error)

- Las notas no se ajustan a una escala ni al grid: si tarareas un poco desafinado
  o adelantado, la nota sale así. Eso es lo que resuelve el próximo sprint.
