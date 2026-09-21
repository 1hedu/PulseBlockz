// The 2A03's voices, generated from the hardware's own definition.
//
//   node scripts/prep-nes.js
//
// The APU's waveforms are exactly specified: an eight-entry duty sequence for the pulses, a
// thirty-two step staircase for the triangle, a fifteen-bit shift register for the noise.
//
// Each voice is one cycle, or for noise one full sequence, written as a looping sample. Pitch
// is playback speed, which is all the chip's timer register does to the same sequence.
const fs = require("fs");
const path = require("path");

const OUT = path.join(__dirname, "sfx");
const RATE = 44100;

// 64 gives each of a pulse's eight duty steps a whole number of samples, so the loop closes on
// a sample boundary. A length that does not divide evenly clicks once per repetition -- at
// these frequencies, a buzz across the whole note.
const CYCLE = 64;
const BASE = RATE / CYCLE;                     // 689.0625 Hz

/** The eight-step duty sequences, straight out of the APU. */
const DUTY = {
  "12.5": [0, 1, 0, 0, 0, 0, 0, 0],
  "25":   [0, 1, 1, 0, 0, 0, 0, 0],
  "50":   [0, 1, 1, 1, 1, 0, 0, 0],
  "75":   [1, 0, 0, 1, 1, 1, 1, 1],
};

function wav(samples, rate) {
  const pcm = Buffer.alloc(samples.length * 2);
  samples.forEach((v, i) => pcm.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(v))), i * 2));
  const head = Buffer.alloc(44);
  head.write("RIFF", 0);
  head.writeUInt32LE(36 + pcm.length, 4);
  head.write("WAVEfmt ", 8);
  head.writeUInt32LE(16, 16);
  head.writeUInt16LE(1, 20);
  head.writeUInt16LE(1, 22);
  head.writeUInt32LE(rate, 24);
  head.writeUInt32LE(rate * 2, 28);
  head.writeUInt16LE(2, 32);
  head.writeUInt16LE(16, 34);
  head.write("data", 36);
  head.writeUInt32LE(pcm.length, 40);
  return Buffer.concat([head, pcm]);
}

fs.mkdirSync(OUT, { recursive: true });
const made = [];

// ---- pulse ------------------------------------------------------------------------------
// 12.5% and 75% are one wave with the phase inverted, so they sound identical. Both exist
// because the track picks between them by register.
for (const [name, seq] of Object.entries(DUTY)) {
  const s = [];
  for (let i = 0; i < CYCLE; i++) s.push(seq[Math.floor(i / (CYCLE / 8))] ? 22000 : -22000);
  const file = `nes-pulse${name.replace(".", "_")}.wav`;
  fs.writeFileSync(path.join(OUT, file), wav(s, RATE));
  made.push([file, `pulse ${name}%`, `${BASE.toFixed(2)} Hz`, s.length]);
}

// ---- triangle ---------------------------------------------------------------------------
// Thirty-two steps up and back down, four bits deep: the quantisation is most of what the
// channel sounds like, so a smooth triangle is wrong.
{
  const steps = [];
  for (let i = 15; i >= 0; i--) steps.push(i);
  for (let i = 0; i <= 15; i++) steps.push(i);
  const s = [];
  for (let i = 0; i < CYCLE; i++) {
    const v = steps[Math.floor(i / (CYCLE / 32)) % 32];
    s.push((v / 15) * 44000 - 22000);
  }
  fs.writeFileSync(path.join(OUT, "nes-triangle.wav"), wav(s, RATE));
  made.push(["nes-triangle.wav", "triangle (32 steps)", `${BASE.toFixed(2)} Hz`, s.length]);
}

// ---- noise ------------------------------------------------------------------------------
// One whole turn of the register: 32,767 steps, one sample each, so the loop is exact. Every
// noise period the music asks for is this sequence clocked faster or slower.
{
  let reg = 1;                                  // the register powers up as 1, never 0
  const s = [];
  for (let i = 0; i < 32767; i++) {
    // Long mode: feedback is bit 0 xor bit 1. Short mode taps bit 6; the track never asks for it.
    const fb = (reg & 1) ^ ((reg >> 1) & 1);
    reg = (reg >> 1) | (fb << 14);
    s.push((reg & 1) ? -20000 : 20000);         // the output is the inverse of bit 0
  }
  fs.writeFileSync(path.join(OUT, "nes-noise.wav"), wav(s, RATE));
  made.push(["nes-noise.wav", "noise (long, one full LFSR turn)", `${RATE} Hz`, s.length]);
}

let total = 0;
for (const [file, what, base, n] of made) {
  const size = fs.statSync(path.join(OUT, file)).size;
  total += size;
  console.log(`${file.padEnd(22)} ${what.padEnd(34)} base ${base.padStart(11)}  ${String(n).padStart(6)} samples  ${(size / 1024).toFixed(1)}KB`);
}
console.log(`\n${made.length} voices, ${(total / 1024).toFixed(1)}KB total -> ${OUT}`);
console.log(`pulse and triangle loop at ${BASE.toFixed(4)} Hz; play at speed = wanted / ${BASE.toFixed(4)}`);
console.log(`noise plays at speed = rate / ${RATE}`);
