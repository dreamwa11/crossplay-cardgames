// Plays the same seeded quarters as trace.cpp with the phone version's engine.
const fs = require('fs');
const vm = require('vm');
// The PaperMono has no colour, so its words for the two number colours are
// round and square; the phone engine is reworded the same way before it runs.
function blackAndWhite(src) {
  const out = src.replace("(c.red?'green':'blue')", "(c.red?'round':'square')").replace('counts as green or blue', 'counts as round or square');
  if (out.indexOf("'round':'square'") < 0 || out.indexOf('round or square') < 0) throw new Error('phone engine wording moved');
  return out;
}
const ctx = {console};
vm.createContext(ctx);
vm.runInContext(blackAndWhite(fs.readFileSync(__dirname + '/phone_engine.js', 'utf8')), ctx);
// The port's xorshift in place of Math.random, consumed the same way.
vm.runInContext(`
var RS = 0;
function seedRng(v){ RS = (v >>> 0) || 0x2545F491; }
function nextRng(){ RS ^= RS << 13; RS >>>= 0; RS ^= RS >>> 17; RS ^= RS << 5; RS >>>= 0; return RS; }
rnd = function(n){ var v = nextRng(); return n <= 1 ? 0 : v % n; };
`, ctx);
const MAP = {KD:62,KS:63,QD:64,QS:65,JD:66,JS:67,AD:68,AS:69,GT:70,GM:71,KC:72,JC:73,BJ:74,RJ:75,RG:76,JE:77,JH:78,QH:79,KH:80};
const ix = id => id[0] === 'n' && /^n\d+$/.test(id) ? +id.slice(1) : MAP[id];
const CH = {K:0,Q:1,J:2,A:3};
const PT = {kier:1, casey:0, reghabi:4};
function state(logSeen) {
  const G = ctx.G;
  let s = '';
  for (let i = 0; i < 16; i++) { s += '|'; G.stacks[i].forEach(c => s += ix(c.id) + (c.faceUp ? 'u' : '') + ','); }
  const list = (n, l) => { s += n; l.forEach(c => s += ix(c.id) + ','); };
  list(' D:', G.draw); list(' H:', G.hand); list(' W:', G.waiting); list(' G:', G.gone);
  s += ' P:'; G.paired.forEach(p => { s += CH[p.ch] + '='; p.cards.forEach(c => s += ix(c.id) + ','); s += ';'; });
  const pt = G.pending ? (G.pending.type === 'casey' ? (G.pending.mode === 'main' ? 2 : 3) : PT[G.pending.type]) : 0;
  const goat = [].concat(...G.stacks, G.draw, G.hand, G.waiting, G.gone, ...G.paired.map(p => p.cards)).find(c => c.id === 'GT');
  const u = G.unlocked.map(x => x ? 1 : 0).join('');
  const blocked = G.blocked ? ix(G.blocked) : -1, lock = G.lockZone === null ? -1 : G.lockZone;
  const f = [G.hellyPause, G.hallPending, G.hallPass, G.defianceUsed, G.reshuffled].map(x => x ? 1 : 0).join('');
  const over = G.over === 'win' ? 1 : G.over === 'lose' ? 2 : 0;
  s += ` U:${u} B:${blocked} L:${lock} F:${f} K:${pt},${G.pending ? G.pending.max : 0} O:${over} V:${goat.val}${goat.red ? 1 : 0}\n`;
  G.log.forEach(l => { if (l.t >= logSeen.v) s += '  ' + l.x + '\n'; });
  logSeen.v = G.turn + 1;
  return s;
}
const seeds = +(process.argv[2] || 50), lenient = process.argv[3] === '1';
let out = '';
for (let seed = 1; seed <= seeds; seed++) {
  vm.runInContext(`seedRng(${(seed * 2654435761) >>> 0})`, ctx);
  vm.runInContext(`newGame({lenient:${lenient}})`, ctx);
  const logSeen = {v: 0};
  out += `SEED ${seed}\nstart` + state(logSeen);
  for (let step = 0; step < 600 && !ctx.G.over; step++) {
    const G = ctx.G;
    let act;
    if (G.pending) {
      const cand = vm.runInContext('pendingCandidates()', ctx);
      const take = G.turn % 4 === 3 ? 0 : Math.min(cand.length, G.pending.max);
      act = 'resolve ' + take;
      ctx.__ids = cand.slice(0, take);
      vm.runInContext('actResolve(__ids)', ctx);
    } else {
      const pc = vm.runInContext('pairable()', ctx);
      if (pc.length) { act = 'pair ' + CH[pc[0]]; ctx.__ch = pc[0]; vm.runInContext('actPair(__ch)', ctx); }
      else {
        const u = vm.runInContext('usableNums()', ctx), nu = u.length;
        let done = false;
        for (let size = 2; size <= 4 && !done; size++) {
          const ixs = []; for (let k = 0; k < size; k++) ixs.push(k);
          while (!done) {
            if (ixs[size - 1] >= nu) break;
            const cards = ixs.map(k => u[k]), wild = cards.find(c => c.wild);
            for (let t = 0; t < (wild ? 2 : 1) && !done; t++) {
              if (wild) wild.red = t === 0;
              ctx.__ids = cards.map(c => c.id);
              if (vm.runInContext('selectionTotals(__ids).ok', ctx)) {
                const fixed = vm.runInContext('priorityFix(__ids).ids', ctx);
                ctx.__ids = fixed; act = 'refine';
                vm.runInContext('actRefine(__ids)', ctx); done = true;
              }
            }
            if (!done && wild) wild.red = true;
            let k = size - 1;
            while (k >= 0 && ixs[k] === nu - size + k) k--;
            if (k < 0) break;
            ixs[k]++;
            for (let q = k + 1; q < size; q++) ixs[q] = ixs[q - 1] + 1;
          }
        }
        if (!done) {
          if (!vm.runInContext('canDraw()', ctx)) break;
          act = 'draw'; vm.runInContext('actDraw()', ctx);
        }
      }
    }
    out += act + state(logSeen);
  }
  out += 'END ' + (ctx.G.over === 'win' ? 1 : ctx.G.over === 'lose' ? 2 : 0) + '\n';
}
process.stdout.write(out);
