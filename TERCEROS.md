# Licencias de 4EVERHUM y de terceros

## 4EVERHUM

Copyright (C) 2026 sosa13m / colectivo 4EVER.

El código de 4EVERHUM es software libre bajo la **GNU Affero General Public
License v3.0 o posterior** (archivo [`LICENSE`](LICENSE)). Puedes usarlo,
estudiarlo, modificarlo y compartirlo. Si distribuyes una versión modificada,
tienes que publicar su código bajo la misma licencia.

Se distribuye **sin ninguna garantía**.

El nombre **4EVER**, **4EVERHUM** y los **logos del colectivo 4EVER**
(`Resources/images/`) son de ese colectivo: no forman parte de la licencia AGPL.
Si publicas una versión modificada, usa otro nombre y otros logos.

## JUCE 8

4EVERHUM está hecho con el framework [JUCE](https://juce.com), que se usa aquí
bajo su licencia de código abierto **AGPLv3**.
Código fuente de JUCE: https://github.com/juce-framework/JUCE

## Tipografías (van embebidas en el plugin)

| Tipografía | Licencia | Archivo |
|---|---|---|
| Anton | SIL Open Font License 1.1 | `Resources/fonts/Anton-OFL.txt` |
| Space Mono | SIL Open Font License 1.1 | `Resources/fonts/SpaceMono-OFL.txt` |
| Special Elite | Apache License 2.0 | `Resources/fonts/SpecialElite-LICENSE.txt` |
| Sedgwick Ave Display | SIL Open Font License 1.1 | `Resources/fonts/sedgwickavedisplay-OFL.txt` |
| Bungee | SIL Open Font License 1.1 | `Resources/fonts/bungee-OFL.txt` |
| Inter | SIL Open Font License 1.1 | `Resources/fonts/inter-OFL.txt` |
| Pixelify Sans | SIL Open Font License 1.1 | `Resources/fonts/pixelifysans-OFL.txt` |
| Press Start 2P | SIL Open Font License 1.1 | `Resources/fonts/pressstart2p-OFL.txt` |
| Permanent Marker | Apache License 2.0 | `Resources/fonts/PermanentMarker-LICENSE.txt` |
| Caveat | SIL Open Font License 1.1 | `Resources/fonts/caveat-OFL.txt` |
| Archivo | SIL Open Font License 1.1 | `Resources/fonts/archivo-OFL.txt` |
| Big Shoulders Display | SIL Open Font License 1.1 | `Resources/fonts/bigshouldersdisplay-OFL.txt` |

Las tipografías variables se instanciaron al peso que usa 4EVERTOOLS y se
recortaron a caracteres latinos. Press Start 2P va sin modificar por su
*Reserved Font Name*. Todas se obtuvieron del repositorio oficial de Google
Fonts (https://github.com/google/fonts).

## Algoritmos

- **YIN**: A. de Cheveigné y H. Kawahara, "YIN, a fundamental frequency estimator
  for speech and music", JASA, 2002.
- **pYIN**: M. Mauch y S. Dixon, "pYIN: A fundamental frequency estimator using
  probabilistic threshold distributions", ICASSP, 2014.
- **Detección de tonalidad**: perfiles de Krumhansl y Kessler (1982).

Las implementaciones de 4EVERHUM son propias; no incluyen código de esos autores.
