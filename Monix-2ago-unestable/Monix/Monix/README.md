# CSOP

CSOP es un MVP de una plataforma de observabilidad para Windows con una sola ventana, telemetria pasiva y una superficie central con estetica CRT.

## Incluye

- Telemetria de CPU, RAM, disco, GPU y red del sistema.
- Observacion de procesos con prioridad, estado, consumo y actividad de IO.
- Registro unificado de eventos sintetizados y eventos recientes de Windows.
- Paneles internos `LOG`, `TASKS`, `USAGE`, `NETWORK`, `S.C.R.A.M` y `SETTINGS`.
- Motor de analisis pasivo basado en reglas para detectar anomalias y explicar el estado del sistema.
- Superficie visual con shader CRT, scanlines, distorsion, glow, flicker, aberracion cromatica y persistencia.

## Arquitectura

- `src/main`: proceso principal de Electron, IPC y colectores Windows.
- `src/renderer`: UI, charts y superficie CRT WebGL.
- `SettingsStore`: persistencia local de configuracion en `userData`.
- `TelemetryHub`: pipeline `Windows APIs -> log engine -> analysis engine -> renderer`.

## Ejecucion

```bash
npm install
npm start
```

## Limitaciones actuales

- La observacion de conexiones por proceso esta implementada.
- El ancho de banda exacto por proceso no lo expone Windows de forma trivial sin ETW/WFP o un driver adicional, por eso la tabla de red marca ese campo como no disponible.
- El motor de IA es heuristico y local; no ejecuta acciones ni modifica el sistema.
