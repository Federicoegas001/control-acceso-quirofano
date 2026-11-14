"""Deteccion y reconocimiento de rostros con OpenCV (modelos YuNet y SFace)."""

import hashlib
import urllib.request
from pathlib import Path
from typing import List, Optional, Tuple

import cv2
import numpy as np

CARPETA_MODELOS = Path(__file__).parent / "modelos"
_BASE = "https://media.githubusercontent.com/media/opencv/opencv_zoo/main/models"

# nombre de archivo -> (URL de descarga, hash SHA-256 esperado)
MODELOS = {
    "face_detection_yunet_2023mar.onnx": (
        _BASE + "/face_detection_yunet/face_detection_yunet_2023mar.onnx",
        "8f2383e4dd3cfbb4553ea8718107fc0423210dc964f9f4280604804ed2552fa4",
    ),
    "face_recognition_sface_2021dec.onnx": (
        _BASE + "/face_recognition_sface/face_recognition_sface_2021dec.onnx",
        "0ba9fbfa01b5270c96627c4ef784da859931e02f04419c829e83484087c34e79",
    ),
}

# Similitud coseno minima para aceptar que dos rostros son la misma persona.
# 0.363 es el valor recomendado por los autores de SFace. Subirlo hace el
# sistema mas estricto (menos falsos positivos, mas rechazos).
UMBRAL_SIMILITUD = 0.363

# Un rostro mas chico que esto (en pixeles de ancho) esta demasiado lejos.
ANCHO_MINIMO_ROSTRO = 60

# Orientaciones a probar, en orden: como llega, cabeza abajo y de costado.
GIROS = (None, cv2.ROTATE_180, cv2.ROTATE_90_CLOCKWISE, cv2.ROTATE_90_COUNTERCLOCKWISE)


def _hash_archivo(ruta: Path) -> str:
    sha = hashlib.sha256()
    with open(ruta, "rb") as archivo:
        for bloque in iter(lambda: archivo.read(1 << 20), b""):
            sha.update(bloque)
    return sha.hexdigest()


def asegurar_modelos() -> None:
    """Descarga los modelos la primera vez y verifica que no esten corruptos."""
    CARPETA_MODELOS.mkdir(exist_ok=True)
    for nombre, (url, hash_esperado) in MODELOS.items():
        ruta = CARPETA_MODELOS / nombre
        if ruta.exists() and _hash_archivo(ruta) == hash_esperado:
            continue
        print("Descargando modelo " + nombre + " ...")
        urllib.request.urlretrieve(url, ruta)
        if _hash_archivo(ruta) != hash_esperado:
            ruta.unlink()
            raise RuntimeError("El modelo " + nombre + " se descargo corrupto. Reintentar.")


class Reconocedor:
    def __init__(self) -> None:
        asegurar_modelos()
        self.detector = cv2.FaceDetectorYN.create(
            str(CARPETA_MODELOS / "face_detection_yunet_2023mar.onnx"),
            "",
            (320, 320),
            score_threshold=0.8,
        )
        self.reconocedor = cv2.FaceRecognizerSF.create(
            str(CARPETA_MODELOS / "face_recognition_sface_2021dec.onnx"), ""
        )

    def extraer_rostro(self, imagen_bytes: bytes) -> Tuple[Optional[np.ndarray], str]:
        """Devuelve (vector, motivo).

        Si hay un rostro utilizable, vector es un arreglo de 128 numeros que lo
        describe y motivo es "ok". Si no, vector es None y motivo explica por que.
        """
        imagen = cv2.imdecode(np.frombuffer(imagen_bytes, dtype=np.uint8), cv2.IMREAD_COLOR)
        if imagen is None:
            return None, "imagen_invalida"

        # La camara puede estar montada girada o cabeza abajo: si no aparece un
        # rostro con la imagen como llega, se prueba con las otras orientaciones.
        rostros = None
        for giro in GIROS:
            candidata = imagen if giro is None else cv2.rotate(imagen, giro)
            alto, ancho = candidata.shape[:2]
            self.detector.setInputSize((ancho, alto))
            _, rostros = self.detector.detect(candidata)
            if rostros is not None and len(rostros) > 0:
                imagen = candidata
                break
        if rostros is None or len(rostros) == 0:
            return None, "sin_rostro"

        # Si hay varias personas, se usa la mas cercana (el rostro mas grande).
        rostro = max(rostros, key=lambda r: r[2] * r[3])
        if rostro[2] < ANCHO_MINIMO_ROSTRO:
            return None, "rostro_muy_lejos"

        alineado = self.reconocedor.alignCrop(imagen, rostro)
        vector = self.reconocedor.feature(alineado).flatten().astype(np.float32)
        norma = float(np.linalg.norm(vector))
        if norma == 0:
            return None, "sin_rostro"
        return vector / norma, "ok"

    @staticmethod
    def buscar_coincidencia(vector: np.ndarray, conocidos: List[dict]) -> Tuple[Optional[dict], float]:
        """Compara contra los rostros registrados.

        Devuelve (medico, similitud) del mas parecido; medico es None si ninguno
        supera el umbral.
        """
        mejor = None
        mejor_similitud = -1.0
        for conocido in conocidos:
            similitud = float(np.dot(vector, conocido["vector"]))
            if similitud > mejor_similitud:
                mejor, mejor_similitud = conocido, similitud
        if mejor is None or mejor_similitud < UMBRAL_SIMILITUD:
            return None, max(mejor_similitud, 0.0)
        return mejor, mejor_similitud
