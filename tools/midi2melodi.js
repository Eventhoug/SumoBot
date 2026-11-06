// MIDI -> Arduino-melodi
// Laver ét spor fra en .mid-fil om til include/sang.h, som src/sang_afspiller.cpp spiller.
//
// Brug:
//   node tools/midi2melodi.js sang.mid          -> viser sporene i filen
//   node tools/midi2melodi.js sang.mid 3        -> laver spor 3 om til include/sang.h
//   node tools/midi2melodi.js sang.mid 3 12     -> samme, men 12 halvtoner (1 oktav) op
//   node tools/midi2melodi.js sang.mid 2+5      -> spor 2 og 5 lagt sammen (højeste tone vinder)
//   node tools/midi2melodi.js sang.mid auto     -> alle spor undtagen trommer lagt sammen til én stemme
//
// Toner bindes sammen (legato): et hul under 200 ms fyldes ud af den forrige tone,
// så melodien ikke lyder hakket. Slå fra ved at skrive "hakket" til sidst.
//
// Uden transponer-tal flyttes melodien automatisk hele oktaver, så den ligger
// omkring 900 Hz, hvor en buzzer lyder klarest.
//
// En buzzer kan kun spille én tone ad gangen. Spiller sporet flere toner samtidig,
// tages den højeste (som regel melodien).

const fs = require('fs');
const path = require('path');

// Her læses argumenterne. "hakket" slår legato fra; tal (fx 12 eller -12) er transponering
const args = process.argv.slice(2);
const erHakket = (a) => /^(hakket|legato=0)$/i.test(a);
const hakket = args.some(erHakket);
const [filnavn, sporArg, transArg] = args.filter(a => !erHakket(a));
if (transArg !== undefined && !/^[+-]?\d+$/.test(transArg)) {
  console.log('Fejl: "' + transArg + '" er ikke et tal. Transponering skal være fx 12 eller -12.');
  process.exit(1);
}
if (sporArg !== undefined && sporArg !== 'auto' && !/^\d+(\+\d+)*$/.test(sporArg)) {
  console.log('Fejl: "' + sporArg + '" er ikke et spor. Skriv fx 3, 2+5 eller auto.');
  process.exit(1);
}
if (!filnavn) {
  console.log('Brug: node tools/midi2melodi.js <fil.mid> [spor] [transponer]');
  process.exit(1);
}

const data = fs.readFileSync(filnavn);
let pos = 0;
const u8 = () => data[pos++];
const u16 = () => { const v = data.readUInt16BE(pos); pos += 2; return v; };
const u32 = () => { const v = data.readUInt32BE(pos); pos += 4; return v; };
const vlq = () => { let v = 0, b; do { b = u8(); v = (v << 7) | (b & 0x7f); } while (b & 0x80); return v; };
const tekst = (n) => { const s = data.toString('latin1', pos, pos + n); pos += n; return s; };

if (tekst(4) !== 'MThd') throw new Error('Ikke en MIDI-fil');
const hLen = u32();
u16(); // format
const antalSpor = u16();
const division = u16();
pos = 8 + hLen;
if (division & 0x8000) throw new Error('SMPTE-tid understøttes ikke');

const tempoer = []; // { tick, usPrKvart }
const spor = [];

for (let s = 0; s < antalSpor && pos < data.length; s++) {
  while (tekst(4) !== 'MTrk') { pos += u32(); } // spring ukendte chunks over
  const len = u32();
  const slut = pos + len;
  let tick = 0, status = 0, navn = '', kanaler = new Set();
  const aabne = new Map(); // pitch -> [starttick...]
  const noder = [];

  while (pos < slut) {
    tick += vlq();
    let b = data[pos];
    if (b & 0x80) { status = b; pos++; }
    if (status === 0xff) {
      const type = u8(); const n = vlq();
      if (type === 0x03) navn = data.toString('latin1', pos, pos + n).trim();
      if (type === 0x51) tempoer.push({ tick, us: data.readUIntBE(pos, 3) });
      pos += n;
    } else if (status === 0xf0 || status === 0xf7) {
      pos += vlq();
    } else {
      const type = status & 0xf0, kanal = status & 0x0f;
      const d1 = u8();
      const d2 = (type === 0xc0 || type === 0xd0) ? 0 : u8();
      if (type === 0x90 && d2 > 0) {
        kanaler.add(kanal);
        if (!aabne.has(d1)) aabne.set(d1, []);
        aabne.get(d1).push(tick);
      } else if (type === 0x80 || (type === 0x90 && d2 === 0)) {
        const liste = aabne.get(d1);
        if (liste && liste.length) noder.push({ start: liste.shift(), slut: tick, pitch: d1 });
      }
    }
  }
  pos = slut;
  spor.push({ navn, noder, trommer: kanaler.has(9) });
}

// Tid: ticks -> millisekunder med tempo-ændringer
tempoer.sort((a, b) => a.tick - b.tick);
if (!tempoer.length || tempoer[0].tick > 0) tempoer.unshift({ tick: 0, us: 500000 });
function tilMs(t) {
  let ms = 0;
  for (let i = 0; i < tempoer.length; i++) {
    const fra = tempoer[i].tick;
    const til = i + 1 < tempoer.length ? Math.min(tempoer[i + 1].tick, t) : t;
    if (til <= fra) break;
    ms += ((til - fra) * tempoer[i].us) / division / 1000;
  }
  return ms;
}

if (sporArg === undefined) {
  console.log(`Spor i ${path.basename(filnavn)}:`);
  spor.forEach((s, i) => {
    if (!s.noder.length) return;
    const lav = Math.min(...s.noder.map(n => n.pitch));
    const hoej = Math.max(...s.noder.map(n => n.pitch));
    console.log(`  ${String(i).padStart(2)}: ${(s.navn || '(uden navn)').padEnd(22)} ${String(s.noder.length).padStart(5)} noder` +
      `  tone ${lav}-${hoej}${s.trommer ? '  (trommer - kan ikke bruges)' : ''}`);
  });
  console.log('\nVælg melodisporet: node tools/midi2melodi.js <fil.mid> <spor>');
  process.exit(0);
}

const valgteNoder = [];
const sporNumre = sporArg === 'auto'
  ? spor.map((sp, i) => i).filter(i => spor[i].noder.length && !spor[i].trommer)
  : String(sporArg).split('+');
if (sporArg === 'auto') console.log('Bruger spor: ' + sporNumre.join(', '));
for (const nr of sporNumre) {
  const sp = spor[Number(nr)];
  if (!sp || !sp.noder.length) throw new Error('Spor ' + nr + ' findes ikke eller har ingen noder');
  if (sp.trommer) console.log('Advarsel: spor ' + nr + ' er trommer og lyder ikke som toner');
  valgteNoder.push(...sp.noder);
}
const valgt = { noder: valgteNoder };

// Her findes transponeringen: enten den man skriver, eller automatisk hele oktaver mod ca. 900 Hz (MIDI 81)
let trans;
if (transArg !== undefined) {
  trans = Number(transArg);
} else {
  const p = valgteNoder.map(n => n.pitch).sort((a, b) => a - b);
  const median = p[Math.floor(p.length / 2)];
  trans = 12 * Math.round((81 - median) / 12);
  console.log('Automatisk transponering: ' + (trans >= 0 ? '+' : '') + trans + ' halvtoner');
}

// Gør sporet enstemmigt: ved samme starttid tages den højeste tone
const starter = new Map();
for (const n of valgt.noder) {
  const g = starter.get(n.start);
  if (!g || n.pitch > g.pitch) starter.set(n.start, n);
}
const raekke = [...starter.values()].sort((a, b) => a.start - b.start);

const ud = []; // [frekvens, ms]
let tid = tilMs(raekke[0].start);
for (let i = 0; i < raekke.length; i++) {
  const n = raekke[i];
  const start = tilMs(n.start);
  const naesteStart = i + 1 < raekke.length ? tilMs(raekke[i + 1].start) : Infinity;
  let slut = Math.min(tilMs(n.slut), naesteStart);
  // Her bindes tonen til den næste, hvis hullet er kort (legato), så melodien ikke bliver opdelt
  if (!hakket && naesteStart !== Infinity && naesteStart - slut < 200) slut = naesteStart;
  if (start - tid >= 1) ud.push([0, Math.round(start - tid)]);
  const hz = Math.round(440 * Math.pow(2, (n.pitch + trans - 69) / 12));
  ud.push([Math.max(31, hz), Math.max(1, Math.round(slut - start))]); // tone() kan ikke under 31 Hz
  tid = slut;
}

// Her ryddes op: meget korte noder/pauser (under 25 ms) lyder som knas, så de lægges til forrige node
for (let i = 1; i < ud.length; i++) {
  if (ud[i][1] < 25) { ud[i - 1][1] += ud[i][1]; ud.splice(i, 1); i--; }
}
// Ens toner lige efter hinanden med pause imellem beholdes; to pauser i træk lægges sammen
for (let i = 1; i < ud.length; i++) {
  if (ud[i][0] === 0 && ud[i - 1][0] === 0) { ud[i - 1][1] += ud[i][1]; ud.splice(i, 1); i--; }
}

// Her tjekkes at alle tal er gyldige, før filen skrives, så sang.h aldrig bliver ødelagt
if (ud.some(([f, m]) => !Number.isFinite(f) || !Number.isFinite(m) || m > 65535)) {
  console.log('Fejl: der kom ugyldige tal ud. include/sang.h er ikke ændret.');
  process.exit(1);
}
const linjer = [];
for (let i = 0; i < ud.length; i += 8) {
  linjer.push('  ' + ud.slice(i, i + 8).map(([f, m]) => `${f}, ${m}`).join(',  ') + ',');
}
const header =
`// Lavet automatisk af tools/midi2melodi.js fra ${path.basename(filnavn)} (spor ${sporArg})
// To tal pr. node: frekvens i Hz (0 = pause), længde i ms. Ligger i flash (PROGMEM), ikke i RAM.
#pragma once
#include <Arduino.h>

const uint16_t SANG[] PROGMEM = {
${linjer.join('\n')}
};
const uint16_t SANG_ANTAL = ${ud.length};
`;
const udfil = path.join(__dirname, '..', 'include', 'sang.h');
fs.writeFileSync(udfil, header);
const sek = (ud.reduce((s, [, m]) => s + m, 0) / 1000).toFixed(1);
console.log(`Skrev ${ud.length} noder (${sek} s, ${ud.length * 4} bytes flash) til include/sang.h`);
