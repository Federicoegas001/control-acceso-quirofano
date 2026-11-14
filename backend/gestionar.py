"""Herramienta de linea de comandos para administrar medicos y ver el historial.

Ejemplos:
    python gestionar.py registrar "Dr. Perez"
    python gestionar.py registrar "Dra. Gomez" --archivo foto.jpg
    python gestionar.py listar
    python gestionar.py revocar 2
    python gestionar.py autorizar 2
    python gestionar.py eliminar 2
    python gestionar.py historial
"""

import argparse
import sys
from pathlib import Path

import database


def cmd_registrar(args) -> None:
    medico_id = database.crear_medico(args.nombre)

    if args.archivo:
        # Alta a partir de una foto guardada en la computadora
        from reconocimiento import Reconocedor

        vector, motivo = Reconocedor().extraer_rostro(Path(args.archivo).read_bytes())
        if vector is None:
            sys.exit("No se pudo usar la foto: " + motivo)
        database.agregar_rostro(medico_id, vector)
        print("Rostro de " + args.nombre + " registrado desde " + args.archivo)
        return

    # Alta con la camara: las proximas fotos del ESP32 se usan como referencia
    database.iniciar_registro(medico_id, args.fotos)
    print("Registro iniciado para " + args.nombre + " (id " + str(medico_id) + ").")
    print("Las proximas " + str(args.fotos) + " fotos con un rostro se guardan como referencia.")
    print("Que la persona mire a la camara; el avance se ve en la ventana del servidor.")


def cmd_cancelar(args) -> None:
    database.cancelar_registro()
    print("Registro pendiente cancelado.")


def cmd_listar(args) -> None:
    medicos = database.listar_medicos()
    if not medicos:
        print("No hay medicos registrados.")
        return
    print("%-4s %-28s %-12s %s" % ("ID", "NOMBRE", "AUTORIZADO", "ROSTROS"))
    for m in medicos:
        print("%-4d %-28s %-12s %d" % (m["id"], m["nombre"], "si" if m["autorizado"] else "no", m["rostros"]))


def _cambiar_permiso(medico_id: int, autorizado: bool) -> None:
    if not database.cambiar_autorizacion(medico_id, autorizado):
        sys.exit("No existe un medico con id " + str(medico_id))
    medico = database.obtener_medico(medico_id)
    print(medico["nombre"] + (": acceso autorizado." if autorizado else ": acceso revocado."))


def cmd_autorizar(args) -> None:
    _cambiar_permiso(args.id, True)


def cmd_revocar(args) -> None:
    _cambiar_permiso(args.id, False)


def cmd_eliminar(args) -> None:
    medico = database.obtener_medico(args.id)
    if not medico:
        sys.exit("No existe un medico con id " + str(args.id))
    database.eliminar_medico(args.id)
    print(medico["nombre"] + " eliminado junto con sus rostros.")


def cmd_historial(args) -> None:
    accesos = database.listar_accesos(args.cantidad)
    if not accesos:
        print("Todavia no hay intentos de acceso.")
        return
    print("%-20s %-10s %-18s %-10s %s" % ("FECHA", "RESULTADO", "MOTIVO", "SIMILITUD", "MEDICO"))
    for a in accesos:
        similitud = "%.2f" % a["similitud"] if a["similitud"] is not None else "-"
        print("%-20s %-10s %-18s %-10s %s" % (a["fecha"], a["resultado"], a["motivo"], similitud, a["nombre"] or "-"))


def main() -> None:
    parser = argparse.ArgumentParser(description="Administracion del control de acceso")
    sub = parser.add_subparsers(dest="comando", required=True)

    p = sub.add_parser("registrar", help="dar de alta un medico o sumarle rostros")
    p.add_argument("nombre")
    p.add_argument("--fotos", type=int, default=3, help="cuantas fotos de la camara usar (por defecto 3)")
    p.add_argument("--archivo", help="registrar desde una foto guardada en vez de la camara")
    p.set_defaults(func=cmd_registrar)

    p = sub.add_parser("cancelar", help="cancelar un registro con camara en curso")
    p.set_defaults(func=cmd_cancelar)

    p = sub.add_parser("listar", help="ver los medicos registrados")
    p.set_defaults(func=cmd_listar)

    for nombre, funcion, ayuda in (
        ("autorizar", cmd_autorizar, "devolver el permiso de acceso"),
        ("revocar", cmd_revocar, "quitar el permiso de acceso"),
        ("eliminar", cmd_eliminar, "borrar un medico y sus rostros"),
    ):
        p = sub.add_parser(nombre, help=ayuda)
        p.add_argument("id", type=int)
        p.set_defaults(func=funcion)

    p = sub.add_parser("historial", help="ver los ultimos intentos de acceso")
    p.add_argument("-n", "--cantidad", type=int, default=20)
    p.set_defaults(func=cmd_historial)

    database.inicializar()
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
