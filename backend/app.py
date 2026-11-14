"""Servidor del control de acceso: recibe fotos del ESP32-CAM y decide el acceso."""

from datetime import datetime
from pathlib import Path

from flask import Flask, jsonify, request

import database
from reconocimiento import Reconocedor

app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = 2 * 1024 * 1024  # ninguna foto pesa mas de 2 MB

CARPETA_FOTOS = Path(__file__).parent / "registros"
CARPETA_FOTOS.mkdir(exist_ok=True)
MAXIMO_FOTOS_GUARDADAS = 200

database.inicializar()
reconocedor = Reconocedor()


def guardar_foto(imagen: bytes) -> str:
    nombre = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3] + ".jpg"
    (CARPETA_FOTOS / nombre).write_bytes(imagen)
    # Solo se conservan las fotos mas recientes
    fotos = sorted(CARPETA_FOTOS.glob("*.jpg"))
    for vieja in fotos[:-MAXIMO_FOTOS_GUARDADAS]:
        vieja.unlink()
    return nombre


def respuesta(status: str, user=None, authorized: bool = False, **extra):
    datos = {"status": status, "user": user, "authorized": authorized}
    datos.update(extra)
    return jsonify(datos)


@app.get("/")
def estado():
    return jsonify({"status": "ok", "mensaje": "Servidor de control de acceso activo"})


@app.post("/api/acceso")
def evaluar_acceso():
    imagen = request.get_data()

    # Un JPEG siempre empieza con los bytes FF D8
    if not imagen.startswith(b"\xff\xd8"):
        return respuesta("error", motivo="imagen_invalida"), 400

    foto = guardar_foto(imagen)
    vector, motivo = reconocedor.extraer_rostro(imagen)

    # --- Modo registro: la foto se usa para dar de alta a un medico ---
    pendiente = database.obtener_registro_pendiente()
    if pendiente:
        if vector is None:
            print("Registro de " + pendiente["nombre"] + ": foto descartada (" + motivo + ")")
            return respuesta("enrolling", user=pendiente["nombre"], motivo=motivo,
                             restantes=pendiente["fotos_restantes"])
        database.agregar_rostro(pendiente["medico_id"], vector)
        restantes = database.descontar_registro()
        print("Registro de " + pendiente["nombre"] + ": rostro guardado, faltan " + str(restantes))
        return respuesta("enrolling", user=pendiente["nombre"], motivo="rostro_guardado",
                         restantes=restantes)

    # --- Modo normal: decidir el acceso. Ante cualquier duda, se deniega ---
    if vector is None:
        database.registrar_acceso("denegado", motivo, foto=foto)
        print("DENEGADO (" + motivo + ")")
        return respuesta("denied", motivo=motivo)

    medico, similitud = reconocedor.buscar_coincidencia(vector, database.cargar_rostros())

    if medico is None:
        database.registrar_acceso("denegado", "desconocido", similitud=similitud, foto=foto)
        print("DENEGADO (desconocido, similitud %.2f)" % similitud)
        return respuesta("denied", motivo="desconocido")

    if not medico["autorizado"]:
        database.registrar_acceso("denegado", "sin_permiso", medico["medico_id"],
                                  medico["nombre"], similitud, foto)
        print("DENEGADO (" + medico["nombre"] + " no tiene permiso)")
        return respuesta("denied", user=medico["nombre"], motivo="sin_permiso")

    database.registrar_acceso("permitido", "reconocido", medico["medico_id"],
                              medico["nombre"], similitud, foto)
    print("PERMITIDO: %s (similitud %.2f)" % (medico["nombre"], similitud))
    return respuesta("ok", user=medico["nombre"], authorized=True)


@app.get("/api/medicos")
def listar_medicos():
    return jsonify(database.listar_medicos())


@app.get("/api/accesos")
def listar_accesos():
    limite = min(request.args.get("limite", default=50, type=int), 500)
    return jsonify(database.listar_accesos(limite))


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5001, debug=True)
