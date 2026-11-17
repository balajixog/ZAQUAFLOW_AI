# ZAQUAFLOW Local AI Starter

Stack: React/Vite, Spring Boot 3 + Java 21, FastAPI, Ollama, PostgreSQL, Mosquitto, Docker Compose.

## Run on Windows
1. Install Docker Desktop and Ollama.
2. Run `ollama pull qwen2.5:3b` then `ollama serve`.
3. Copy `.env.example` to `.env` and change passwords.
4. Run `docker compose up --build`.
5. Open React at http://localhost:5173, Spring API at http://localhost:8080,
   AI API docs at http://localhost:8000/docs.

Ollama runs on the Windows host so it can use your RTX 3050 GPU. Containers reach
it through `host.docker.internal:11434`. Change `OLLAMA_MODEL` in `.env` as needed.

## Included
- React dashboard and AI chat UI
- FastAPI local agent that calls Ollama's local chat API
- Pump ON/OFF proposals require explicit confirmation in the UI
- Spring Boot records and publishes approved commands to Mosquitto
- PostgreSQL device, telemetry and command tables
- MQTT topic contract and ESP32 safety requirements

## Module-by-module implementation status

### Module 01 — MQTT telemetry and acknowledgement ingestion
- Spring Boot subscribes to device telemetry, status and acknowledgement topics.
- Telemetry is stored in PostgreSQL with the original JSON payload and de-duplication by `message_id`.
- Device last-seen/status/pump state are updated from device messages.
- Command acknowledgements update the matching command row and device pump state.
- Device latest-telemetry and bounded telemetry-history endpoints are available.
- Idempotent schema creation is enabled at backend startup so existing PostgreSQL volumes receive the additional tables.
- Automated backend unit tests cover telemetry persistence and acknowledgement updates.
- Test instructions: `docs/tests/MODULE_01_MQTT_TELEMETRY_TEST.md`.

### Not yet complete
- The ESP32 publisher/command subscriber/acknowledgement firmware must be implemented and tested on the actual board.
- DIIS formula and cycle aggregation are not finalized.
- XGBoost/SHAP inference API is not connected.
- Persistent scheduler, manual-control UI/API, digital twin and full AI tool registry remain future modules.
- Authentication/roles, production MQTT credentials/TLS and deployment hardening are not implemented.

Do not connect a physical pump until firmware command expiry, fault-lock rejection, manual-run timeout and OFF behaviour have been tested. A backend publish is not proof that the pump acted.

Mosquitto is configured for local development only. Never expose MQTT or PostgreSQL
directly to the public internet. Remote access needs HTTPS and a VPN/secure proxy.
