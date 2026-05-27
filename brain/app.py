import ast
import operator
import os
import re
import tempfile
import threading
import time
import unicodedata
from pathlib import Path

try:
    import serial
except ImportError:
    serial = None

from flask import Flask, request, jsonify, render_template

try:
    import speech_recognition as sr
except ImportError:  # Permite que el modo dibujo siga funcionando aunque no esté instalada la librería.
    sr = None

try:
    from pydub import AudioSegment
except ImportError:
    AudioSegment = None

BASE_DIR = Path(__file__).resolve().parent
TEMPLATE_DIR = BASE_DIR / "templates" if (BASE_DIR / "templates").exists() else BASE_DIR
app = Flask(__name__, template_folder=str(TEMPLATE_DIR))

# Configura aquí el puerto al que está conectado tu ESP32.
PUERTO_SERIAL = os.getenv("ESP32_PORT", "COM6")
BAUD_RATE = int(os.getenv("ESP32_BAUD", "115200"))
STT_LANGUAGE = os.getenv("STT_LANGUAGE", "es-ES")

esp32 = None
serial_lock = threading.Lock()
if serial is None:
    print("Aviso: falta instalar pyserial. Instala con: pip install pyserial")
else:
    try:
        esp32 = serial.Serial(PUERTO_SERIAL, BAUD_RATE, timeout=2)
        time.sleep(2)  # Dar tiempo al ESP32 para reiniciar.
        print("Conectado al ESP32.")
    except Exception as e:
        print(f"Error: No se pudo conectar al puerto {PUERTO_SERIAL}. Revisa la conexión. Detalle: {e}")


def enviar_comando(cmd, timeout=60):
    """Envía un comando al ESP32 y bloquea hasta recibir confirmación OK o ERR."""
    if not esp32 or not esp32.is_open:
        return False, "ESP32 no conectado"

    with serial_lock:
        print(f"Enviando: {cmd}")
        try:
            esp32.reset_input_buffer()
            esp32.write((cmd + "\n").encode("utf-8"))
            inicio = time.time()
            while time.time() - inicio < timeout:
                respuesta = esp32.readline().decode("utf-8", errors="ignore").strip()
                if not respuesta:
                    continue
                print(f"ESP32: {respuesta}")
                if respuesta == "OK":
                    return True, "OK"
                if respuesta.startswith("ERR"):
                    return False, respuesta
            return False, "Tiempo de espera agotado esperando OK del ESP32"
        except Exception as e:
            return False, f"Error serial: {e}"


def enviar_secuencia(comandos, timeout_por_comando=60):
    for cmd in comandos:
        ok, msg = enviar_comando(cmd, timeout=timeout_por_comando)
        if not ok:
            return False, msg
    return True, "OK"


# ─────────────────────────────────────────────────────────────
# Reconocimiento e interpretación matemática
# ─────────────────────────────────────────────────────────────

UNITS = {
    "cero": 0, "zero": 0,
    "uno": 1, "un": 1, "una": 1, "one": 1,
    "dos": 2, "two": 2,
    "tres": 3, "three": 3,
    "cuatro": 4, "four": 4,
    "cinco": 5, "five": 5,
    "seis": 6, "six": 6,
    "siete": 7, "seven": 7,
    "ocho": 8, "eight": 8,
    "nueve": 9, "nine": 9,
    "diez": 10, "ten": 10,
    "once": 11, "eleven": 11,
    "doce": 12, "twelve": 12,
    "trece": 13, "thirteen": 13,
    "catorce": 14, "fourteen": 14,
    "quince": 15, "fifteen": 15,
    "dieciseis": 16, "sixteen": 16,
    "diecisiete": 17, "seventeen": 17,
    "dieciocho": 18, "eighteen": 18,
    "diecinueve": 19, "nineteen": 19,
    "veinte": 20, "twenty": 20,
    "veintiuno": 21, "veintiun": 21, "veintiuna": 21,
    "veintidos": 22,
    "veintitres": 23,
    "veinticuatro": 24,
    "veinticinco": 25,
    "veintiseis": 26,
    "veintisiete": 27,
    "veintiocho": 28,
    "veintinueve": 29,
}

TENS = {
    "treinta": 30, "thirty": 30,
    "cuarenta": 40, "forty": 40,
    "cincuenta": 50, "fifty": 50,
    "sesenta": 60, "sixty": 60,
    "setenta": 70, "seventy": 70,
    "ochenta": 80, "eighty": 80,
    "noventa": 90, "ninety": 90,
}

HUNDREDS = {
    "cien": 100,
    "ciento": 100,
    "doscientos": 200,
    "trescientos": 300,
    "cuatrocientos": 400,
    "quinientos": 500,
    "seiscientos": 600,
    "setecientos": 700,
    "ochocientos": 800,
    "novecientos": 900,
}

PHRASE_REPLACEMENTS = [
    ("abre parentesis", " ( "),
    ("abrir parentesis", " ( "),
    ("parentesis abierto", " ( "),
    ("cierra parentesis", " ) "),
    ("cerrar parentesis", " ) "),
    ("parentesis cerrado", " ) "),
    ("multiplicado por", " * "),
    ("multiplicado", " * "),
    ("dividido entre", " / "),
    ("dividido por", " / "),
    ("dividido", " / "),
    ("elevado a", " ** "),
    ("mas", " + "),
    ("plus", " + "),
    ("menos", " - "),
    ("minus", " - "),
    ("por", " * "),
    ("veces", " * "),
    ("times", " * "),
    ("entre", " / "),
    ("sobre", " / "),
    ("divided by", " / "),
    ("igual a", " = "),
    ("igual", " = "),
    ("equals", " = "),
    ("punto", " . "),
    ("coma", " . "),
    ("point", " . "),
]

FILLER_WORDS = {
    "cuanto", "cuanto", "cuanto", "cuanta", "es", "son", "calcula", "calcular",
    "resultado", "de", "la", "el", "operacion", "porfavor", "favor", "me", "da",
    "dame", "resuelve", "resolver", "what", "is", "calculate", "please", "the"
}

SAFE_OPERATORS = {
    ast.Add: operator.add,
    ast.Sub: operator.sub,
    ast.Mult: operator.mul,
    ast.Div: operator.truediv,
    ast.Pow: operator.pow,
    ast.USub: operator.neg,
    ast.UAdd: operator.pos,
}


def quitar_acentos(texto):
    texto = unicodedata.normalize("NFD", texto)
    return "".join(ch for ch in texto if unicodedata.category(ch) != "Mn")


def limpiar_texto_voz(texto):
    texto = quitar_acentos(texto.lower().strip())
    texto = texto.replace("×", " * ").replace("x", " * ").replace("÷", " / ")
    texto = texto.replace("^", " ** ")
    texto = re.sub(r"[^a-z0-9+\-*/().=\s]", " ", texto)
    for origen, destino in PHRASE_REPLACEMENTS:
        texto = re.sub(rf"\b{re.escape(origen)}\b", destino, texto)
    texto = re.sub(r"\s+", " ", texto).strip()
    return texto


def consumir_numero(tokens, i):
    """Convierte una secuencia de palabras numéricas a número. Devuelve (valor, nuevo_indice)."""
    inicio = i
    total = 0
    consumio = False

    if i < len(tokens) and re.fullmatch(r"\d+(?:\.\d+)?", tokens[i]):
        valor = tokens[i]
        i += 1
        if i + 1 < len(tokens) and tokens[i] == "." and re.fullmatch(r"\d+", tokens[i + 1]):
            valor = valor + "." + tokens[i + 1]
            i += 2
        return valor, i

    if i < len(tokens) and tokens[i] in HUNDREDS:
        total += HUNDREDS[tokens[i]]
        consumio = True
        i += 1

    if i < len(tokens) and tokens[i] in TENS:
        total += TENS[tokens[i]]
        consumio = True
        i += 1
        if i < len(tokens) and tokens[i] == "y":
            i += 1
        if i < len(tokens) and tokens[i] in UNITS and UNITS[tokens[i]] < 10:
            total += UNITS[tokens[i]]
            i += 1
    elif i < len(tokens) and tokens[i] in UNITS:
        total += UNITS[tokens[i]]
        consumio = True
        i += 1

    if not consumio:
        return None, inicio

    # Decimales dictados como: dos punto cinco / tres coma catorce.
    if i < len(tokens) and tokens[i] == ".":
        i += 1
        decimales = []
        while i < len(tokens):
            if re.fullmatch(r"\d+", tokens[i]):
                decimales.extend(list(tokens[i]))
                i += 1
            elif tokens[i] in UNITS and 0 <= UNITS[tokens[i]] <= 9:
                decimales.append(str(UNITS[tokens[i]]))
                i += 1
            else:
                break
        if decimales:
            return f"{total}." + "".join(decimales), i

    return str(total), i


def texto_a_expresion(texto):
    limpio = limpiar_texto_voz(texto)
    if "=" in limpio:
        # Si el usuario dice "igual", usamos solo la parte antes del igual.
        limpio = limpio.split("=", 1)[0]

    tokens = re.findall(r"\*\*|[()+\-*/.=]|\d+(?:\.\d+)?|[a-z]+", limpio)
    salida = []
    i = 0
    while i < len(tokens):
        token = tokens[i]

        numero, nuevo_i = consumir_numero(tokens, i)
        if numero is not None:
            salida.append(numero)
            i = nuevo_i
            continue

        if token in {"+", "-", "*", "/", "**", "(", ")", "."}:
            salida.append(token)
        elif token not in FILLER_WORDS and token != "y":
            raise ValueError(f"No entendí esta parte de la operación: '{token}'")
        i += 1

    expresion = "".join(salida)
    expresion = expresion.replace("**", "^DISPLAY^")
    expresion = re.sub(r"(?<!\*)\*(?!\*)", "*", expresion)
    expresion = expresion.replace("^DISPLAY^", "**")

    if not expresion or not re.search(r"\d", expresion):
        raise ValueError("No se detectó una operación matemática clara")
    if not re.fullmatch(r"[0-9+\-*/().]+", expresion):
        raise ValueError("La operación contiene símbolos no permitidos")
    if re.search(r"[+\-*/.]{3,}", expresion):
        raise ValueError("La operación parece incompleta o confusa")

    return expresion


def evaluar_ast(node):
    if isinstance(node, ast.Expression):
        return evaluar_ast(node.body)
    if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
        return node.value
    if isinstance(node, ast.BinOp) and type(node.op) in SAFE_OPERATORS:
        left = evaluar_ast(node.left)
        right = evaluar_ast(node.right)
        if isinstance(node.op, ast.Pow) and abs(right) > 6:
            raise ValueError("Exponente demasiado grande para este robot")
        return SAFE_OPERATORS[type(node.op)](left, right)
    if isinstance(node, ast.UnaryOp) and type(node.op) in SAFE_OPERATORS:
        return SAFE_OPERATORS[type(node.op)](evaluar_ast(node.operand))
    raise ValueError("Operación no permitida")


def resolver_expresion(expresion):
    try:
        arbol = ast.parse(expresion, mode="eval")
        resultado = evaluar_ast(arbol)
    except ZeroDivisionError:
        raise ValueError("No se puede dividir entre cero")
    except SyntaxError:
        raise ValueError("La operación matemática no está completa")

    if not isinstance(resultado, (int, float)):
        raise ValueError("Resultado no válido")
    if abs(resultado) > 999999:
        raise ValueError("Resultado demasiado grande para el área física del robot")
    return resultado


def formatear_numero(valor):
    if abs(valor - round(valor)) < 1e-9:
        return str(int(round(valor)))
    return f"{valor:.4f}".rstrip("0").rstrip(".")


def texto_para_robot(expresion, resultado):
    """Devuelve únicamente el resultado para que el robot dibuje solo el total."""
    return formatear_numero(resultado)


def reconocer_audio(archivo):
    if sr is None:
        raise RuntimeError("Falta instalar SpeechRecognition: pip install SpeechRecognition")
    if AudioSegment is None:
        raise RuntimeError("Falta instalar pydub: pip install pydub. También necesitas ffmpeg instalado.")

    suffix = Path(archivo.filename or "audio.webm").suffix or ".webm"
    with tempfile.TemporaryDirectory() as tmpdir:
        entrada = Path(tmpdir) / f"entrada{suffix}"
        wav = Path(tmpdir) / "audio.wav"
        archivo.save(entrada)

        try:
            AudioSegment.from_file(entrada).set_channels(1).set_frame_rate(16000).export(wav, format="wav")
        except Exception as e:
            raise RuntimeError(f"No se pudo convertir el audio a WAV. Revisa ffmpeg. Detalle: {e}")

        recognizer = sr.Recognizer()
        with sr.AudioFile(str(wav)) as source:
            audio = recognizer.record(source)

        try:
            return recognizer.recognize_google(audio, language=STT_LANGUAGE)
        except sr.UnknownValueError:
            raise ValueError("No se entendió claramente la voz")
        except sr.RequestError as e:
            raise RuntimeError(f"Error con el servicio de reconocimiento de voz: {e}")


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/print", methods=["POST"])
def print_drawing():
    data = request.json or {}
    trazos = data.get("trazos", [])

    ok, msg = enviar_comando("E")
    if not ok:
        return jsonify({"ok": False, "status": msg}), 500

    for trazo in trazos:
        if not trazo:
            continue

        primer_punto = trazo[0]
        comandos = [
            "U",
            f"M,{primer_punto['x']:.2f},{primer_punto['y']:.2f}",
            "D",
        ]
        for punto in trazo[1:]:
            comandos.append(f"M,{punto['x']:.2f},{punto['y']:.2f}")
        comandos.append("U")

        ok, msg = enviar_secuencia(comandos)
        if not ok:
            return jsonify({"ok": False, "status": msg}), 500

    ok, msg = enviar_comando("P")
    if not ok:
        return jsonify({"ok": False, "status": msg}), 500

    return jsonify({"ok": True, "status": "¡Dibujo completado con éxito!"})


@app.route("/calculate", methods=["POST"])
def calculate_from_voice():
    try:
        texto_detectado = None
        if "audio" in request.files:
            texto_detectado = reconocer_audio(request.files["audio"])
        else:
            data = request.json or {}
            texto_detectado = data.get("text", "")

        if not texto_detectado or not texto_detectado.strip():
            return jsonify({"ok": False, "status": "No se recibió audio o texto para analizar"}), 400

        expresion = texto_a_expresion(texto_detectado)
        resultado = resolver_expresion(expresion)
        resultado_str = formatear_numero(resultado)
        printable_text = texto_para_robot(expresion, resultado)

        return jsonify({
            "ok": True,
            "spoken_text": texto_detectado,
            "operation": expresion.replace("**", "^"),
            "result": resultado_str,
            "printable_text": printable_text,
            "status": "Operación detectada correctamente"
        })
    except Exception as e:
        return jsonify({"ok": False, "status": str(e)}), 400


@app.route("/print-calculation", methods=["POST"])
def print_calculation():
    data = request.json or {}
    printable_text = data.get("printable_text", "")
    printable_text = re.sub(r"[^0-9\-.]", "", printable_text)

    if not printable_text:
        return jsonify({"ok": False, "status": "No hay resultado válido para imprimir"}), 400

    comandos = ["E", f"C,{printable_text}", "P"]
    ok, msg = enviar_secuencia(comandos, timeout_por_comando=90)
    if not ok:
        return jsonify({"ok": False, "status": msg}), 500

    return jsonify({"ok": True, "status": "¡Resultado dibujado con éxito!"})


if __name__ == "__main__":
    # El host 0.0.0.0 permite que tu teléfono acceda usando la IP local de tu PC.
    app.run(host="0.0.0.0", port=5000)
