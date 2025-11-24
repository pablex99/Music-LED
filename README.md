# Music-LED

Control de tira RGB con ESP32 — servidor web, modos: Manual / Música / Arcoíris.

Contenido
- `src/` : firmware (sketch `main.ino`).
- `data/` : archivos servidos por SPIFFS (HTML/CSS/JS/iro.min.js).
- `platformio.ini` : configuración de PlatformIO.

Quick start
1. Copia `src/secrets.example.h` a `src/secrets.h` y rellena tu `WIFI_SSID`/`WIFI_PASS`.
2. Instala PlatformIO (VSCode + PlatformIO extension) o usa `pip install -U platformio`.
3. Construye y sube la imagen SPIFFS (desde la raíz del proyecto):
   ```powershell
   pio run -t buildfs -e esp32dev
   pio run -t uploadfs -e esp32dev --upload-port COM9
   ```
4. Compila y sube el firmware:
   ```powershell
   pio run -t upload -e esp32dev --upload-port COM9
   ```

Seguridad / Secrets
- `src/secrets.h` está listado en `.gitignore`. Nunca comites tus credenciales.
- `src/secrets.example.h` sirve como plantilla para colaborar.

Versionado (Git)
- El repositorio ya incluye `.gitattributes` y `.gitignore` para mantener consistencia de finales de línea y evitar artefactos de build.
- Flujo recomendado: crea ramas `feat/*` para nuevas características y abre Pull Requests hacia `main`.
- Para marcar releases: `git tag -a v1.0 -m "Stable"`.

CI
- Hay una configuración de GitHub Actions en `.github/workflows/ci.yml` que compila con PlatformIO en cada push/PR.

Notas
- Mantén la tira LED alimentada por fuente externa y comparte masa (GND) con el ESP32.
- Si quieres que suba automáticamente archivos o prepare releases, dime y lo configuro.
