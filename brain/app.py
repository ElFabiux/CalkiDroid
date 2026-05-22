import time
import serial
from flask import Flask, request, jsonify, render_template

app = Flask(__name__)

# Configura aquí el puerto al que está conectado tu ESP32
PUERTO_SERIAL = 'COM6' 
BAUD_RATE = 115200

esp32 = None
try:
    esp32 = serial.Serial(PUERTO_SERIAL, BAUD_RATE, timeout=2)
    time.sleep(2) # Dar tiempo al ESP32 para reiniciar
    print("Conectado al ESP32.")
except Exception as e:
    print(f"Error: No se pudo conectar al puerto {PUERTO_SERIAL}. Revisa la conexión.")

def enviar_comando(cmd):
    """Envía un comando al ESP32 y bloquea hasta recibir confirmación 'OK'"""
    if esp32:
        print(f"Enviando: {cmd}")
        esp32.write((cmd + '\n').encode('utf-8'))
        while True:
            respuesta = esp32.readline().decode('utf-8').strip()
            if respuesta == "OK":
                break

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/print', methods=['POST'])
def print_drawing():
    data = request.json
    trazos = data.get('trazos', [])
    
    # 1. Ejecutar secuencia de borrado y posicionar en parking
    enviar_comando("E")
    
    # 2. Dibujar cada trazo procesado
    for trazo in trazos:
        if not trazo: continue
        
        primer_punto = trazo[0]
        enviar_comando("U") # Lápiz Arriba
        enviar_comando(f"M,{primer_punto['x']:.2f},{primer_punto['y']:.2f}") # Mover a inicio
        enviar_comando("D") # Lápiz Abajo
        
        # Recorrer el resto del trazo
        for punto in trazo[1:]:
            enviar_comando(f"M,{punto['x']:.2f},{punto['y']:.2f}")
            
        enviar_comando("U") # Levantar al terminar la línea
        
    # 3. Regresar al Parking y apagar motores
    enviar_comando("P")
    
    return jsonify({"status": "¡Dibujo completado con éxito!"})

if __name__ == '__main__':
    # El host '0.0.0.0' permite que tu teléfono acceda a la app usando la IP local de tu PC
    app.run(host='0.0.0.0', port=5000)