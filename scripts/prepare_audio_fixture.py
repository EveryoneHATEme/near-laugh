"""Regenerate P04 assets from original text and deterministic synthetic effects.

Requires eSpeak NG 1.52 and ffmpeg on PATH (or pass --espeak).
No synthesis tools or source voice data are needed to build/run the game.
"""

import argparse
import array
import hashlib
import json
import math
import os
from pathlib import Path
import random
import subprocess
import tempfile
import wave

ROOT = Path(__file__).resolve().parents[1]
RATE = 48000


def save_wav(path, samples):
    pcm = array.array("h", (round(max(-1, min(1, x)) * 32767) for x in samples))
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(pcm.tobytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--espeak", default="espeak-ng")
    args = parser.parse_args()
    executable = str(Path(args.espeak).resolve()) if Path(args.espeak).exists() else args.espeak
    command = [executable]
    if (Path(executable).parent / "espeak-ng-data").is_dir():
        command.append("--path=" + str(Path(executable).parent))
        os.environ["ESPEAK_DATA_PATH"] = str(Path(executable).parent / "espeak-ng-data")
    audio = ROOT / "resources/audio"
    captions = ROOT / "resources/captions"
    audio.mkdir(exist_ok=True)
    captions.mkdir(exist_ok=True)
    rng = random.Random(4001)

    def effect(name, seconds, sample, label, text):
        save_wav(audio / f"{name}.wav", (sample(i / RATE) for i in range(seconds * RATE)))
        (captions / f"{name}.captions").write_text(
            f"0\t{seconds}\t{label}\t{text}\n", encoding="utf-8", newline="\n")

    def radio(t):
        # A short original five-note melody with deliberately quiet static.
        notes = (220, 261.625565, 293.664768, 329.627557, 261.625565)
        note = notes[int(t * 2) % len(notes)]
        envelope = math.sin(math.pi * ((t * 2) % 1)) ** 2
        return 0.11 * envelope * math.sin(2 * math.pi * note * t) + 0.009 * rng.uniform(-1, 1)

    def ring(t):
        beat = t % 3
        envelope = min(1, beat * 100, max(0, (1.5 - beat) * 100)) if beat < 1.5 else 0
        return envelope * 0.15 * (math.sin(2 * math.pi * 440 * t) + math.sin(2 * math.pi * 480 * t))

    def footsteps(t):
        step = t % 0.6
        envelope = math.exp(-step * 35) * min(1, step * 1000)
        return envelope * (0.38 * math.sin(2 * math.pi * 95 * step) + 0.2 * rng.uniform(-1, 1))

    effect("radio", 10, radio, "Радио", "Тихая музыка и помехи.")
    effect("phone-ring", 6, ring, "Телефон", "Звонит телефон.")
    effect("footsteps", 6, footsteps, "За дверью", "Шаги приближаются, затем удаляются.")

    script = {
        "phone-conversation": [
            ("Лена", "Ты скоро вернёшься?", "ru+f3"),
            ("Голос в телефоне", "Я за городом. Сегодня не вернусь. Никому не открывай.", "ru"),
            ("Лена", "Поняла. До завтра.", "ru+f3"),
            ("Телефон", "Разговор окончен.", None),
        ],
        "invitation": [
            ("Голос за дверью", "Это я. Я уже дома. Открой дверь, пойдём на кухню.", "ru"),
        ],
    }
    for name, segments in script.items():
        combined = []
        timed = []
        with tempfile.TemporaryDirectory(prefix="near-laugh-speech-") as temp:
            temp = Path(temp)
            for label, text, voice in segments:
                if voice:
                    (temp / "line.txt").write_text(text, encoding="utf-8")
                    subprocess.run(command + ["-v", voice, "-s", "145", "-a", "125", "-b", "1",
                                    "-f", str(temp / "line.txt"), "-w", str(temp / "raw.wav")], check=True)
                    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(temp / "raw.wav"),
                                    "-ar", "48000", "-ac", "1", "-c:a", "pcm_s16le", str(temp / "line.wav")], check=True)
                    with wave.open(str(temp / "line.wav"), "rb") as wav:
                        data = array.array("h", wav.readframes(wav.getnframes()))
                    samples = [s / 32768 for s in data]
                else:
                    # Hang-up click followed by silence, represented by an essential sound caption.
                    samples = [0.18 * math.exp(-i / 160) * rng.uniform(-1, 1) for i in range(2000)]
                start = len(combined) / RATE
                combined.extend([0.0] * 9600)
                combined.extend(samples)
                combined.extend([0.0] * max(24000, int(1.5 * RATE) - len(samples)))
                end = math.floor(len(combined) / RATE * 1_000_000) / 1_000_000
                timed.append(f"{start:.6f}\t{end:.6f}\t{label}\t{text}\n")
        save_wav(audio / f"{name}.wav", combined)
        (captions / f"{name}.captions").write_text("".join(timed), encoding="utf-8", newline="\n")

    manifest = {
        "generator": "scripts/prepare_audio_fixture.py",
        "speech_tool": subprocess.check_output(command + ["--version"], text=True).split("Data at:")[0].strip(),
        "effect_seed": 4001,
        "sample_rate": RATE,
        "profile": "RIFF WAVE signed PCM16 mono",
        "files": {},
    }
    for path in sorted([*audio.glob("*.wav"), *captions.glob("*.captions")]):
        record = {"sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        if path.suffix == ".wav":
            with wave.open(str(path), "rb") as wav:
                assert wav.getnchannels() == 1 and wav.getsampwidth() == 2 and wav.getframerate() == RATE
                record["duration_seconds"] = wav.getnframes() / RATE
                assert 0 < record["duration_seconds"] <= 120
        manifest["files"][str(path.relative_to(ROOT)).replace("\\", "/")] = record
    (audio / "fixture.sources.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                                               encoding="utf-8", newline="\n")
    print(json.dumps(manifest, ensure_ascii=True, indent=2))


if __name__ == "__main__":
    main()
