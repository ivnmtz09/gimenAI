import os
import json
import time
import asyncio
import socket
from typing import Dict, Any, List, Optional
from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import JSONResponse, HTMLResponse
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from dotenv import load_dotenv

load_dotenv()

API_KEY = os.getenv("GEMINI_API_KEY")
if not API_KEY or API_KEY == "tu_clave_api_de_google_aqui":
    print("ADVERTENCIA: GEMINI_API_KEY no esta configurada en server/.env")

# Cliente oficial de Google GenAI
try:
    from google import genai
    from google.genai import types
    USE_NEW_SDK = True
    client = genai.Client(api_key=API_KEY) if API_KEY else None
except ImportError:
    import warnings
    warnings.filterwarnings("ignore", category=FutureWarning)
    import google.generativeai as legacy_genai
    USE_NEW_SDK = False
    if API_KEY:
        legacy_genai.configure(api_key=API_KEY)

# ==============================================================================
# Registro Dinámico de Dispositivos Inteligentes del Hogar
# ==============================================================================
# Inicia vacío: Solo se agregan dispositivos reales cuando el ESP32 se conecta
# o cuando el escáner de red detecta equipos en tu WiFi.
smart_devices: Dict[str, Dict[str, Any]] = {}

# Nodos ESP32 conectados en la red
esp32_nodes: Dict[str, Dict[str, Any]] = {}

# Dispositivos de red descubiertos (mDNS)
discovered_network_devices: List[Dict[str, Any]] = []

# ==============================================================================
# Historial de Conversación Real (Memoria Multi-Turn)
# ==============================================================================
conversation_memory: List[Dict[str, str]] = []

def get_system_prompt() -> str:
    devices_lines = []
    if smart_devices:
        for d_id, d in smart_devices.items():
            st = "ENCENDIDO" if d["state"] else "APAGADO"
            ip_info = f" (IP: {d['ip']})" if d.get("ip") else ""
            devices_lines.append(f"- '{d_id}': {d['name']} [{st}] - {d['description']}{ip_info}")
    else:
        devices_lines.append("- En este momento NO hay ningún dispositivo registrado ni conectado.")

    devices_str = "\n".join(devices_lines)

    esp_lines = []
    if esp32_nodes:
        for node_id, node in esp32_nodes.items():
            esp_lines.append(f"- Nodo ESP32 '{node_id}' en IP {node.get('ip')}")
    else:
        esp_lines.append("- Ningún ESP32 conectado todavía. Esperando conexión a la red WiFi.")

    esp_str = "\n".join(esp_lines)
    net_str = f"{len(discovered_network_devices)} dispositivos detectados en red local" if discovered_network_devices else "Ninguno adicional"

    return f"""
Eres Gimena, un asistente interactivo para el hogar, desarrollado en la Universidad de La Guajira.
Tu tono es respetuoso, empático, claro, conciso y formal pero accesible.

REGLAS OBLIGATORIAS:
1. No utilices emojis ni caracteres de emoticonos en tus respuestas bajo ninguna circunstancia.
2. Recuerdas los mensajes previos de la conversación actual. Si el usuario te indica su nombre o hace referencia a algo anterior, recuérdalo con fidelidad.
3. Si el usuario pregunta qué dispositivos tienes o detectas, responde basándote exactamente en la lista siguiente. Si no hay dispositivos, indícalo claramente con sinceridad.
4. Si el usuario pide encender, apagar o ajustar dispositivos:
   - Responde de forma natural sin emojis.
   - Incluye los cambios en la lista "device_actions".

DISPOSITIVOS REGISTRADOS:
{devices_str}

NODOS ESP32 HARDWARE EN LA RED:
{esp_str}

RED LOCAL:
{net_str}

FORMATO OBLIGATORIO DE RESPUESTA (JSON ESTRICTO):
{{
  "reply": "Texto de la respuesta en español para el usuario",
  "face": "neutral",
  "device_actions": [
     {{ "device": "id_del_dispositivo", "state": true/false }}
  ]
}}
Si no modificas dispositivos, "device_actions" debe ser [].
"""

app = FastAPI(
    title="GimenAI IoT Gateway",
    description="Servidor con chat y control domotico para ESP32",
    version="2.2.0"
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

class ChatRequest(BaseModel):
    message: str

class ESP32RegisterRequest(BaseModel):
    id: str
    name: str
    ip: str
    capabilities: List[str]

# ==============================================================================
# Generación de Respuestas con Gemini y Memoria
# ==============================================================================
def generate_chat_response(user_message: str) -> Dict[str, Any]:
    global conversation_memory

    if not API_KEY or API_KEY == "tu_clave_api_de_google_aqui":
        return {
            "reply": "No has configurado tu GEMINI_API_KEY en server/.env todavía.",
            "face": "neutral",
            "device_actions": []
        }

    sys_instruction = get_system_prompt()
    candidate_models = ["gemini-3.5-flash-lite", "gemini-3.8-flash", "gemini-flash-latest"]

    recent_history = conversation_memory[-12:]
    dialog_context = ""
    for turn in recent_history:
        role_label = "Usuario" if turn["role"] == "user" else "Gimena"
        dialog_context += f"{role_label}: {turn['text']}\n"
    dialog_context += f"Usuario: {user_message}\nGimena:"

    full_prompt = f"HISTORIAL DE CONVERSACION:\n{dialog_context}\n\nResponde como Gimena en JSON estricto sin emojis."

    for model_name in candidate_models:
        try:
            if USE_NEW_SDK:
                response = client.models.generate_content(
                    model=model_name,
                    contents=full_prompt,
                    config=types.GenerateContentConfig(
                        system_instruction=sys_instruction,
                        response_mime_type="application/json",
                        temperature=0.7
                    )
                )
                raw_text = response.text
            else:
                model = legacy_genai.GenerativeModel(
                    model_name=model_name,
                    system_instruction=sys_instruction,
                    generation_config={"response_mime_type": "application/json", "temperature": 0.7}
                )
                response = model.generate_content(full_prompt)
                raw_text = response.text

            data = json.loads(raw_text)

            conversation_memory.append({"role": "user", "text": user_message})
            conversation_memory.append({"role": "model", "text": data.get("reply", "")})

            return data
        except Exception as e:
            print(f"Intento con {model_name} fallo: {e}")
            continue

    return {
        "reply": "Ocurrio una dificultad al comunicarse con los servicios de inteligencia en la nube.",
        "face": "neutral",
        "device_actions": []
    }

# ==============================================================================
# Notificar a un ESP32 Físico en la Red
# ==============================================================================
async def notify_esp32_physical(device_id: str, new_state: bool):
    device = smart_devices.get(device_id)
    if not device:
        return

    esp_ip = device.get("ip")
    if not esp_ip and esp32_nodes:
        first_node = next(iter(esp32_nodes.values()))
        esp_ip = first_node.get("ip")

    if esp_ip:
        import httpx
        url = f"http://{esp_ip}/api/device"
        try:
            async with httpx.AsyncClient(timeout=2.0) as http_client:
                await http_client.post(url, json={"device": device_id, "state": new_state})
                print(f"[HARDWARE ESP32] Comando enviado a {esp_ip}: {device_id} -> {new_state}")
        except Exception as err:
            print(f"[HARDWARE ESP32] No se pudo enviar orden al ESP32 en {esp_ip}: {err}")

# ==============================================================================
# Endpoints de API y Chat
# ==============================================================================

@app.post("/api/chat")
async def chat_endpoint(req: ChatRequest):
    user_msg = req.message.strip()
    print(f"\n[MENSAJE USUARIO]: {user_msg}")

    result = generate_chat_response(user_msg)
    reply_text = result.get("reply", "Entendido.")
    face = result.get("face", "neutral")
    actions = result.get("device_actions", [])

    for action in actions:
        d_id = action.get("device")
        st = action.get("state")
        if d_id in smart_devices and st is not None:
            smart_devices[d_id]["state"] = bool(st)
            print(f"[DOMOTICA]: {d_id} cambiado a {st}")
            asyncio.create_task(notify_esp32_physical(d_id, bool(st)))

    devices_summary = {k: v["state"] for k, v in smart_devices.items()}

    return {
        "reply": reply_text,
        "face": face,
        "devices": devices_summary,
        "raw_devices": smart_devices,
        "connected_nodes": list(esp32_nodes.values())
    }

@app.get("/api/chat/history")
def get_chat_history():
    return {"history": conversation_memory}

@app.delete("/api/chat/history")
def clear_chat_history():
    global conversation_memory
    conversation_memory = []
    return {"status": "cleared", "message": "Historial de conversacion reiniciado."}

@app.get("/api/devices")
def get_all_devices():
    return {
        "devices": smart_devices,
        "esp32_nodes": esp32_nodes,
        "network_devices": discovered_network_devices
    }

@app.post("/api/devices/{device_id}/toggle")
async def toggle_device_endpoint(device_id: str):
    if device_id in smart_devices:
        new_state = not smart_devices[device_id]["state"]
        smart_devices[device_id]["state"] = new_state
        asyncio.create_task(notify_esp32_physical(device_id, new_state))
        return {
            "status": "ok",
            "device": device_id,
            "state": new_state,
            "devices": {k: v["state"] for k, v in smart_devices.items()}
        }
    raise HTTPException(status_code=404, detail="Dispositivo no encontrado")

@app.post("/api/devices/register")
def register_esp32_node(req: ESP32RegisterRequest):
    esp32_nodes[req.id] = {
        "id": req.id,
        "name": req.name,
        "ip": req.ip,
        "capabilities": req.capabilities,
        "last_seen": int(time.time())
    }
    
    if "luz_sala" in req.capabilities:
        smart_devices["luz_sala"] = {
            "id": "luz_sala",
            "name": "LED Azul Placa (Bombillo Sala)",
            "type": "light",
            "target": "esp32_hardware",
            "state": False,
            "pin": 2,
            "ip": req.ip,
            "description": f"LED integrado en ESP32 DevKit (GPIO 2) en {req.ip}"
        }

    print(f"\n[DISPOSITIVO DETECTADO]: {req.name} ({req.id}) en IP {req.ip}")
    return {
        "status": "registered",
        "assigned_ip": req.ip,
        "device_states": {k: v["state"] for k, v in smart_devices.items()}
    }

@app.get("/api/devices/scan")
async def scan_network_devices():
    global discovered_network_devices
    discovered = []

    try:
        from zeroconf import Zeroconf, ServiceBrowser

        class SmartListener:
            def add_service(self, zc, type_, name):
                info = zc.get_service_info(type_, name)
                if info:
                    addrs = ["%d.%d.%d.%d" % tuple(a) for a in info.addresses]
                    discovered.append({
                        "name": name.split(".")[0],
                        "type": type_,
                        "ip": addrs[0] if addrs else "Desconocida",
                        "port": info.port
                    })
            def remove_service(self, zc, type_, name): pass
            def update_service(self, zc, type_, name): pass

        zc = Zeroconf()
        services = [
            "_http._tcp.local.",
            "_googlecast._tcp.local.",
            "_esphomelib._tcp.local."
        ]
        browser = ServiceBrowser(zc, services, SmartListener())
        await asyncio.sleep(2)
        zc.close()

    except Exception as e:
        print(f"Error en escaneo mDNS: {e}")

    discovered_network_devices = discovered
    return {
        "count": len(discovered),
        "devices": discovered,
        "esp32_nodes": esp32_nodes
    }

# ==============================================================================
# Interfaz Web Responsiva, Minimalista y Sin Emojis ni Iconos
# ==============================================================================
@app.get("/", response_class=HTMLResponse)
def get_web_ui():
    return """
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>GimenAI - Asistente Domotico</title>
    <style>
        :root {
            --bg: #090d16;
            --surface: #111827;
            --surface-hover: #1f2937;
            --surface-border: #1e293b;
            --primary: #0284c7;
            --primary-hover: #0369a1;
            --text-primary: #f8fafc;
            --text-secondary: #94a3b8;
            --text-muted: #64748b;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            -webkit-tap-highlight-color: transparent;
        }

        body {
            background: var(--bg);
            color: var(--text-primary);
            height: 100vh;
            height: 100dvh;
            display: flex;
            flex-direction: column;
            overflow: hidden;
        }

        header {
            background: var(--surface);
            border-bottom: 1px solid var(--surface-border);
            padding: 14px 20px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            flex-shrink: 0;
        }

        .brand-container {
            display: flex;
            flex-direction: column;
        }

        .brand-title {
            font-size: 1.15rem;
            font-weight: 700;
            letter-spacing: -0.02em;
            color: var(--text-primary);
        }

        .brand-subtitle {
            font-size: 0.72rem;
            color: var(--text-muted);
            font-weight: 500;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            margin-top: 2px;
        }

        .header-controls {
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .status-pill {
            font-size: 0.7rem;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            padding: 5px 10px;
            border-radius: 4px;
            background: var(--surface-hover);
            color: var(--text-muted);
            border: 1px solid var(--surface-border);
        }

        .status-pill.online {
            background: rgba(5, 150, 105, 0.15);
            color: #34d399;
            border-color: rgba(5, 150, 105, 0.35);
        }

        .btn-action {
            background: var(--surface-hover);
            color: var(--text-secondary);
            border: 1px solid var(--surface-border);
            padding: 6px 12px;
            border-radius: 4px;
            font-size: 0.76rem;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.15s ease;
        }

        .btn-action:hover {
            color: var(--text-primary);
            border-color: var(--primary);
        }

        /* Sección de Dispositivos: Oculta por defecto, solo visible si hay dispositivos */
        #devices-section {
            display: none;
            background: #0d1322;
            border-bottom: 1px solid var(--surface-border);
            padding: 14px 20px;
            flex-shrink: 0;
            animation: fadeIn 0.2s ease;
        }

        .section-header {
            font-size: 0.72rem;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.06em;
            color: var(--text-muted);
            margin-bottom: 10px;
            display: flex;
            justify-content: space-between;
        }

        .devices-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
            gap: 10px;
        }

        .device-card {
            background: var(--surface);
            border: 1px solid var(--surface-border);
            border-radius: 6px;
            padding: 12px 14px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 12px;
        }

        .device-info {
            overflow: hidden;
        }

        .device-name {
            font-size: 0.85rem;
            font-weight: 600;
            color: var(--text-primary);
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }

        .device-meta {
            font-size: 0.72rem;
            color: var(--text-muted);
            margin-top: 3px;
        }

        .toggle-btn {
            padding: 6px 14px;
            font-size: 0.72rem;
            font-weight: 600;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            transition: background 0.15s ease;
            text-transform: uppercase;
            letter-spacing: 0.04em;
        }

        .toggle-btn.state-on {
            background: #15803d;
            color: #f0fdf4;
        }

        .toggle-btn.state-off {
            background: #334155;
            color: #94a3b8;
        }

        /* Area de Chat */
        .chat-area {
            flex: 1;
            min-height: 0;
            overflow-y: auto;
            padding: 20px;
            display: flex;
            flex-direction: column;
            gap: 14px;
        }

        .chat-message {
            max-width: 80%;
            padding: 12px 16px;
            border-radius: 8px;
            font-size: 0.92rem;
            line-height: 1.5;
            word-break: break-word;
            animation: fadeIn 0.15s ease;
        }

        .chat-message.user {
            align-self: flex-end;
            background: #0284c7;
            color: #ffffff;
            border-bottom-right-radius: 2px;
        }

        .chat-message.bot {
            align-self: flex-start;
            background: var(--surface);
            border: 1px solid var(--surface-border);
            color: var(--text-primary);
            border-bottom-left-radius: 2px;
        }

        .message-author {
            font-size: 0.72rem;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.04em;
            color: var(--text-secondary);
            margin-bottom: 4px;
        }

        .chat-message.user .message-author {
            color: #bae6fd;
        }

        /* Pie de Pagina / Entrada de Texto */
        footer {
            background: var(--surface);
            padding: 14px 20px;
            border-top: 1px solid var(--surface-border);
            display: flex;
            gap: 10px;
            flex-shrink: 0;
        }

        .input-text {
            flex: 1;
            background: var(--bg);
            border: 1px solid var(--surface-border);
            color: var(--text-primary);
            padding: 12px 14px;
            border-radius: 6px;
            font-size: 0.95rem;
            outline: none;
            transition: border-color 0.15s ease;
        }

        .input-text:focus {
            border-color: var(--primary);
        }

        .btn-send {
            background: var(--primary);
            color: #ffffff;
            border: none;
            font-weight: 600;
            font-size: 0.88rem;
            padding: 0 20px;
            border-radius: 6px;
            cursor: pointer;
            transition: background 0.15s ease;
        }

        .btn-send:hover {
            background: var(--primary-hover);
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(4px); }
            to { opacity: 1; transform: translateY(0); }
        }

        /* Adaptacion Responsiva para Moviles */
        @media (max-width: 640px) {
            header {
                padding: 10px 14px;
            }
            .brand-title {
                font-size: 1rem;
            }
            .brand-subtitle {
                display: none;
            }
            .btn-action {
                padding: 6px 8px;
                font-size: 0.72rem;
            }
            #devices-section {
                padding: 10px 14px;
            }
            .devices-grid {
                grid-template-columns: 1fr;
            }
            .chat-area {
                padding: 14px 12px;
            }
            .chat-message {
                max-width: 90%;
                font-size: 0.88rem;
                padding: 10px 12px;
            }
            footer {
                padding: 10px 12px;
            }
            .input-text {
                font-size: 0.9rem;
                padding: 10px 12px;
            }
            .btn-send {
                padding: 0 14px;
            }
        }
    </style>
</head>
<body>
    <header>
        <div class="brand-container">
            <span class="brand-title">GimenAI</span>
            <span class="brand-subtitle">Asistente Domotico</span>
        </div>
        <div class="header-controls">
            <span class="status-pill" id="esp-status-badge">ESP32 Desconectado</span>
            <button class="btn-action" onclick="scanNetwork()">Escanear Red</button>
            <button class="btn-action" onclick="clearHistory()">Limpiar Chat</button>
        </div>
    </header>

    <!-- Solo visible cuando hay dispositivos registrados -->
    <section id="devices-section">
        <div class="section-header">
            <span>Dispositivos Activos</span>
            <span id="devices-count">0</span>
        </div>
        <div class="devices-grid" id="devices-dashboard"></div>
    </section>

    <div class="chat-area" id="chat">
        <div class="chat-message bot">
            <div class="message-author">Gimena</div>
            Hola. Soy Gimena, tu asistente de hogar. Puedes darme instrucciones de control o hacerme cualquier consulta.
        </div>
    </div>

    <footer>
        <input type="text" class="input-text" id="userInput" placeholder="Escribe un mensaje o instruccion..." onkeydown="if(event.key==='Enter') sendMessage()">
        <button class="btn-send" onclick="sendMessage()">Enviar</button>
    </footer>

    <script>
        const chatDiv = document.getElementById('chat');
        const input = document.getElementById('userInput');
        const devicesSection = document.getElementById('devices-section');
        const devicesDashboard = document.getElementById('devices-dashboard');
        const devicesCount = document.getElementById('devices-count');
        const espStatusBadge = document.getElementById('esp-status-badge');

        async function fetchDevices() {
            try {
                const res = await fetch('/api/devices');
                const data = await res.json();
                renderDashboard(data.devices);
                updateHardwareStatus(data.esp32_nodes);
            } catch(e) { console.error(e); }
        }

        function updateHardwareStatus(nodes) {
            const count = Object.keys(nodes).length;
            if (count > 0) {
                const first = Object.values(nodes)[0];
                espStatusBadge.textContent = 'ESP32 Conectado (' + first.ip + ')';
                espStatusBadge.className = 'status-pill online';
            } else {
                espStatusBadge.textContent = 'ESP32 Desconectado';
                espStatusBadge.className = 'status-pill';
            }
        }

        function renderDashboard(devices) {
            devicesDashboard.innerHTML = '';
            const keys = Object.keys(devices);

            // Si NO hay dispositivos, se oculta completamente la sección
            if (keys.length === 0) {
                devicesSection.style.display = 'none';
                return;
            }

            // Si hay dispositivos, se muestra la sección
            devicesSection.style.display = 'block';
            devicesCount.textContent = keys.length.toString();

            for (const [key, dev] of Object.entries(devices)) {
                const card = document.createElement('div');
                card.className = 'device-card';
                const targetLabel = dev.target === 'esp32_hardware' ? 'ESP32 GPIO ' + dev.pin : 'Virtual';
                const stateClass = dev.state ? 'state-on' : 'state-off';
                const stateLabel = dev.state ? 'Encendido' : 'Apagado';

                card.innerHTML = `
                    <div class="device-info">
                        <div class="device-name">${dev.name}</div>
                        <div class="device-meta">${targetLabel}</div>
                    </div>
                    <button class="toggle-btn ${stateClass}" onclick="toggleDevice('${key}')">
                        ${stateLabel}
                    </button>
                `;
                devicesDashboard.appendChild(card);
            }
        }

        async function toggleDevice(id) {
            const res = await fetch(`/api/devices/${id}/toggle`, { method: 'POST' });
            fetchDevices();
        }

        async function scanNetwork() {
            appendMessage('bot', 'Escaneando la red local en busca de dispositivos...');
            try {
                const res = await fetch('/api/devices/scan');
                const data = await res.json();
                appendMessage('bot', `Escaneo finalizado. Se detectaron ${data.count} dispositivos.`);
                fetchDevices();
            } catch(e) {
                appendMessage('bot', 'Error al escanear la red.');
            }
        }

        async function clearHistory() {
            await fetch('/api/chat/history', { method: 'DELETE' });
            chatDiv.innerHTML = `
                <div class="chat-message bot">
                    <div class="message-author">Gimena</div>
                    Conversacion reiniciada. ¿En que puedo ayudarte?
                </div>
            `;
        }

        async function sendMessage() {
            const text = input.value.trim();
            if (!text) return;
            input.value = '';

            appendMessage('user', text);

            try {
                const res = await fetch('/api/chat', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ message: text })
                });
                const data = await res.json();
                appendMessage('bot', data.reply);
                fetchDevices();
            } catch (err) {
                appendMessage('bot', 'Error al conectar con el servidor.');
            }
        }

        function appendMessage(sender, text) {
            const msg = document.createElement('div');
            msg.className = `chat-message ${sender === 'user' ? 'user' : 'bot'}`;
            const author = sender === 'user' ? 'Tu' : 'Gimena';
            msg.innerHTML = `<div class="message-author">${author}</div>` + text;
            chatDiv.appendChild(msg);
            chatDiv.scrollTop = chatDiv.scrollHeight;
        }

        fetchDevices();
        setInterval(fetchDevices, 4000);
    </script>
</body>
</html>
    """

if __name__ == "__main__":
    import uvicorn
    port = int(os.getenv("PORT", 8000))
    host = os.getenv("HOST", "0.0.0.0")
    print(f"Iniciando servidor GimenAI en http://{host}:{port}")
    uvicorn.run("main:app", host=host, port=port, reload=True)
