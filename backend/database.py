"""Acceso a la base de datos SQLite: medicos, rostros registrados e historial."""

import sqlite3
from datetime import datetime
from pathlib import Path
from typing import List, Optional

import numpy as np

RUTA_DB = Path(__file__).parent / "acceso.db"


def conectar() -> sqlite3.Connection:
    con = sqlite3.connect(RUTA_DB)
    con.row_factory = sqlite3.Row
    con.execute("PRAGMA foreign_keys = ON")
    return con


def ahora() -> str:
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def inicializar() -> None:
    """Crea las tablas si todavia no existen."""
    with conectar() as con:
        con.executescript(
            """
            CREATE TABLE IF NOT EXISTS medicos (
                id         INTEGER PRIMARY KEY AUTOINCREMENT,
                nombre     TEXT NOT NULL UNIQUE,
                autorizado INTEGER NOT NULL DEFAULT 1,
                creado     TEXT NOT NULL
            );

            -- Un medico puede tener varios rostros de referencia.
            -- Se guarda el vector numerico del rostro, no la foto.
            CREATE TABLE IF NOT EXISTS rostros (
                id        INTEGER PRIMARY KEY AUTOINCREMENT,
                medico_id INTEGER NOT NULL REFERENCES medicos(id) ON DELETE CASCADE,
                vector    BLOB NOT NULL,
                creado    TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS accesos (
                id        INTEGER PRIMARY KEY AUTOINCREMENT,
                fecha     TEXT NOT NULL,
                medico_id INTEGER REFERENCES medicos(id) ON DELETE SET NULL,
                nombre    TEXT,
                resultado TEXT NOT NULL,   -- 'permitido' o 'denegado'
                motivo    TEXT NOT NULL,
                similitud REAL,
                foto      TEXT
            );

            -- Como maximo una fila: el medico que se esta registrando ahora.
            CREATE TABLE IF NOT EXISTS registro_pendiente (
                id              INTEGER PRIMARY KEY CHECK (id = 1),
                medico_id       INTEGER NOT NULL REFERENCES medicos(id) ON DELETE CASCADE,
                fotos_restantes INTEGER NOT NULL
            );
            """
        )


# ---------- Medicos ----------

def crear_medico(nombre: str) -> int:
    """Devuelve el id del medico; lo crea si no existe."""
    nombre = nombre.strip()
    with conectar() as con:
        fila = con.execute("SELECT id FROM medicos WHERE nombre = ?", (nombre,)).fetchone()
        if fila:
            return fila["id"]
        cursor = con.execute(
            "INSERT INTO medicos (nombre, autorizado, creado) VALUES (?, 1, ?)",
            (nombre, ahora()),
        )
        return cursor.lastrowid


def obtener_medico(medico_id: int) -> Optional[dict]:
    with conectar() as con:
        fila = con.execute("SELECT * FROM medicos WHERE id = ?", (medico_id,)).fetchone()
        return dict(fila) if fila else None


def listar_medicos() -> List[dict]:
    with conectar() as con:
        filas = con.execute(
            """
            SELECT m.id, m.nombre, m.autorizado, m.creado, COUNT(r.id) AS rostros
            FROM medicos m LEFT JOIN rostros r ON r.medico_id = m.id
            GROUP BY m.id ORDER BY m.nombre
            """
        ).fetchall()
        return [dict(f) for f in filas]


def cambiar_autorizacion(medico_id: int, autorizado: bool) -> bool:
    with conectar() as con:
        cursor = con.execute(
            "UPDATE medicos SET autorizado = ? WHERE id = ?", (int(autorizado), medico_id)
        )
        return cursor.rowcount > 0


def eliminar_medico(medico_id: int) -> bool:
    with conectar() as con:
        cursor = con.execute("DELETE FROM medicos WHERE id = ?", (medico_id,))
        return cursor.rowcount > 0


# ---------- Rostros ----------

def agregar_rostro(medico_id: int, vector: np.ndarray) -> None:
    with conectar() as con:
        con.execute(
            "INSERT INTO rostros (medico_id, vector, creado) VALUES (?, ?, ?)",
            (medico_id, vector.astype(np.float32).tobytes(), ahora()),
        )


def cargar_rostros() -> List[dict]:
    """Todos los rostros registrados, con los datos de su medico."""
    with conectar() as con:
        filas = con.execute(
            """
            SELECT r.vector, m.id AS medico_id, m.nombre, m.autorizado
            FROM rostros r JOIN medicos m ON m.id = r.medico_id
            """
        ).fetchall()
    return [
        {
            "medico_id": f["medico_id"],
            "nombre": f["nombre"],
            "autorizado": bool(f["autorizado"]),
            "vector": np.frombuffer(f["vector"], dtype=np.float32),
        }
        for f in filas
    ]


# ---------- Registro de un medico nuevo ----------

def iniciar_registro(medico_id: int, fotos: int) -> None:
    with conectar() as con:
        con.execute("DELETE FROM registro_pendiente")
        con.execute(
            "INSERT INTO registro_pendiente (id, medico_id, fotos_restantes) VALUES (1, ?, ?)",
            (medico_id, fotos),
        )


def obtener_registro_pendiente() -> Optional[dict]:
    with conectar() as con:
        fila = con.execute(
            """
            SELECT p.medico_id, p.fotos_restantes, m.nombre
            FROM registro_pendiente p JOIN medicos m ON m.id = p.medico_id
            """
        ).fetchone()
        return dict(fila) if fila else None


def descontar_registro() -> int:
    """Resta una foto pendiente y devuelve cuantas faltan."""
    with conectar() as con:
        con.execute("UPDATE registro_pendiente SET fotos_restantes = fotos_restantes - 1")
        fila = con.execute("SELECT fotos_restantes FROM registro_pendiente").fetchone()
        restantes = fila["fotos_restantes"] if fila else 0
        if restantes <= 0:
            con.execute("DELETE FROM registro_pendiente")
        return max(restantes, 0)


def cancelar_registro() -> None:
    with conectar() as con:
        con.execute("DELETE FROM registro_pendiente")


# ---------- Historial ----------

def registrar_acceso(
    resultado: str,
    motivo: str,
    medico_id: Optional[int] = None,
    nombre: Optional[str] = None,
    similitud: Optional[float] = None,
    foto: Optional[str] = None,
) -> None:
    with conectar() as con:
        con.execute(
            """
            INSERT INTO accesos (fecha, medico_id, nombre, resultado, motivo, similitud, foto)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            """,
            (ahora(), medico_id, nombre, resultado, motivo, similitud, foto),
        )


def listar_accesos(limite: int = 50) -> List[dict]:
    with conectar() as con:
        filas = con.execute(
            "SELECT * FROM accesos ORDER BY id DESC LIMIT ?", (limite,)
        ).fetchall()
        return [dict(f) for f in filas]
