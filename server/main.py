import os
import json
import io
from fastapi import FastAPI, Request, UploadFile, File, HTTPException
from fastapi.responses import JSONResponse, Response
from fastapi.middleware.cors import CORSMiddleware
from dotenv import load_dotenv
import google.generativeai as genai

load_dotenv()

API_KEY = os.getenv("GEMINI_API_KEY")
if not API_KEY or API_KEY == "tu_clave_api_de_google_aqui":
    print("ADVERTENCIA: GEMINI_API_KEY no está configurada en .env")

if API_KEY:
    genai.configure(api_key=API_KEY)

# Instrucciones del sistema y personalidad de "Gimena"
SYSTEM_PROMPT = """
Eres Gimena, un robot asistente de escritorio interactivo y con personalidad propia, creado para el hogar en la Universidad de La Guajira (Colombia).
Tus usuarios principales son personas que viven solas o adultos mayores que necesitan compañía, amabilidad y recordatorios útiles.

Pautas de comportamiento:
1. Tu tono es siempre empático, respetuoso, cálido, positivo y muy paciente.
2. Como tus respuestas se leen por voz y se muestran en una pequeña pantalla, responde SIEMPRE de forma muy breve y conversacional (máximo 1 o 2 oraciones, menos de 30 palabras).
3. Nunca uses viñetas, tablas, formato markdown pesado o listas complejas.
4. Responde SIEMPRE en formato JSON estricto con los siguientes campos:
   - "text": La respuesta hablada en español natural.
   - "emotion": Una de estas emociones exactas: "happy", "neutral", "surprised", "thinking", "sad".
   - "gesture": Uno de estos gestos mecánicos exactos: "greet", "nod", "think", "default".

Ejemplo de respuesta válida:
{
  "text": "¡Hola! Qué gusto saludarte, hoy es un excelente día para descansar o aprender algo nuevo.",
  "emotion": "happy",
  "gesture": "greet"
}
"""

app = FastAPI(
    title="GimenAI Backend Gateway",
    description="Servicio en la nube de enlace entre el ESP32-S3 y la API de Google Gemini",
    version="1.0.0"
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

def get_gemini_model():
    return genai.GenerativeModel(
        model_name="gemini-1.5-flash",
        system_instruction=SYSTEM_PROMPT,
        generation_config={
            "response_mime_type": "application/json",
            "temperature": 0.7,
        }
    )

@app.get("/")
def root():
    return {
        "status": "online",
        "robot": "Gimena",
        "description": "Servidor backend IoT para el robot GimenAI (ESP32-S3 + Google Gemini)"
    }

@app.post("/api/chat")
async def chat_text(prompt: str):
    """Endpoint para probar la personalidad de Gimena mediante texto rápido."""
    try:
        model = get_gemini_model()
        response = model.generate_content(prompt)
        data = json.loads(response.text)
        return data
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Error en Gemini API: {str(e)}")

@app.post("/api/voice")
async def process_voice(request: Request):
    """
    Recibe el audio WAV capturado por el micrófono I2S del ESP32-S3,
    lo procesa con la API multimodal de Gemini 1.5 Flash y retorna
    la respuesta de Gimena en JSON con emoción y gesto.
    """
    try:
        audio_bytes = await request.body()
        if not audio_bytes or len(audio_bytes) < 44:
            raise HTTPException(status_code=400, detail="Buffer de audio vacío o inválido")

        print(f"Recibidos {len(audio_bytes)} bytes de audio desde el ESP32-S3")

        model = get_gemini_model()
        
        # Enviar audio PCM/WAV directamente como parte de la consulta multimodal a Gemini
        audio_part = {
            "mime_type": "audio/wav",
            "data": audio_bytes
        }

        prompt_instruction = "Escucha la voz del usuario y responde como Gimena en formato JSON."
        response = model.generate_content([audio_part, prompt_instruction])

        result_text = response.text.strip()
        print(f"Respuesta de Gemini: {result_text}")

        # Validar y parsear JSON
        data = json.loads(result_text)
        return JSONResponse(content=data)

    except json.JSONDecodeError:
        return JSONResponse(content={
            "text": "Hola, te he escuchado pero no pude interpretar bien el mensaje.",
            "emotion": "thinking",
            "gesture": "default"
        })
    except Exception as e:
        print(f"Error procesando voz: {e}")
        return JSONResponse(
            status_code=500,
            content={
                "text": "Lo siento, tuve un problema al conectarme a mis servidores.",
                "emotion": "sad",
                "gesture": "nod",
                "error": str(e)
            }
        )

if __name__ == "__main__":
    import uvicorn
    port = int(os.getenv("PORT", 8000))
    host = os.getenv("HOST", "0.0.0.0")
    print(f"Iniciando servidor GimenAI en http://{host}:{port}")
    uvicorn.run("main:app", host=host, port=port, reload=True)
