#pragma once
#include <pgmspace.h>
// Panel servido directamente desde la memoria flash del ESP32.
static const char WEB_PANEL[] PROGMEM = R"GIMENAI_HTML(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>GimenAI - ESP32 autónomo</title>
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

        .btn-send:disabled { opacity: 0.6; cursor: wait; }

        .btn-send:hover {
            background: var(--primary-hover);
        }

        /* Bluetooth: selector de parlantes encontrados por el ESP32 */
        #bluetooth-section {
            background: #0d1322;
            border-bottom: 1px solid var(--surface-border);
            padding: 12px 20px;
            flex-shrink: 0;
        }

        .bt-row {
            display: flex;
            align-items: center;
            gap: 8px;
            flex-wrap: wrap;
        }

        .bt-select {
            flex: 1;
            min-width: 180px;
            background: var(--bg);
            color: var(--text-primary);
            border: 1px solid var(--surface-border);
            border-radius: 4px;
            padding: 8px 10px;
            font-size: 0.85rem;
            outline: none;
        }

        .bt-select:focus {
            border-color: var(--primary);
        }

        .bt-status {
            font-size: 0.74rem;
            color: var(--text-muted);
            margin-top: 8px;
            min-height: 1em;
        }

        .bt-status.ok { color: #34d399; }
        .bt-status.error { color: #f87171; }

        .btn-action.primary {
            background: var(--primary);
            border-color: var(--primary);
            color: #ffffff;
        }

        .btn-action:disabled {
            opacity: 0.45;
            cursor: default;
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
            <span class="brand-subtitle">ESP32 autónomo</span>
        </div>
        <div class="header-controls">
            <span class="status-pill" id="esp-status-badge">ESP32 Desconectado</span>
            
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

    <section id="bluetooth-section">
        <div class="section-header">
            <span>Bluetooth</span>
            <span id="bt-state">Sin conexion</span>
        </div>
        <div class="bt-row">
            <select class="bt-select" id="bt-select" aria-label="Dispositivos Bluetooth disponibles" aria-describedby="bt-status" disabled>
                <option value="">Comprobando Bluetooth...</option>
            </select>
            <button class="btn-action" id="bt-scan-btn" onclick="btScan()" disabled>Buscar</button>
            <button class="btn-action primary" id="bt-connect-btn" onclick="btToggleConnection()" disabled>Conectar</button>
        </div>
        <div class="bt-status" id="bt-status" role="status" aria-live="polite"></div>
        <div class="bt-status">El ESP32 busca parlantes y audífonos Bluetooth cercanos. Activa el modo emparejamiento del dispositivo.</div>
    </section>

    <div class="chat-area" id="chat">
        <div class="chat-message bot">
            <div class="message-author">Gimena</div>
            Hola. Soy Gimena, tu asistente de hogar. Puedes darme instrucciones de control o hacerme cualquier consulta.
        </div>
    </div>

    <footer>
        <input type="text" class="input-text" id="userInput" maxlength="500" placeholder="Escribe un mensaje o instruccion..." onkeydown="if(event.key==='Enter') sendMessage()">
        <button class="btn-send" onclick="sendMessage()">Enviar</button>
    </footer>

    <script>
        const chatDiv = document.getElementById('chat');
        const input = document.getElementById('userInput');
        const devicesSection = document.getElementById('devices-section');
        const devicesDashboard = document.getElementById('devices-dashboard');
        const devicesCount = document.getElementById('devices-count');
        const espStatusBadge = document.getElementById('esp-status-badge');

        let chatBusy = false;

        async function fetchDevices() {
            if (chatBusy) return;
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

        async function cloudRequest(path, options = {}) {
            const controller = new AbortController();
            const timer = setTimeout(() => controller.abort(), 90000);
            try {
                const res = await fetch(path, { ...options, signal: controller.signal, cache: 'no-store' });
                const data = await res.json();
                if (!res.ok) throw new Error(data.error || 'No se pudo completar la solicitud');
                return data;
            } finally { clearTimeout(timer); }
        }

        async function clearHistory() {
            if (chatBusy) return;
            try {
                await cloudRequest('/api/chat/history', { method: 'DELETE' });
                chatDiv.replaceChildren();
                appendMessage('bot', 'Conversación reiniciada. ¿En qué puedo ayudarte?');
            } catch(e) { appendMessage('bot', e.message); }
        }

        async function sendMessage() {
            const text = input.value.trim();
            if (!text || chatBusy) return;
            input.value = '';
            chatBusy = true;
            btRevision++;
            input.disabled = true;
            const sendButton = document.querySelector('.btn-send');
            sendButton.disabled = true;
            sendButton.textContent = 'Pensando...';
            renderBluetooth();
            appendMessage('user', text);
            let shown = false;
            try {
                const job = await cloudRequest('/api/chat', {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ message: text })
                });
                const deadline = Date.now() + 180000;
                while (true) {
                    const data = await cloudRequest('/api/chat/status?job=' + job.job);
                    if (data.reply && !shown) {
                        appendMessage('bot', data.reply);
                        shown = true;
                        sendButton.textContent = 'Hablando...';
                    }
                    if (data.error) throw new Error(data.error);
                    if (!data.busy) {
                        if (data.audio_error) appendMessage('bot', 'No pude reproducir la voz: ' + data.audio_error);
                        break;
                    }
                    if (Date.now() >= deadline) throw new Error('La consulta tardó demasiado. Comprueba la conexión del ESP32.');
                    await new Promise(resolve => setTimeout(resolve, 500));
                }
            } catch(e) {
                appendMessage('bot', e.name === 'AbortError' ? 'El ESP32 tardó demasiado en responder.' : e.message);
            } finally {
                chatBusy = false;
                input.disabled = false;
                sendButton.disabled = false;
                sendButton.textContent = 'Enviar';
                renderBluetooth();
                fetchDevices();
                fetchBluetooth();
                input.focus();
            }
        }

        function appendMessage(sender, text) {
            const msg = document.createElement('div');
            msg.className = `chat-message ${sender === 'user' ? 'user' : 'bot'}`;
            const author = sender === 'user' ? 'Tu' : 'Gimena';
            const label = document.createElement('div');
            label.className = 'message-author';
            label.textContent = author;
            msg.append(label, document.createTextNode(String(text)));
            chatDiv.appendChild(msg);
            chatDiv.scrollTop = chatDiv.scrollHeight;
        }

        // ---------------- Bluetooth ----------------
        const btSelect = document.getElementById('bt-select');
        const btState = document.getElementById('bt-state');
        const btStatus = document.getElementById('bt-status');
        const btScanBtn = document.getElementById('bt-scan-btn');
        const btConnectBtn = document.getElementById('bt-connect-btn');
        let btData = { esp32: false, conectado: false, escaneando: false, dispositivos: [] };
        let btBusy = false;
        let btPolling = false;
        let btRevision = 0;
        let btCommandError = '';

        async function btRequest(path, options = {}) {
            const controller = new AbortController();
            const timer = setTimeout(() => controller.abort(), 15000);
            try {
                const res = await fetch(path, { ...options, signal: controller.signal, cache: 'no-store' });
                let data;
                try { data = await res.json(); }
                catch (_) { throw new Error('El servidor devolvió una respuesta Bluetooth inválida'); }
                if (!res.ok) throw new Error(typeof data.detail === 'string' ? data.detail : (data.error || 'No se pudo completar la solicitud Bluetooth'));
                if (!data || typeof data.esp32 !== 'boolean' || !Array.isArray(data.dispositivos)) {
                    throw new Error('El servidor devolvió un estado Bluetooth inválido');
                }
                return data;
            } catch (e) {
                if (e.name === 'AbortError') throw new Error('El ESP32 tardó demasiado en responder. Vuelve a intentarlo.');
                throw e;
            } finally { clearTimeout(timer); }
        }

        async function fetchBluetooth() {
            if (btPolling || btBusy || chatBusy) return;
            const revision = btRevision;
            btPolling = true;
            try {
                const data = await btRequest('/api/bluetooth');
                if (revision === btRevision) btData = data;
            } catch (e) {
                if (revision === btRevision) {
                    btData = { esp32: false, conectado: false, escaneando: false, dispositivos: [], error: e.message };
                }
            } finally {
                btPolling = false;
                renderBluetooth();
            }
        }

        function btSetOptions(devs) {
            const previo = btSelect.value;
            // Actualizar las opciones existentes conserva la selección durante el sondeo.
            const addresses = new Set(devs.map(d => d.direccion));
            for (const opt of Array.from(btSelect.options)) {
                if (!addresses.has(opt.value)) opt.remove();
            }
            if (devs.length === 0) {
                const opt = document.createElement('option');
                opt.value = '';
                opt.textContent = !btData.esp32 ? 'ESP32 no disponible' : (btData.escaneando ? 'Buscando dispositivos...' : 'Sin dispositivos: pulsa Buscar');
                btSelect.appendChild(opt);
                return;
            }
            for (const d of devs) {
                let opt = Array.from(btSelect.options).find(o => o.value === d.direccion);
                if (!opt) {
                    opt = document.createElement('option');
                    opt.value = d.direccion;
                    btSelect.appendChild(opt);
                }
                const nombre = d.nombre && d.nombre.length ? d.nombre : d.direccion;
                const signal = Number.isFinite(d.rssi) && d.rssi < 0 ? ' · ' + d.rssi + ' dBm' : '';
                const label = nombre + ' (' + d.direccion + ')' + (d.conectado ? ' · conectado' : '') + signal;
                if (opt.textContent !== label) opt.textContent = label;
            }
            const conectado = devs.find(d => d.conectado);
            if (devs.some(d => d.direccion === previo)) btSelect.value = previo;
            else if (conectado) btSelect.value = conectado.direccion;
        }

        function renderBluetooth() {
            const d = btData;
            const devs = (d.dispositivos || []).slice().sort((a, b) => (a.nombre || a.direccion).localeCompare(b.nombre || b.direccion));
            btSetOptions(devs);

            let estado = 'Sin conexion', clase = '';
            if (btCommandError) { estado = btCommandError; clase = 'error'; }
            else if (!d.esp32) { estado = d.error || 'ESP32 no disponible'; clase = 'error'; }
            else if (d.error) { estado = d.error; clase = 'error'; }
            else if (d.conectado) { estado = 'Conectado a ' + (d.nombre || d.direccion); clase = 'ok'; }
            else if (d.objetivo) { estado = 'Conectando a ' + d.objetivo + '...'; }
            else if (d.escaneando) { estado = 'Buscando dispositivos cercanos...'; }
            else if (devs.length) { estado = 'Elige un dispositivo y pulsa Conectar'; }
            else { estado = 'Pulsa Buscar. El parlante debe estar encendido en modo emparejamiento.'; }

            const conectando = !!d.objetivo && !d.conectado;
            btState.textContent = d.conectado ? 'Conectado' : (conectando ? 'Conectando' : (d.escaneando ? 'Buscando' : 'Sin conexion'));
            btStatus.textContent = estado;
            btStatus.className = 'bt-status ' + clase;

            btSelect.disabled = chatBusy || btBusy || !d.esp32 || d.conectado || conectando || !devs.length;
            btScanBtn.disabled = chatBusy || btBusy || !d.esp32 || d.conectado || d.escaneando || conectando;
            btScanBtn.textContent = d.escaneando ? 'Buscando...' : 'Buscar';
            btConnectBtn.textContent = d.conectado ? 'Desconectar' : (conectando ? 'Cancelar conexión' : 'Conectar');
            btConnectBtn.disabled = chatBusy || btBusy || !d.esp32 || (!d.conectado && !conectando && !btSelect.value);
        }

        async function btCommand(path, payload) {
            if (btBusy) return;
            btRevision++;
            btCommandError = '';
            btBusy = true; renderBluetooth();
            try {
                const options = { method: 'POST' };
                if (payload) {
                    options.headers = { 'Content-Type': 'application/json' };
                    options.body = JSON.stringify(payload);
                }
                btData = await btRequest(path, options);
            } catch (e) { btCommandError = e.message; }
            finally { btBusy = false; renderBluetooth(); }
        }

        async function btScan() {
            await btCommand('/api/bluetooth/scan');
        }

        async function btToggleConnection() {
            if (btData.conectado || btData.objetivo) {
                await btCommand('/api/bluetooth/disconnect');
            } else if (btSelect.value) {
                await btCommand('/api/bluetooth/connect', { address: btSelect.value });
            }
        }

        btSelect.addEventListener('change', renderBluetooth);

        fetchDevices();
        setInterval(fetchDevices, 4000);
        fetchBluetooth();
        setInterval(fetchBluetooth, 2000);
    </script>
</body>
</html>
    )GIMENAI_HTML";
