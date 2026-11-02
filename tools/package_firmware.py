"""Create/check MiniPet MPFW packages without third-party Python dependencies."""
from __future__ import annotations

import argparse
import hashlib
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER_BYTES = 128
MAX_IMAGE_BYTES = 6_553_600
TARGETS = ("minipet-s3-n16r8-touch", "minipet-s3-n16r8-button")


def field(value: str) -> bytes:
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,31}", value):
        raise ValueError("Los campos deben tener 1–31 caracteres ASCII: letras, números, punto, guion o guion bajo")
    return value.encode("ascii").ljust(32, b"\0")


def read_field(value: bytes) -> str:
    first = value.find(b"\0")
    if first <= 0 or any(value[first:]):
        raise ValueError("Campo MPFW vacío, sin terminador o con relleno incorrecto")
    result = value[:first].decode("ascii")
    field(result)
    return result


def validate_image(image: bytes, target: str, version: str, limit: int = MAX_IMAGE_BYTES) -> None:
    if not 1024 <= len(image) <= limit:
        raise ValueError("La imagen no cabe en la partición OTA o es demasiado pequeña")
    if not (image[0] == 0xE9 and 1 <= image[1] <= 16 and image[12:14] == b"\x09\x00"
            and image[3] >> 4 == 4 and image[23] == 1):
        raise ValueError("La imagen debe ser ESP32-S3 N16R8 de 16 MB con SHA-256 de imagen")
    checksum = 0xEF
    cursor = 24
    for _ in range(image[1]):
        if cursor + 8 > len(image):
            raise ValueError("Cabecera de segmento ESP incompleta")
        _, length = struct.unpack_from("<II", image, cursor)
        cursor += 8
        if length > len(image) - cursor:
            raise ValueError("Segmento ESP truncado")
        for byte in image[cursor:cursor + length]:
            checksum ^= byte
        cursor += length
    checksum_at = (cursor // 16 + 1) * 16 - 1
    digest_at = checksum_at + 1
    if len(image) != digest_at + 32 or image[checksum_at] != checksum:
        raise ValueError("Longitud o checksum de imagen ESP incorrectos")
    if hashlib.sha256(image[:digest_at]).digest() != image[digest_at:]:
        raise ValueError("El SHA-256 interno de la imagen ESP no coincide")
    for marker in (f"\nMPFW-TARGET:{target}\n", f"\nMPFW-VERSION:{version}\n"):
        if marker.encode("ascii") not in image:
            raise ValueError("La variante o versión declarada no coincide con el firmware compilado")


def package(image: bytes, target: str, version: str, limit: int = MAX_IMAGE_BYTES) -> bytes:
    if target not in TARGETS:
        raise ValueError("Variante de MiniPet desconocida")
    target_field, version_field = field(target), field(version)
    validate_image(image, target, version, limit)
    header = struct.pack("<4sHHI", b"MPFW", 1, HEADER_BYTES, len(image))
    header += target_field + version_field + hashlib.sha256(image).digest() + bytes(20)
    return header + image


def check(raw: bytes, limit: int = MAX_IMAGE_BYTES) -> tuple[str, str]:
    if len(raw) < HEADER_BYTES:
        raise ValueError("Cabecera MPFW incompleta")
    magic, protocol, header_bytes, image_bytes = struct.unpack_from("<4sHHI", raw)
    if magic != b"MPFW" or protocol != 1 or header_bytes != HEADER_BYTES or any(raw[108:128]):
        raise ValueError("Cabecera o campos reservados MPFW incorrectos")
    if image_bytes != len(raw) - HEADER_BYTES:
        raise ValueError("Paquete MPFW truncado o con bytes adicionales")
    target, version = read_field(raw[12:44]), read_field(raw[44:76])
    if target not in TARGETS:
        raise ValueError("Variante de MiniPet desconocida")
    image = raw[HEADER_BYTES:]
    if hashlib.sha256(image).digest() != raw[76:108]:
        raise ValueError("El SHA-256 del firmware no coincide con la cabecera")
    validate_image(image, target, version, limit)
    return target, version


def atomic_write(path: Path, raw: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(raw)
    temporary.replace(path)


def main() -> None:
    parser = argparse.ArgumentParser(description="Crear/verificar paquetes OTA MiniPet para ESP32-S3 N16R8")
    parser.add_argument("--image", type=Path)
    parser.add_argument("--target", choices=TARGETS)
    parser.add_argument("--version", help="Versión compilada; por defecto se lee include/FirmwareVersion.h")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--max-image-bytes", type=int, default=MAX_IMAGE_BYTES)
    parser.add_argument("--check", nargs="+", type=Path, metavar="PAQUETE")
    args = parser.parse_args()
    try:
        if args.max_image_bytes < 1024 or args.max_image_bytes > MAX_IMAGE_BYTES:
            raise ValueError("El límite de imagen debe estar entre 1024 y 6553600 bytes")
        if args.check:
            if args.image or args.output or args.target or args.version:
                parser.error("--check no se combina con las opciones para crear un paquete")
            for path in args.check:
                raw = path.read_bytes()
                target, version = check(raw, args.max_image_bytes)
                sidecar = Path(str(path) + ".sha256")
                if sidecar.exists():
                    expected = f"{hashlib.sha256(raw).hexdigest()}  {path.name}\n"
                    if sidecar.read_text() != expected:
                        raise ValueError(f"El SHA-256 externo no coincide: {sidecar}")
                print(f"Verificado: {path} · {target} · {version} · {len(raw) - HEADER_BYTES} bytes")
        else:
            if not args.image or not args.target or not args.output:
                parser.error("Para crear un paquete se requieren --image, --target y --output")
            version = args.version
            if version is None:
                definition = re.search(r'#define\s+MINIPET_VERSION\s+"([^"]+)"', (ROOT / "include/FirmwareVersion.h").read_text())
                if definition is None:
                    raise ValueError("No se encontró MINIPET_VERSION en include/FirmwareVersion.h")
                version = definition.group(1)
            if any(char in args.output.name for char in "\r\n"):
                raise ValueError("Nombre de salida incorrecto")
            raw = package(args.image.read_bytes(), args.target, version, args.max_image_bytes)
            check(raw, args.max_image_bytes)
            atomic_write(args.output, raw)
            atomic_write(Path(str(args.output) + ".sha256"), f"{hashlib.sha256(raw).hexdigest()}  {args.output.name}\n".encode("ascii"))
            print(f"Creado: {args.output} · {len(raw)} bytes (cabecera incluida)")
    except (OSError, ValueError, UnicodeError, struct.error) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__":
    main()
