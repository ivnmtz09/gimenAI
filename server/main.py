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
    print("⚠️  ADVERTENCIA: GEMINI_API_KEY no está configurada en server/.env")

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
# Registro de Dispositivos Inteligentes del Hogar
# ==============================================================================
smart_devices: Dict[str, Dict[str, Any]] = {
    "luz_sala": {
        "id": "luz_sala",
        "name": "Bombillo de la Sala",
        "type": "light",
        "target": "esp32",
        "state": False,
        "pin": 2,
        "ip": None,
        "description": "LED azul integrado en la placa ESP32 DevKit V1"
    },
    "luz_cocina": {
        "id": "luz_cocina",
        "name": "Bombillo de la Cocina",
        "type": "light",
        "target": "virtual",
        "state": False,
        "pin": 4,
        "ip": None,
        "description": "Bombillo inteligente virtual (o GPIO 4)"
    },
    "luz_habitacion": {
        "id": "luz_habitacion",
        "name": "Luz de la Habitación",
        "type": "light",
        "target": "virtual",
        "state": False,
        "pin": 16,
        "ip": None,
        "description": "Luz inteligente de dormitorio"
    },
    "ventilador": {
        "id": "ventilador",
        "name": "Ventilador de Techo",
        "type": "fan",
        "target": "virtual",
        "state": False,
        "pin": 5,
        "ip": None,
        "description": "Ventilador / Enchufe inteligente"
    }
}

# Nodos ESP32 conectados en la red
esp32_nodes: Dict[str, Dict[str, Any]] = {}

# Dispositivos de red descubiertos (mDNS / Smart TVs, etc.)
discovered_network_devices: List[Dict[str, Any]] = []

# ==============================================================================
# Historial de Conversación Real (Memoria Multi-Turn)
# ==============================================================================
# Guarda los turnos de diálogo: [{"role": "user"|"model", "text": "..."}]
conversation_memory: List[Dict[str, str]] = []

def get_system_prompt() -> str:
    # Resumen de dispositivos disponibles
    devices_lines = []
    for d_id, d in smart_devices.items():
        st = "ENCENDIDO" if d["state"] else "APAGADO"
        ip_info = f" (IP: {d['ip']})" if d.get("ip") else ""
        devices_lines.append(f"- '{d_id}': {d['name']} [{st}] - {d['description']}{ip_info}")

    devices_str = "\n".join(devices_lines)

    # Nodos ESP32 físicos detectados
    esp_lines = []
    if esp32_nodes:
        for node_id, node in esp32_nodes.items():
            esp_lines.append(f"- Nodo ESP32 '{node_id}' en IP {node.get('ip')} (Última señal: {node.get('last_seen')}s)")
    else:
        esp_lines.append("- Ningún ESP32 ha enviado anuncio todavía (los controles son simulados hasta que se conecte).")

    esp_str = "\n".join(esp_lines)

    # Dispositivos de red descubiertos
    net_str = f"{len(discovered_network_devices)} dispositivos en red local" if discovered_network_devices else "Ninguno adicional escaneado"

    return f"""
Eres Gimena, un robot interactivo asistente de hogar con personalidad propia, creado en la Universidad de La Guajira.
Tu tono es siempre cálido, empático, claro, inteligente y conciso (apto para mostrarse en pantalla OLED y leerse en voz alta).

REGLAS DE CONVERSACIÓN REAL:
1. Recuerdas los mensajes previos de la conversación actual. Si el usuario te dice su nombre, gustos o hace referencia a algo anterior ("apágalo", "cuál es mi nombre"), debes recordarlo fielmente.
2. Si te preguntan qué dispositivos inteligentes detectas, explica con detalle los bombillos, nodos ESP32 y dispositivos que ves en tu lista.
3. Si el usuario pide encender, apagar o ajustar dispositivos:
   - Responde de forma muy natural y agradable.
   - Incluye los cambios en la lista "device_actions".

DISPOSITIVOS INTELIGENTES REGISTRADOS:
{devices_str}

NODOS ESP32 HARDWARE EN LA RED:
{esp_str}

RED LOCAL:
{net_str}

FORMATO OBLIGATORIO DE RESPUESTA (JSON ESTRICTO):
{{
  "reply": "Texto de la respuesta para el usuario",
  "face": "happy" | "neutral" | "thinking" | "surprised",
  "device_actions": [
     {{ "device": "id_del_dispositivo", "state": true/false }}
  ]
}}
Si no modificas dispositivos, "device_actions" debe ser [].
"""

app = FastAPI(
    title="GimenAI IoT Gateway & Smart Chat",
    description="Servidor con chat real, descubrimiento de dispositivos y control domótico para ESP32",
    version="2.1.0"
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
            "face": "thinking",
            "device_actions": []
        }

    sys_instruction = get_system_prompt()
    candidate_models = ["gemini-3.5-flash-lite", "gemini-3.8-flash", "gemini-flash-latest"]

    # Construir contenido con historial de conversación
    # Mantener los últimos 12 turnos para contexto rico y rápido
    recent_history = conversation_memory[-12:]
    
    # Formatear el historial para Gemini
    dialog_context = ""
    for turn in recent_history:
        role_label = "Usuario" if turn["role"] == "user" else "Gimena"
        dialog_context += f"{role_label}: {turn['text']}\n"
    dialog_context += f"Usuario: {user_message}\nGimena:"

    full_prompt = f"HISTORIAL DE CONVERSACIÓN:\n{dialog_context}\n\nResponde como Gimena a la última intervención en JSON estricto."

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

            # Guardar en memoria multi-turn
            conversation_memory.append({"role": "user", "text": user_message})
            conversation_memory.append({"role": "model", "text": data.get("reply", "")})

            return data
        except Exception as e:
            print(f"Intento con {model_name} falló: {e}")
            continue

    return {
        "reply": "Tuve una pequeña dificultad para comunicarme con mis servicios en la nube.",
        "face": "thinking",
        "device_actions": []
    }

# ==============================================================================
# Notificar a un ESP32 Físico en la Red
# ==============================================================================
async def notify_esp32_physical(device_id: str, new_state: bool):
    """Si el dispositivo está asociado a una IP de un ESP32, le envía la orden vía HTTP."""
    device = smart_devices.get(device_id)
    if not device:
        return

    esp_ip = device.get("ip")
    # Si no tiene IP directa asignada pero hay un nodo ESP32 registrado, usamos la de ese nodo
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
    """Chat conversacional con memoria y control domótico."""
    user_msg = req.message.strip()
    print(f"\n[MENSAJE USUARIO]: {user_msg}")

    result = generate_chat_response(user_msg)
    reply_text = result.get("reply", "Entendido.")
    face = result.get("face", "happy")
    actions = result.get("device_actions", [])

    # Aplicar acciones de dispositivos
    for action in actions:
        d_id = action.get("device")
        st = action.get("state")
        if d_id in smart_devices and st is not None:
            smart_devices[d_id]["state"] = bool(st)
            print(f"[DOMÓTICA]: {d_id} cambiado a {st}")
            # Notificar al ESP32 por hardware si está en línea
            asyncio.create_task(notify_esp32_physical(d_id, bool(st)))

    # Generar vista simple para dispositivos
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
    """Retorna el historial completo de la conversación."""
    return {"history": conversation_memory}

@app.delete("/api/chat/history")
def clear_chat_history():
    """Borra la memoria de la conversación para iniciar un chat nuevo."""
    global conversation_memory
    conversation_memory = []
    return {"status": "cleared", "message": "Historial de conversación reiniciado."}

@app.get("/api/devices")
def get_all_devices():
    """Consulta la lista de dispositivos inteligentes y nodos ESP32 conectados."""
    return {
        "devices": smart_devices,
        "esp32_nodes": esp32_nodes,
        "network_devices": discovered_network_devices
    }

@app.post("/api/devices/{device_id}/toggle")
async def toggle_device_endpoint(device_id: str):
    """Alterna manualmente el estado de un dispositivo desde la web."""
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
    """El ESP32 llama a este endpoint al arrancar para anunciar su IP y dispositivos."""
    esp32_nodes[req.id] = {
        "id": req.id,
        "name": req.name,
        "ip": req.ip,
        "capabilities": req.capabilities,
        "last_seen": int(time.time())
    }
    # Asignar IP al bombillo de la sala
    if "luz_sala" in req.capabilities and "luz_sala" in smart_devices:
        smart_devices["luz_sala"]["ip"] = req.ip
        smart_devices["luz_sala"]["target"] = "esp32_hardware"

    print(f"\n🎉 [NUEVO DISPOSITIVO HARDWARE DETECTADO]: {req.name} ({req.id}) en IP {req.ip}")
    print(f"   Capacidades registradas: {req.capabilities}\n")

    return {
        "status": "registered",
        "assigned_ip": req.ip,
        "device_states": {k: v["state"] for k, v in smart_devices.items()}
    }

@app.get("/api/devices/scan")
async def scan_network_devices():
    """Escanea la red local para descubrir dispositivos inteligentes activos."""
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
            "_esphomelib._tcp.local.",
            "_airplay._tcp.local."
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
# Interfaz Web Completa (Panel de Control y Chat en Tiempo Real)
# ==============================================================================
@app.get("/", response_class=HTMLResponse)
def get_web_ui():
    return """
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>GimenAI - Asistente & Domótica Real</title>
    <style>
        :root {
            --bg-dark: #0b1120;
            --panel-bg: #1e293b;
            --panel-border: #334155;
            --primary: #38bdf8;
            --primary-hover: #0ea5e9;
            --accent-on: #22c55e;
            --accent-off: #64748b;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
        body { background: var(--bg-dark); color: var(--text-main); display: flex; flex-direction: column; height: 100vh; }

        header { background: var(--panel-bg); padding: 14px 20px; display: flex; align-items: center; justify-content: space-between; border-bottom: 1px solid var(--panel-border); }
        header h1 { font-size: 1.2rem; color: var(--primary); display: flex; align-items: center; gap: 8px; }
        .header-actions { display: flex; gap: 8px; align-items: center; }
        .badge { font-size: 0.75rem; background: #064e3b; color: #34d399; padding: 5px 12px; border-radius: 999px; font-weight: bold; }
        .btn-scan { background: #0284c7; color: white; border: none; padding: 6px 12px; border-radius: 6px; font-size: 0.8rem; cursor: pointer; font-weight: bold; }
        .btn-clear { background: #475569; color: #f8fafc; border: none; padding: 6px 10px; border-radius: 6px; font-size: 0.8rem; cursor: pointer; }

        /* Panel de Dispositivos */
        .hardware-banner { background: #172554; border-bottom: 1px solid #1e40af; padding: 8px 20px; font-size: 0.85rem; display: flex; justify-content: space-between; align-items: center; color: #93c5fd; }
        .dashboard { background: #0f172a; padding: 12px 20px; display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; border-bottom: 1px solid var(--panel-border); }
        .device-card { background: var(--panel-bg); padding: 12px; border-radius: 8px; text-align: center; border: 1px solid var(--panel-border); transition: 0.2s; }
        .device-card h4 { font-size: 0.82rem; color: var(--text-muted); margin-bottom: 4px; }
        .device-type { font-size: 0.7rem; color: #64748b; margin-bottom: 8px; }
        .device-btn { width: 100%; border: none; padding: 7px; border-radius: 6px; font-weight: bold; cursor: pointer; transition: 0.2s; font-size: 0.85rem; }
        .btn-on { background: #16a34a; color: white; box-shadow: 0 0 10px rgba(34,197,94,0.3); }
        .btn-off { background: #334155; color: #cbd5e1; }

        /* Chat */
        .chat-container { flex: 1; overflow-y: auto; padding: 20px; display: flex; flex-direction: column; gap: 14px; }
        .message { max-width: 78%; padding: 12px 16px; border-radius: 12px; font-size: 0.95rem; line-height: 1.45; animation: fadeIn 0.2s ease; }
        .user-msg { align-self: flex-end; background: #0284c7; color: white; border-bottom-right-radius: 2px; }
        .bot-msg { align-self: flex-start; background: var(--panel-bg); border: 1px solid var(--panel-border); border-bottom-left-radius: 2px; }
        .bot-header { font-size: 0.75rem; color: var(--primary); margin-bottom: 4px; font-weight: bold; }

        .chips { display: flex; gap: 8px; overflow-x: auto; padding: 8px 20px; background: #0f172a; border-top: 1px solid var(--panel-border); }
        .chip { background: var(--panel-bg); color: var(--text-muted); border: 1px solid var(--panel-border); padding: 6px 12px; border-radius: 999px; font-size: 0.8rem; white-space: nowrap; cursor: pointer; }
        .chip:hover { color: var(--primary); border-color: var(--primary); }

        footer { background: var(--panel-bg); padding: 12px 20px; border-top: 1px solid var(--panel-border); display: flex; gap: 10px; }
        input[type="text"] { flex: 1; background: #0b1120; border: 1px solid var(--panel-border); color: white; padding: 12px 16px; border-radius: 8px; font-size: 1rem; outline: none; }
        input[type="text"]:focus { border-color: var(--primary); }
        button.send-btn { background: var(--primary); color: #0b1120; border: none; font-weight: bold; padding: 0 22px; border-radius: 8px; cursor: pointer; font-size: 1rem; }
        button.send-btn:hover { background: var(--primary-hover); }

        @keyframes fadeIn { from { opacity: 0; transform: translateY(6px); } to { opacity: 1; transform: translateY(0); } }
    </style>
</head>
<body>
    <header>
        <h1>🤖 Gimena — Asistente de Hogar</h1>
        <div class="header-actions">
            <span class="badge" id="esp-status-badge">ESP32: Esperando conexión</span>
            <button class="btn-scan" onclick="scanNetwork()">🔍 Escanear Red</button>
            <button class="btn-clear" onclick="clearHistory()">🗑️ Limpiar Chat</button>
        </div>
    </header>

    <div class="hardware-banner" id="hardware-banner">
        <span>Hardware: ESP32 DevKit V1 (LED GPIO 2 asignado a Bombillo Sala)</span>
        <span id="nodes-count">0 Nodos detectados</span>
    </div>

    <div class="dashboard" id="devices-dashboard"></div>

    <div class="chat-container" id="chat">
        <div class="message bot-msg">
            <div class="bot-header">Gimena (^.^)</div>
            ¡Hola! Soy Gimena. Tengo control sobre los dispositivos inteligentes de tu hogar y recuerdo toda nuestra conversación. Puedes pedirme que encienda los bombillos o preguntarme lo que necesites.
        </div>
    </div>

    <div class="chips">
        <div class="chip" onclick="sendChip('Enciende la luz de la sala')">💡 Enciende luz sala</div>
        <div class="chip" onclick="sendChip('Apaga todos los bombillos')">🌑 Apaga todo</div>
        <div class="chip" onclick="sendChip('¿Qué dispositivos inteligentes detectas?')">🔍 ¿Qué dispositivos detectas?</div>
        <div class="chip" onclick="sendChip('Me llamo Iván, recuérdalo')">👤 Mi nombre es Iván</div>
        <div class="chip" onclick="sendChip('¿Cómo me llamo y qué luces tienes encendidas?')">🧠 ¿Cómo me llamo y qué luces hay?</div>
    </div>

    <footer>
        <input type="text" id="userInput" placeholder="Escribe un mensaje o comando a Gimena..." onkeydown="if(event.key==='Enter') sendMessage()">
        <button class="send-btn" onclick="sendMessage()">Enviar</button>
    </footer>

    <script>
        const chatDiv = document.getElementById('chat');
        const input = document.getElementById('userInput');

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
            const badge = document.getElementById('esp-status-badge');
            const nodesCount = document.getElementById('nodes-count');
            if (count > 0) {
                const first = Object.values(nodes)[0];
                badge.textContent = `● ESP32 Online (${first.ip})`;
                badge.style.background = '#064e3b';
                badge.style.color = '#34d399';
                nodesCount.textContent = `${count} ESP32 conectado (${first.ip})`;
            } else {
                badge.textContent = `○ ESP32: Esperando señal WiFi`;
                badge.style.background = '#475569';
                badge.style.color = '#cbd5e1';
                nodesCount.textContent = `Modo Simulado / Esperando ESP32`;
            }
        }

        function renderDashboard(devices) {
            const container = document.getElementById('devices-dashboard');
            container.innerHTML = '';
            for (const [key, dev] of Object.entries(devices)) {
                const card = document.createElement('div');
                card.className = 'device-card';
                const isLight = dev.type === 'light';
                const icon = isLight ? (dev.state ? '💡' : '🌑') : (dev.state ? '🌀' : '💨');
                const targetLabel = dev.target === 'esp32_hardware' ? '⚡ ESP32 GPIO ' + dev.pin : 'Virtual';
                card.innerHTML = `
                    <h4>${icon} ${dev.name}</h4>
                    <div class="device-type">${targetLabel}</div>
                    <button class="device-btn ${dev.state ? 'btn-on' : 'btn-off'}" onclick="toggleDevice('${key}')">
                        ${dev.state ? 'ENCENDIDO' : 'APAGADO'}
                    </button>
                `;
                container.appendChild(card);
            }
        }

        async function toggleDevice(id) {
            const res = await fetch(`/api/devices/${id}/toggle`, { method: 'POST' });
            fetchDevices();
        }

        async function scanNetwork() {
            appendMessage('bot', 'Escaneando dispositivos inteligentes en la red local... 🔍');
            try {
                const res = await fetch('/api/devices/scan');
                const data = await res.json();
                appendMessage('bot', `Escaneo completado. Se detectaron ${data.count} dispositivos en la red local.`);
                fetchDevices();
            } catch(e) {
                appendMessage('bot', 'Error al escanear la red.');
            }
        }

        async function clearHistory() {
            await fetch('/api/chat/history', { method: 'DELETE' });
            chatDiv.innerHTML = `
                <div class="message bot-msg">
                    <div class="bot-header">Gimena (^.^)</div>
                    Memoria reiniciada. ¿En qué te puedo ayudar ahora?
                </div>
            `;
        }

        function sendChip(text) {
            input.value = text;
            sendMessage();
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
                appendMessage('bot', data.reply, data.face);
                fetchDevices();
            } catch (err) {
                appendMessage('bot', 'Error de conexión con el servidor.');
            }
        }

        function appendMessage(sender, text, face = 'happy') {
            const msg = document.createElement('div');
            msg.className = `message ${sender === 'user' ? 'user-msg' : 'bot-msg'}`;
            if (sender === 'bot') {
                const faceIcon = face === 'happy' ? '(^.^)' : (face === 'surprised' ? '(O.O)' : (face === 'thinking' ? '(~.~)' : '(o.o)'));
                msg.innerHTML = `<div class="bot-header">Gimena ${faceIcon}</div>` + text;
            } else {
                msg.textContent = text;
            }
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
