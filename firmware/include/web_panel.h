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
            
            <button class="btn-action" id="voice-btn" onclick="toggleVoice()" title="Dónde suena la voz de Gimena">Voz: este equipo</button>
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

        // Voz: 'gemini' = voz de Gemini en este equipo; 'fast' = voz del propio navegador (instantánea);
        //       'device' = parlante de Gimena (amplificador I2S)
        const VOICE_LABELS = { gemini: 'Voz: Gemini aquí', fast: 'Voz: rápida aquí', device: 'Voz: parlante Gimena' };
        const VOICE_ORDER = ['gemini', 'fast', 'device'];
        const voiceBtn = document.getElementById('voice-btn');
        let voiceMode = 'gemini';
        try { voiceMode = localStorage.getItem('gimena-voz') || 'gemini'; } catch(e) {}
        if (!VOICE_LABELS[voiceMode]) voiceMode = 'gemini';
        let audioCtx = null;

        function renderVoiceButton() { voiceBtn.textContent = VOICE_LABELS[voiceMode]; }

        function toggleVoice() {
            voiceMode = VOICE_ORDER[(VOICE_ORDER.indexOf(voiceMode) + 1) % VOICE_ORDER.length];
            try { localStorage.setItem('gimena-voz', voiceMode); } catch(e) {}
            renderVoiceButton();
        }

        // Debe llamarse dentro del clic: los celulares solo permiten audio iniciado por el usuario
        function unlockAudio() {
            if (voiceMode === 'fast') {
                if (window.speechSynthesis) speechSynthesis.speak(new SpeechSynthesisUtterance(''));
                return;
            }
            const Ctx = window.AudioContext || window.webkitAudioContext;
            if (!Ctx) return;
            if (!audioCtx) audioCtx = new Ctx();
            if (audioCtx.state === 'suspended') audioCtx.resume();
        }

        function speakFast(text) {
            return new Promise(resolve => {
                if (!window.speechSynthesis) return resolve();
                const u = new SpeechSynthesisUtterance(text);
                u.lang = 'es-CO';
                const voice = speechSynthesis.getVoices().find(v => v.lang.replace('_', '-').startsWith('es-CO')) ||
                              speechSynthesis.getVoices().find(v => v.lang.startsWith('es'));
                if (voice) u.voice = voice;
                u.onend = u.onerror = () => resolve();
                speechSynthesis.cancel();
                speechSynthesis.speak(u);
            });
        }

        // La placa envía PCM 16 bit mono a 24 kHz tal como lo genera Gemini TTS, más lento que
        // tiempo real (la ventana TCP del ESP32 lo limita). Se empieza a reproducir cuando lo ya
        // recibido alcanza para no cortarse, estimando la duración total por el largo del texto.
        async function playBrowserVoice(job, text) {
            if (!audioCtx) throw new Error('Este navegador no permite reproducir audio.');
            const res = await fetch('/api/chat/voice?job=' + job, { cache: 'no-store' });
            if (!res.ok) {
                let msg = 'No se pudo generar la voz';
                try { msg = (await res.json()).error || msg; } catch(e) {}
                throw new Error(msg);
            }
            const RATE = 24000, BYTES_PER_SEC = RATE * 2;
            const estimatedSec = Math.max(1.5, text.length / 14);
            const reader = res.body.getReader();
            let pending = new Uint8Array(0), total = 0, firstAt = 0, started = false, nextTime = 0;

            const schedule = () => {
                const usable = pending.length - (pending.length % 2);   // muestras completas de 16 bit
                if (usable < 2) return;
                const pcm = new Int16Array(pending.buffer.slice(pending.byteOffset, pending.byteOffset + usable));
                pending = pending.slice(usable);
                const buffer = audioCtx.createBuffer(1, pcm.length, RATE);
                const channel = buffer.getChannelData(0);
                for (let i = 0; i < pcm.length; i++) channel[i] = pcm[i] / 32768;
                const source = audioCtx.createBufferSource();
                source.buffer = buffer;
                source.connect(audioCtx.destination);
                nextTime = Math.max(nextTime, audioCtx.currentTime + 0.05);
                source.start(nextTime);
                nextTime += buffer.duration;
            };

            while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                if (!firstAt) firstAt = performance.now();
                const merged = new Uint8Array(pending.length + value.length);
                merged.set(pending); merged.set(value, pending.length);
                pending = merged;
                total += value.length;
                const receivedSec = total / BYTES_PER_SEC;
                if (!started) {
                    const elapsed = (performance.now() - firstAt) / 1000;
                    const speed = elapsed > 0.3 ? receivedSec / elapsed : 0;   // segundos de audio por segundo
                    const remaining = Math.max(0, estimatedSec - receivedSec);
                    const needed = speed >= 1 ? 0.3 : speed > 0 ? remaining * (1 / speed - 1) * 1.15 + 0.3 : Infinity;
                    if (receivedSec >= needed) started = true;
                }
                if (started && pending.length >= BYTES_PER_SEC / 4) schedule();   // bloques de ~0,25 s
            }
            schedule();
            if (!total) throw new Error('Gemini no devolvió audio.');
            const wait = Math.max(0, nextTime - audioCtx.currentTime);
            await new Promise(resolve => setTimeout(resolve, wait * 1000 + 100));
        }

        renderVoiceButton();

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
                const targetLabel = dev.target === 'esp32_hardware' ? 'ESP32 GPIO ' + dev.pin
                    : dev.target === 'tuya' ? 'Smart Life' + (dev.online === false ? ' · sin conexion' : dev.power != null ? ' · ' + dev.power + ' W' : '')
                    : 'Virtual';
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
            const mode = voiceMode;
            const browserVoice = mode !== 'device';
            if (browserVoice) unlockAudio();
            input.value = '';
            chatBusy = true;
            input.disabled = true;
            const sendButton = document.querySelector('.btn-send');
            sendButton.disabled = true;
            sendButton.textContent = 'Pensando...';
            appendMessage('user', text);
            let shown = false, replyText = '';
            try {
                const job = await cloudRequest('/api/chat', {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ message: text, voice: browserVoice ? 'browser' : 'device' })
                });
                const deadline = Date.now() + 180000;
                while (true) {
                    const data = await cloudRequest('/api/chat/status?job=' + job.job);
                    if (data.reply && !shown) {
                        appendMessage('bot', data.reply);
                        replyText = data.reply;
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
                if (browserVoice && shown) {
                    try {
                        if (mode === 'fast') await speakFast(replyText);
                        else await playBrowserVoice(job.job, replyText);
                    } catch(e) { appendMessage('bot', 'No pude reproducir la voz: ' + e.message); }
                }
            } catch(e) {
                appendMessage('bot', e.name === 'AbortError' ? 'El ESP32 tardó demasiado en responder.' : e.message);
            } finally {
                chatBusy = false;
                input.disabled = false;
                sendButton.disabled = false;
                sendButton.textContent = 'Enviar';
                fetchDevices();
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

        fetchDevices();
        setInterval(fetchDevices, 4000);
    </script>
</body>
</html>
    )GIMENAI_HTML";
