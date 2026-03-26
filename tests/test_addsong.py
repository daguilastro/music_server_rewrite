#!/usr/bin/env python3
import socket
import struct


SERVER_PORT = 8080
SONG_URL = "https://music.youtube.com/watch?v=846cjX0ZTrk&si=cQEyeELjFMHTnrLE"


def get_local_ip() -> str:
    """Obtiene la IP local principal de la máquina."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # No necesita que 8.8.8.8 sea alcanzable; solo fuerza selección de interfaz local.
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


def encode_tlv_command(cmd_type: int, args: list[bytes]) -> bytes:
    """
    Protocolo esperado por el servidor:
    - Header: [1 byte tipo][4 bytes big-endian payload_len]
    - Payload: repetición de [2 bytes big-endian arg_len][arg_bytes]
    """
    payload = bytearray()

    for arg in args:
        if len(arg) > 0xFFFF:
            raise ValueError("Un argumento excede 65535 bytes")
        payload += struct.pack(">H", len(arg))
        payload += arg

    header = struct.pack(">BI", cmd_type, len(payload))
    return header + payload


def main() -> None:
    local_ip = get_local_ip()

    # CMD_ADDSONG = 1
    cmd_type = 1

    # args esperados por comando 1:
    # <url> <title> <location> <channel_name>
    args = [
        SONG_URL.encode("utf-8"),
        "Test - 846cjX0ZTrk".encode("utf-8"),
        "/tmp/test_846cjX0ZTrk.mp3".encode("utf-8"),
        "TestChannel".encode("utf-8"),
    ]

    packet = encode_tlv_command(cmd_type, args)

    print(f"Conectando a {local_ip}:{SERVER_PORT}...")
    with socket.create_connection((local_ip, SERVER_PORT), timeout=5) as sock:
        sock.sendall(packet)
        print("Comando ADDSONG enviado.")

    print("Listo. Revisa logs del servidor para confirmar inserción en DB.")


if __name__ == "__main__":
    main()
