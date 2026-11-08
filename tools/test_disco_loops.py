#!/usr/bin/env python3
"""Проверяет десять диско-лупов в disco/.

Что именно проверяется:

* формат: WAV, стерео, 16 бит, 22 050 Гц, ровно 30.000 с;
* целое число тактов: длительность соответствует документированному BPM,
  поэтому стык петли попадает на первую долю;
* зацикленность: скачок между последним и первым сэмплом не больше обычного
  шага сигнала в этом же файле (щелчка на стыке нет), и конец файла не тише
  середины (луп не заканчивается тишиной);
* уровень: RMS в разумных пределах, пик далёк от перегрузки, нет постоянной
  составляющей;
* файлы не пустые и различаются по содержимому (десять разных треков, а не
  один файл, скопированный десять раз).
"""

from __future__ import annotations

import array
import math
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DISCO = ROOT / "disco"

# Имя -> (BPM, число тактов).  Ровно 8 * тактов = BPM, значит 30 с = целые такты.
LOOPS = {
    "01_roller_rink_sunset": (120, 15),
    "02_neon_boots": (128, 16),
    "03_mirrorball_dreams": (112, 14),
    "04_saturday_night_bus": (120, 15),
    "05_velvet_elevator": (104, 13),
    "06_feel_the_floor": (128, 16),
    "07_boogie_boulevard": (112, 14),
    "08_disco_nebula": (128, 16),
    "09_strobe_hearts": (120, 15),
    "10_last_dance_last_train": (104, 13),
}


def load(path: Path) -> tuple[array.array, array.array, int]:
    with wave.open(str(path), "rb") as audio:
        assert audio.getnchannels() == 2, f"{path.name}: нужен стерео"
        assert audio.getsampwidth() == 2, f"{path.name}: нужно 16 бит"
        assert audio.getframerate() == 22050, f"{path.name}: нужна частота 22050 Гц"
        frames = audio.getnframes()
        raw = array.array("h", audio.readframes(frames))
    return raw[0::2], raw[1::2], frames


def percentile(values: list[int], share: float) -> int:
    return values[min(len(values) - 1, int(len(values) * share))]


def check_loop(name: str, bpm: int, bars: int) -> None:
    path = DISCO / f"{name}.wav"
    assert path.is_file(), f"Нет диско-лупа: {path.relative_to(ROOT)}"
    left, right, frames = load(path)

    duration = frames / 22050
    assert abs(duration - 30.0) < 1e-9, f"{name}: длительность {duration}, нужно 30 с"
    musical_bars = duration * bpm / 240.0
    assert abs(musical_bars - bars) < 1e-6, (
        f"{name}: при {bpm} BPM в файле {musical_bars:.3f} такта, ожидалось {bars}"
    )

    # Щелчок на стыке: скачок на границе не должен выделяться на фоне сигнала.
    steps = sorted(abs(left[i + 1] - left[i]) for i in range(0, frames - 1, 11))
    seam = abs(left[0] - left[-1])
    limit = percentile(steps, 0.9995)
    # Настоящий луп даёт отношение ~0.1, обрезанный не на такт - больше 0.5.
    assert seam <= limit * 0.35 + 64, (
        f"{name}: разрыв на стыке {seam} при обычном шаге до {limit} - будет щелчок"
    )

    # Конец файла не должен быть тишиной: иначе луп «дышит» на стыке.
    tail = frames // 20
    tail_rms = math.sqrt(sum(value * value for value in left[-tail:]) / tail)
    total_rms = math.sqrt(sum(value * value for value in left) / frames)
    assert tail_rms > total_rms * 0.45, (
        f"{name}: последние 50 мс тихие (RMS {tail_rms:.0f} против {total_rms:.0f})"
    )

    peak = max(max(map(abs, left)), max(map(abs, right)))
    assert peak < 0.97 * 32767, f"{name}: пик {peak} слишком близко к перегрузке"
    level = total_rms / 32767
    assert 0.09 < level < 0.3, f"{name}: RMS {level:.3f} вне рабочего диапазона"

    for channel in (left, right):
        assert abs(sum(channel) / frames) < 200, f"{name}: постоянная составляющая в канале"
    assert left != right, f"{name}: каналы одинаковые, стерео потеряно"


def check_set() -> None:
    files = sorted(DISCO.glob("*.wav"))
    assert len(files) == 10, f"В assets/audio/disco должно быть 10 файлов, а есть {len(files)}"
    signatures = {path.read_bytes()[:4096] for path in files}
    assert len(signatures) == 10, "Некоторые лупы начинаются одинаково - это копии, а не разные треки"


def check_preview() -> None:
    """Превью-картинка набора: десять осциллограмм прямо из WAV-файлов."""
    preview = DISCO / "preview.png"
    if not preview.exists():
        return
    header = preview.read_bytes()[:24]
    assert header[:8] == b"\x89PNG\r\n\x1a\n", "disco/preview.png повреждён"
    width = int.from_bytes(header[16:20], "big")
    height = int.from_bytes(header[20:24], "big")
    assert (width, height) == (684, 526), f"disco/preview.png неверного размера {width}x{height}"


def check_generator() -> None:
    """Генератор должен оставаться без внешних зависимостей и без сэмплов."""
    for name in ("disco_synth.py", "disco_loops.py", "disco_arrange.py",
                 "generate_disco_loops.py"):
        text = (ROOT / "tools" / name).read_text(encoding="utf-8")
        for forbidden in ("import numpy", "import scipy", "import pydub", "subprocess",
                          "urllib", "requests"):
            assert forbidden not in text, f"tools/{name}: сторонние зависимости или сеть ({forbidden})"
    assert not list(DISCO.glob("*.mp3")), "В assets/audio/disco не должно быть mp3"


def main() -> int:
    check_set()
    check_preview()
    check_generator()
    for name, (bpm, bars) in LOOPS.items():
        check_loop(name, bpm, bars)
    total = sum((DISCO / f"{name}.wav").stat().st_size for name in LOOPS) / (1024 * 1024)
    print(f"Диско-лупы: норма, 10 файлов по 30 с, вместе {total:.1f} МиБ")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
