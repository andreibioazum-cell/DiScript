#!/usr/bin/env python3
"""Convert the verified Kevin MacLeod recording to the game's PCM WAV format.

This is a format conversion of a real published recording, not music synthesis.
The source MP3 and its CC BY 4.0 notice are kept in the repository. FFmpeg is
needed only when rebuilding the WAV; normal game builds use the bundled WAV.
"""
from __future__ import annotations

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import wave

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "third_party/music/Disco Medusae - Kevin MacLeod.mp3"
OUTPUT = ROOT / "assets/audio/disco_medusae_kevin_macleod.wav"
SOURCE_SHA256 = "0957c02613a361cd89b3f95ab8d0e708ded2849cc28787202e7a46931ff4bace"
RATE = 22050


def verify_source() -> None:
    digest = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    if digest != SOURCE_SHA256:
        raise ValueError(f"Source recording checksum mismatch: {digest}")


def ffmpeg_executable() -> str:
    found = os.environ.get("FFMPEG") or shutil.which("ffmpeg")
    if found:
        return found
    try:
        import imageio_ffmpeg  # optional build-time dependency
    except ImportError as error:
        raise RuntimeError("Install ffmpeg or `pip install imageio-ffmpeg==0.6.0`") from error
    return imageio_ffmpeg.get_ffmpeg_exe()


def convert() -> Path:
    verify_source()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="cb4-disco-") as directory:
        temporary = Path(directory) / "disco.wav"
        subprocess.run([
            ffmpeg_executable(), "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
            "-i", str(SOURCE), "-map", "0:a:0", "-vn", "-map_metadata", "-1",
            "-ac", "2", "-ar", str(RATE), "-c:a", "pcm_s16le", str(temporary),
        ], check=True)
        with wave.open(str(temporary), "rb") as audio:
            if (audio.getnchannels(), audio.getsampwidth(), audio.getframerate()) != (2, 2, RATE):
                raise ValueError("Unexpected PCM output format")
            duration = audio.getnframes() / audio.getframerate()
            if abs(duration - 221.283265) > 0.01:
                raise ValueError(f"Recording was truncated: {duration:.6f} s")
        os.replace(temporary, OUTPUT)
    print(f"{OUTPUT.relative_to(ROOT)}: {duration:.6f} s, stereo PCM16, {RATE} Hz")
    print(f"SHA-256: {hashlib.sha256(OUTPUT.read_bytes()).hexdigest()}")
    return OUTPUT


if __name__ == "__main__":
    convert()
