"""
Lanzador de GimenAI.

Escucha en UDP el aviso "GIMENAI_DISCOVER" que el ESP32 envía por broadcast al
encenderse. Si el servidor (main.py) no está corriendo, lo arranca, y responde
"GIMENAI_SERVER <puerto>" para que el ESP32 sepa la IP de este equipo.

Se ejecuta al iniciar sesión en Windows (ver install_autostart.ps1).
"""
import os
import socket
import subprocess
import sys
import time
import urllib.request

DISCOVERY_PORT = 4210
SERVER_PORT = int(os.getenv("PORT", 8000))
HERE = os.path.dirname(os.path.abspath(__file__))
LOG_PATH = os.path.join(HERE, "launcher.log")

server_proc = None


def log(msg: str) -> None:
    try:
        with open(LOG_PATH, "a", encoding="utf-8") as f:
            f.write(f"{time.strftime('%Y-%m-%d %H:%M:%S')} {msg}\n")
    except OSError:
        pass


def server_responde() -> bool:
    try:
        urllib.request.urlopen(f"http://127.0.0.1:{SERVER_PORT}/api/devices", timeout=1.5)
        return True
    except Exception:
        return False


def python_con_consola() -> str:
    """pythonw no sirve para uvicorn (no tiene stdout): usar python.exe sin ventana."""
    exe = sys.executable
    if exe.lower().endswith("pythonw.exe"):
        candidato = exe[:-len("pythonw.exe")] + "python.exe"
        if os.path.exists(candidato):
            return candidato
    return exe


def arrancar_servidor() -> None:
    global server_proc
    if server_proc is not None and server_proc.poll() is None:
        return  # ya lo arrancamos y sigue vivo (puede estar iniciando)
    if server_responde():
        return  # lo arrancó alguien más
    log("Arrancando servidor main.py")
    salida = open(os.path.join(HERE, "server.log"), "a", encoding="utf-8")
    flags = 0
    if os.name == "nt":
        flags = subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP
    server_proc = subprocess.Popen(
        [python_con_consola(), "main.py"],
        cwd=HERE,
        stdout=salida,
        stderr=subprocess.STDOUT,
        stdin=subprocess.DEVNULL,
        creationflags=flags,
    )


def main() -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", DISCOVERY_PORT))
    log(f"Lanzador escuchando en UDP {DISCOVERY_PORT}")
    while True:
        try:
            data, addr = sock.recvfrom(256)
        except OSError as e:
            log(f"Error de socket: {e}")
            time.sleep(1)
            continue
        if data.startswith(b"GIMENAI_DISCOVER"):
            log(f"ESP32 detectado en {addr[0]}")
            arrancar_servidor()
            sock.sendto(f"GIMENAI_SERVER {SERVER_PORT}".encode(), addr)


if __name__ == "__main__":
    main()
