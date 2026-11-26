// The phone version's rules engine (Severance: The Card Game, Version 2), copied from the
// web build so the port can be checked against it; CrossPlay keeps unused draws in a pile.

/* ===================== ENGINE (no DOM) — Version 2 rules ===================== */
var CHARS = {K:'Mark S.',Q:'Helly R.',J:'Dylan G.',A:'Irving B.'};
var CHAR_ORDER = ['K','Q','J','A'];
var LAYERS = [1,3,4,4];
var ADMIN = {1:2,2:2,3:2,4:4,5:4,6:4,7:2,8:2,9:2,10:2};
var SPECIALS = {kier:'Kier Eagan',dieter:'Dieter Eagan',casey:'Ms. Casey',drummond:'Mr. Drummond',graner:'Doug Graner',
  milchick:'Seth Milchick',cobel:'Harmony Cobel',reghabi:'Reghabi',jame:'Jame Eagan',gemma:'Gemma'};
var G = null;
var onSnap = null;
function snap(kind,spot,extra){if(onSnap)onSnap(kind,spot||null,extra||null);}

function rnd(n){return Math.floor(Math.random()*n);}
function pick(a){return a[rnd(a.length)];}
function shuffle(a){for(var i=a.length-1;i>0;i--){var j=rnd(i+1),t=a[i];a[i]=a[j];a[j]=t;}return a;}
function zoneOf(i){return Math.floor(i/4);}

function mkSpecial(fx,id,adm){return {id:id,type:'special',fx:fx,name:SPECIALS[fx],faceUp:false,adm:!!adm};}
function lab(c){
  if(c.wild)return 'Emile the Goat'+(c.faceUp&&c.val?' (worth '+c.val+')':'');
  if(c.type==='num')return c.val+' '+(c.red?'green':'blue');
  if(c.type==='main')return c.name+' ('+(c.red?'innie':'outie')+')';
  return c.name;
}
function log(text,kind){G.log.push({t:G.turn,x:text,k:kind||''});if(G.log.length>80)G.log.shift();}

function newGame(opts){
  var n=0,gridNumsAll=[],admin=[],v,k,mains=[];
  for(v=2;v<=10;v++)for(k=0;k<4;k++)gridNumsAll.push({id:'n'+(n++),type:'num',val:v,red:k<2,faceUp:false});
  for(v=1;v<=10;v++)for(k=0;k<ADMIN[v];k++)admin.push({id:'n'+(n++),type:'num',val:v,red:k%2===0,faceUp:false,adm:true});
  CHAR_ORDER.forEach(function(ch){
    mains.push({id:ch+'D',type:'main',ch:ch,red:true,name:CHARS[ch],faceUp:false});
    mains.push({id:ch+'S',type:'main',ch:ch,red:false,name:ch==='Q'?'Helena':CHARS[ch],faceUp:false});
  });
  var goat={id:'GT',type:'num',wild:true,val:0,red:true,name:'Emile',faceUp:false};
  var gemma=mkSpecial('casey','GM',1);gemma.name='Gemma';              // same ability as Ms. Casey, but lives in the pile
  var draw=shuffle(admin.concat([mkSpecial('dieter','KC',1),gemma,mkSpecial('graner','JC',1),mkSpecial('milchick','BJ',1),
    mkSpecial('cobel','RJ',1),mkSpecial('reghabi','RG',1),mkSpecial('jame','JE',1)]));
  var rest=shuffle(gridNumsAll.concat(mains,[goat,mkSpecial('drummond','JH'),mkSpecial('casey','QH')]));
  var stacks=[];for(var i=0;i<16;i++)stacks.push([]);
  stacks[9].push(mkSpecial('kier','KH'));                      // Kier placed first, position 10
  for(i=0;i<16;i++){while(stacks[i].length<LAYERS[zoneOf(i)])stacks[i].push(rest.pop());}
  G={v:2,stacks:stacks,unlocked:[true,false,false,false],draw:draw,hand:[],waiting:[],paired:[],gone:[],total:81,
     lastPair:null,blocked:null,lockZone:null,hellyPause:false,hallPending:false,hallPass:false,defianceUsed:false,
     reshuffled:false,seen:{},pending:null,queue:[],over:null,turn:0,log:[],ann:null,dealing:true,opts:opts||{lenient:false}};
  log('A new quarter begins. Zone 1 is open; Zones 2, 3 and 4 are locked.','');
  snap('deal');
  settle();
  G.dealing=false;
  return G;
}

/* ---- queries ---- */
function topOf(i){var s=G.stacks[i];return s.length?s[s.length-1]:null;}
function rawTops(){var out=[];for(var i=0;i<16;i++){var c=topOf(i);if(c&&c.faceUp)out.push({c:c,i:i});}return out;}
function gridTops(){return rawTops().filter(function(o){return zoneOf(o.i)!==G.lockZone;});}   // Drummond's locked zone is out of use
function gridNums(){return gridTops().filter(function(o){return o.c.type==='num'&&o.c.id!==G.blocked;});}
function gridMains(){return gridTops().filter(function(o){return o.c.type==='main';});}
// Drawn number cards stay in hand: Dieter's are all playable, the rest form a pile whose top is playable.
function playableHand(){var top=null;G.hand.forEach(function(c){if(!c.keep)top=c;});return G.hand.filter(function(c){return c.keep||c===top;});}
function wasteDepth(){return G.hand.filter(function(c){return !c.keep;}).length;}
function usableNums(){return gridNums().map(function(o){return o.c;}).concat(playableHand());}
function sums(cards){var s={0:1};cards.forEach(function(c){Object.keys(s).forEach(function(k){s[+k+c.val]=1;});});delete s[0];return s;}
function hasValidPair(){
  var u=usableNums(),w=u.filter(function(c){return c.wild;})[0],opts=w?[true,false]:[null];
  for(var n=0;n<opts.length;n++){
    var side=function(c){return c.wild?opts[n]:c.red;};
    var r=sums(u.filter(function(c){return side(c);})),b=sums(u.filter(function(c){return !side(c);}));
    for(var k in r)if(b[k])return true;
  }
  return false;
}
function canReshuffle(){return !G.reshuffled&&G.stacks.slice(8,12).some(function(s){return s.some(function(c){return !c.faceUp;});});}
function canDraw(){return !G.over&&!G.pending&&G.draw.length>0;}   // drawing is allowed even if a pair was missed
function isPaired(ch){return G.paired.some(function(p){return p.ch===ch;});}
function selectionTotals(ids){
  var r=0,b=0,nr=0,nb=0;usableNums().forEach(function(c){if(ids.indexOf(c.id)>=0){if(c.red){r+=c.val;nr++;}else{b+=c.val;nb++;}}});
  return {r:r,b:b,ok:nr>0&&nb>0&&r===b};
}
// Priority rule: among identical usable grid cards, the one in the earliest position must be used first.
function priorityFix(ids){
  var gn=gridNums(),out=ids.slice(),changed=false;
  gn.forEach(function(o){
    if(o.c.wild||out.indexOf(o.c.id)<0)return;
    var first=gn.filter(function(q){return !q.c.wild&&q.c.val===o.c.val&&q.c.red===o.c.red&&out.indexOf(q.c.id)<0&&q.i<o.i;})[0];
    if(first){out[out.indexOf(o.c.id)]=first.c.id;changed=true;}
  });
  return {ids:out,changed:changed};
}
function findCard(id){
  for(var i=0;i<16;i++)for(var j=0;j<G.stacks[i].length;j++)if(G.stacks[i][j].id===id)return {c:G.stacks[i][j],i:i,j:j};
  return null;
}

/* ---- mutations ---- */
function takeFromGrid(id){var f=findCard(id);if(f)G.stacks[f.i].splice(f.j,1);return f;}
function pickStack(){
  var idx=[];for(var i=0;i<16;i++)if(G.stacks[i].length)idx.push(i);
  if(!idx.length)for(i=0;i<16;i++)idx.push(i);
  return pick(idx);
}
function insertHidden(card,k){
  card.faceUp=false;var st=G.stacks[k];
  if(G.unlocked[zoneOf(k)]&&st.length&&st[st.length-1].faceUp)st.splice(st.length-1,0,card);else st.push(card);
}
function stackList(ks){
  if(ks.length>1&&ks[0]===ks[1])return 'the same stack in Zone '+(zoneOf(ks[0])+1);
  var z=[];ks.forEach(function(k){if(z.indexOf(zoneOf(k)+1)<0)z.push(zoneOf(k)+1);});z.sort();
  return (ks.length>1?'stacks':'a stack')+' in Zone'+(z.length>1?'s ':' ')+z.join(' and ');
}
// Log an ability and hold it on screen (with the cards and stacks it touches) before it is carried out.
function say(c,title,text,kind,o){
  o=o||{};
  log(title+(/[!?]$/.test(title)?' ':': ')+text,kind);
  G.ann={card:c,title:title,text:text,spots:o.spots||[],stacks:o.stacks||[],limbo:o.limbo||[],draw:!!o.draw,choice:!!o.choice};
  snap('announce');var a=G.ann;G.ann=null;return a;
}
function toWaiting(card,why){
  takeFromGrid(card.id);card.faceUp=true;G.waiting.push(card);
  log(lab(card)+' '+why+' and waits aside for '+(card.red?'the outie':'the innie')+'.','good');
}
function mainCards(ch){
  var found=[];
  gridMains().forEach(function(o){if(o.c.ch===ch)found.push(o.c);});
  G.waiting.forEach(function(c){if(c.ch===ch)found.push(c);});
  return found;
}
function mateWaiting(ch){return G.waiting.some(function(w){return w.ch===ch;});}
function caseyMains(){return gridMains().filter(function(o){return !mateWaiting(o.c.ch);});}
function pairable(){return CHAR_ORDER.filter(function(ch){return !isPaired(ch)&&!(ch==='Q'&&G.hellyPause)&&mainCards(ch).length===2;});}
function actPair(ch){
  if(G.over||G.pending||pairable().indexOf(ch)<0)return false;
  beginTurn();
  var found=mainCards(ch);
  found.forEach(function(c){takeFromGrid(c.id);G.waiting=G.waiting.filter(function(w){return w.id!==c.id;});c.faceUp=true;});
  G.paired.push({ch:ch,cards:found});
  log(CHARS[ch]+' is paired: innie and outie are reunited ('+G.paired.length+' of 4).','good');snap('pair');
  if(G.paired.length===4){G.over='win';G.queue=[];log('You hit quota! All main characters are paired.','good');snap('end');return true;}
  settle();return true;
}
function updateUnlocks(){
  var ch=false,z1=0,i,z;
  for(i=0;i<4;i++)z1+=G.stacks[i].filter(function(c){return c.type==='num';}).length;
  if(!G.unlocked[1]&&z1<=2){G.unlocked[1]=true;ch=true;log('Zone 2 unlocked: two number cards or fewer remain in Zone 1.','good');snap('unlock');}
  for(z=2;z<4;z++){
    if(G.unlocked[z]||!G.unlocked[z-1])continue;
    var n=0;for(i=(z-1)*4;i<z*4;i++)if(G.stacks[i].some(function(c){return !c.faceUp;}))n++;
    if(n<=1){G.unlocked[z]=true;ch=true;log('Zone '+(z+1)+' unlocked: at most one stack in Zone '+z+' still hides a card.','good');snap('unlock');}
  }
  // No number card is face up anywhere on the grid: the next locked zone opens so play can go on.
  if(!ch&&!G.dealing){
    var pendingFlip=false;
    for(i=0;i<16;i++){var c=topOf(i);if(c&&!c.faceUp&&G.unlocked[zoneOf(i)])pendingFlip=true;}
    z=G.unlocked.indexOf(false);
    if(!pendingFlip&&z>0&&!rawTops().some(function(o){return o.c.type==='num';})){
      G.unlocked[z]=true;ch=true;log('Zone '+(z+1)+' unlocked: no number card is left face up on the grid.','good');snap('unlock');
    }
  }
  return ch;
}
function revealOne(){
  for(var i=0;i<16;i++){
    var c=topOf(i);
    if(c&&!c.faceUp&&G.unlocked[zoneOf(i)]){c.faceUp=true;onReveal(c,i);return true;}
  }
  return updateUnlocks();
}
function rollGoat(c){
  var d=G.gone.filter(function(x){return x.type==='num'&&!x.wild;});
  if(d.length){var s=pick(d);c.val=s.val;return 'a random card from the discard pile ('+lab(s)+')';}
  c.val=1+rnd(6);return 'a die roll, as the discard pile is empty';
}
function onReveal(c,i){
  var pos='Zone '+(zoneOf(i)+1);
  if(c.wild){var how=rollGoat(c);log('Emile the Goat is revealed in '+pos+'. He is worth '+c.val+', set by '+how+', and counts as green or blue, whichever you choose.','good');snap('reveal',c.id);return;}
  if(c.type==='num'){if(!G.dealing)log('Revealed '+lab(c)+' in '+pos+'.');snap('reveal');return;}
  if(c.type==='main'){
    log(lab(c)+' is revealed in '+pos+'.');snap('reveal',c.id);
    if(mateWaiting(c.ch))return;            // stays on the grid; the player taps it to pair
    if(c.ch==='Q'){toWaiting(c,'exits the stack by Free Will');snap('move');}
    else if(G.hallPass){toWaiting(c,'uses the Hall Pass');snap('move');}
    return;
  }
  log(lab(c)+' is revealed in '+pos+'.',c.fx==='kier'||c.fx==='casey'?'good':'bad');
  snap('special',c.id);
  G.stacks[i].pop();G.gone.push(c);
  snap('effect',null,applySpecial(c,i));
}
function revealedMains(){return gridMains().map(function(o){return o.c;}).concat(G.waiting);}
function applySpecial(c,at){
  var ids=function(a){return a.map(function(o){return o.c.id;});};
  if(c.fx==='kier'){
    var ns=gridNums();
    if(ns.length){G.pending={type:'kier',max:2,ann:say(c,'Lumon always provides!','You may move up to two numerical cards away from their stacks.','good',{spots:ids(ns),choice:true})};}
    else say(c,'Lumon always provides!','But no numerical card is face up. No effect.','');
  }else if(c.fx==='casey'){
    var ms=caseyMains(),n2=gridNums();
    if(ms.length===1){
      var m=ms[0].c;
      say(c,'Wellness Session',c.name+' escorts '+lab(m)+' out of the grid to wait for '+(m.red?'the outie':'the innie')+'.','good',{spots:[m.id]});
      toWaiting(m,'is escorted out by '+c.name);
    }
    else if(ms.length){G.pending={type:'casey',mode:'main',max:1,who:c.name,ann:say(c,'Wellness Session',c.name+' can set one unmatched character aside to wait for their pair.','good',{spots:ids(ms),choice:true})};}
    else if(n2.length){G.pending={type:'casey',mode:'num',max:1,who:c.name,ann:say(c,'Wellness Session','No unmatched character is on the grid, so '+c.name+' can move one numerical card away.','good',{spots:ids(n2),choice:true})};}
    else say(c,'Wellness Session',c.name+' finds nobody to help. No effect.','');
  }else if(c.fx==='drummond'){
    var z=at===undefined?-1:zoneOf(at);
    // a zone is "in use" when it is unlocked and has a face-up number card to refine with
    var lock=z>0&&G.stacks.slice(z*4,z*4+4).some(function(s){return s.length;})&&numsOutside(z);
    say(c,'Workplace Reforms',(lock?'Zone '+(z+1)+' is locked until your next refined pair. ':z===0?'He cannot lock Zone 1. ':z>0?'He cannot lock Zone '+(z+1)+', as no other zone has a face-up number card to refine with. ':'')+
      'The order of the draw pile and of every Zone 4 stack is reversed.','bad',{stacks:[12,13,14,15],draw:true});
    if(lock)G.lockZone=z;
    G.draw.reverse();
    for(var i=12;i<16;i++){var st=G.stacks[i];if(st.length>1){st.reverse();st.forEach(function(x){x.faceUp=false;});}}
    return {stacks:[12,13,14,15],draw:true};
  }else if(c.fx==='graner'){
    var cand=revealedMains();
    G.paired.forEach(function(p){cand=cand.concat(p.cards);});
    if(!cand.length){say(c,'Break Room','No main character has been revealed yet. No effect.','');return null;}
    var v=pick(cand),wasPaired=isPaired(v.ch),k=15;
    say(c,'Break Room','Graner sends '+lab(v)+' face down to the bottom of the last stack in Zone 4.'+(wasPaired?' The '+CHARS[v.ch]+' pair is broken; the other half waits aside.':''),'bad',{spots:[v.id],stacks:[k]});
    if(wasPaired){
      var p=G.paired.filter(function(q){return q.ch===v.ch;})[0];
      G.paired=G.paired.filter(function(q){return q!==p;});
      p.cards.forEach(function(x){if(x.id!==v.id)G.waiting.push(x);});
    }else{takeFromGrid(v.id);G.waiting=G.waiting.filter(function(w){return w.id!==v.id;});}
    v.faceUp=false;G.stacks[k].unshift(v);
    return {stacks:[k]};
  }else if(c.fx==='dieter'){
    say(c,'Dieter Eagan','Two cards are drawn from the draw pile. Number cards he draws stay in your hand until you refine them.','good',{draw:true});
    G.queue.unshift({t:'draw',dieter:1},{t:'draw',dieter:2});
  }else if(c.fx==='jame'){
    var rep=['milchick','cobel'].filter(function(f){return G.seen[f];});
    say(c,'Jame Eagan','Helly\'s Defiance is cancelled for the rest of the quarter, and she cannot be paired until your next refined pair.'+
      (rep.length?' '+rep.map(function(f){return SPECIALS[f];}).join(' and ')+' ha'+(rep.length>1?'ve':'s')+' already appeared, so their abilit'+(rep.length>1?'ies play':'y plays')+' again.':''),'bad');
    G.defianceUsed=true;G.hellyPause=true;
    rep.forEach(function(f){
      if(G.over||G.pending)return;
      snap('effect',null,applySpecial({id:f==='milchick'?'BJ':'RJ',type:'special',fx:f,name:SPECIALS[f],faceUp:true}));
    });
  }else if(c.fx==='reghabi'){
    var hid=[];G.stacks.forEach(function(s,n){if(s.some(function(x){return !x.faceUp;}))hid.push(n);});
    var rv=revealedMains().length;
    if(!hid.length){say(c,'Reintegration','No stack hides a card for Reghabi to search. No effect.','');return null;}
    G.pending={type:'reghabi',max:rv?2:4,ann:say(c,'Reintegration',rv
      ?'Choose up to two stacks. Reghabi searches their hidden cards and frees any match of a revealed main character.'
      :'No main character is revealed, so choose up to four stacks. Reghabi frees the first hidden main character she finds.','good',{stacks:hid,choice:true})};
  }else if(c.fx==='cobel'||c.fx==='milchick'){
    G.seen[c.fx]=true;
    var helly=G.waiting.filter(function(w){return w.ch==='Q';})[0],defy=!G.defianceUsed&&!!helly;
    if(defy){
      G.defianceUsed=true;
      say(c,'Defiance!','Helly cancels '+c.name+"'s effect. She cannot do this again.",'good',{spots:[helly.id]});
    }else if(c.fx==='cobel'){
      if(G.paired.length){
        var sp=pick(G.paired),ks=[pickStack(),pickStack()];
        say(c,'Reintegration Check','Cobel splits the '+CHARS[sp.ch]+' pair and hides both cards face down in '+stackList(ks)+'.','bad',
          {spots:sp.cards.map(function(x){return x.id;}),stacks:ks});
        G.paired=G.paired.filter(function(q){return q!==sp;});
        sp.cards.forEach(function(x,n){insertHidden(x,ks[n]);});
        return {stacks:ks};
      }
      var nn=gridNums();
      if(nn.length){var o=pick(nn);
        say(c,'Kier, Chosen One, Kier!','Nobody is paired, so Cobel blocks '+lab(o.c)+' in Zone '+(zoneOf(o.i)+1)+' until your next refined pair.','bad',{spots:[o.c.id]});
        G.blocked=o.c.id;}
      else say(c,'Kier, Chosen One, Kier!','Cobel finds no pair to split and no numerical card to block. No effect.','');
    }else{
      var cards=(G.lastPair||[]).filter(function(x){return G.gone.indexOf(x)>=0;});
      if(cards.length){
        var dest;
        if(G.opts.lenient){var open=[];for(var s=0;s<16;s++)if(G.unlocked[zoneOf(s)])open.push(s);dest=cards.map(function(){return pick(open);});}
        else dest=cards.map(pickStack);
        say(c,'AGAIN!','Milchick returns your last refined cards ('+cards.map(lab).join(', ')+') '+(G.opts.lenient?'face up':'face down')+' to '+stackList(dest)+'.','bad',{limbo:cards,stacks:dest});
        G.lastPair=null;G.gone=G.gone.filter(function(x){return cards.indexOf(x)<0;});
        cards.forEach(function(x,n){x.keep=false;if(G.opts.lenient){x.faceUp=true;G.stacks[dest[n]].push(x);}else insertHidden(x,dest[n]);});
        return {stacks:dest};
      }
      say(c,'AGAIN!','There is no refined pair to return. No effect.','');
    }
  }
  return null;
}
function reshuffle(){
  var back=G.gone.filter(function(x){return x.adm;});
  say({id:'GM',type:'special',fx:'casey',name:'Gemma',faceUp:true},'Administration reshuffled',
    'The draw pile ran out while Zone 3 still hides cards, so its '+back.length+' used cards, Gemma among them, are reshuffled and used once more.'+
    (G.hallPass?'':' After your next refined pair, main characters get the Hall Pass and exit their stacks when they appear.'),'good',{draw:true});
  G.gone=G.gone.filter(function(x){return !x.adm;});
  back.forEach(function(x){x.faceUp=false;x.keep=false;});
  G.draw=shuffle(back);G.reshuffled=true;
  if(!G.hallPass)G.hallPending=true;
  snap('effect',null,{draw:true});
}
function drawOne(q){
  var dt=q&&q.dieter?"Dieter's "+(q.dieter===1?'first':'second')+' draw':'';
  if(!G.draw.length){log((dt?dt+': ':'')+'the draw pile is empty; nothing to draw.');return;}
  var c=G.draw.pop();c.faceUp=true;
  if(c.type==='num'){
    G.hand.push(c);
    if(dt){c.keep=true;log(dt+' is '+lab(c)+'. It stays in your hand until you refine it.','good');}
    else log('You drew '+lab(c)+'.'+(G.hand.some(function(h){return h.keep;})?' It can be used together with Dieter\'s cards.':'')+(wasteDepth()>1?' It goes on top of your earlier draws.':''));
    snap('draw');}
  else{log(dt?dt+' is '+lab(c)+', so its ability applies instead of a number card.':'You drew '+lab(c)+'.',c.fx==='cobel'||c.fx==='milchick'||c.fx==='jame'?'bad':'good');
    G.gone.push(c);snap('effect',null,applySpecial(c));}
}
function numsOutside(z){
  return rawTops().some(function(o){return o.c.type==='num'&&o.c.id!==G.blocked&&G.unlocked[zoneOf(o.i)]&&zoneOf(o.i)!==z;});
}
// Safety net: Drummond's lock may never leave the grid without a usable number card.
function liftStaleLock(){
  if(G.lockZone===null||numsOutside(G.lockZone))return false;
  log('No face-up number card is left outside Zone '+(G.lockZone+1)+', so Drummond\'s lock is lifted.','good');
  G.lockZone=null;return true;
}
// An employee whose innie and outie both wait aside is reunited at once.
function pairWaiting(){
  for(var n=0;n<CHAR_ORDER.length;n++){
    var ch=CHAR_ORDER[n];
    if(isPaired(ch)||(ch==='Q'&&G.hellyPause))continue;
    var both=G.waiting.filter(function(w){return w.ch===ch;}).slice(0,2);
    if(both.length<2)continue;
    G.waiting=G.waiting.filter(function(w){return both.indexOf(w)<0;});
    G.paired.push({ch:ch,cards:both});
    log(CHARS[ch]+'\'s innie and outie both wait aside, so they are reunited ('+G.paired.length+' of 4).','good');snap('pair');
    if(G.paired.length===4){G.over='win';G.queue=[];log('You hit quota! All main characters are paired.','good');snap('end');}
    return true;
  }
  return false;
}
function settle(){
  for(;;){
    if(G.over)return;
    if(G.pending)return;
    if(liftStaleLock()){snap('unlock');continue;}
    if(pairWaiting())continue;
    if(revealOne())continue;
    if(!G.draw.length&&canReshuffle()){reshuffle();continue;}
    if(G.queue.length){drawOne(G.queue.shift());continue;}
    if(!hasValidPair()&&!G.draw.length&&!pairable().length&&(G.lockZone!==null||G.hellyPause)){
      // nothing else can be done, so the one-round restrictions end early
      if(G.lockZone!==null){log('No other move is possible, so Drummond\'s lock on Zone '+(G.lockZone+1)+' is lifted.');G.lockZone=null;}
      if(G.hellyPause){log('No other move is possible, so Helly may be paired again.');G.hellyPause=false;}
      snap('unlock');continue;
    }
    break;
  }
  if(!hasValidPair()&&!G.draw.length&&!pairable().length){
    G.over='lose';log('No valid moves are left and the draw pile is empty. The quarter is lost.','bad');snap('end');
  }
}
function beginTurn(){G.turn++;}
function pairIds(){var ids=[];pairable().forEach(function(ch){mainCards(ch).forEach(function(c){ids.push(c.id);});});return ids;}

/* ---- player actions ---- */
function actDraw(){
  if(!canDraw())return false;
  beginTurn();
  drawOne();settle();return true;
}
function actRefine(ids){
  if(G.over||G.pending)return false;
  var t=selectionTotals(ids);if(!t.ok||priorityFix(ids).changed)return false;
  beginTurn();
  var cards=usableNums().filter(function(c){return ids.indexOf(c.id)>=0;});
  var reds=cards.filter(function(c){return c.red;}).map(lab).join(' + '),blks=cards.filter(function(c){return !c.red;}).map(lab).join(' + ');
  cards.forEach(function(c){if(!takeFromGrid(c.id))G.hand=G.hand.filter(function(h){return h.id!==c.id;});});
  cards.forEach(function(c){c.keep=false;G.gone.push(c);});
  G.lastPair=cards;
  log('Refined '+reds+' with '+blks+' (both total '+t.r+').','good');
  if(G.blocked){G.blocked=null;log('Cobel steps away; her blocked card is free again.','good');}
  if(G.lockZone!==null){log('Drummond\'s lock on Zone '+(G.lockZone+1)+' ends.','good');G.lockZone=null;}
  if(G.hellyPause){G.hellyPause=false;log('Helly may be paired again.','good');}
  snap('refine');
  if(G.hallPending&&!G.hallPass){
    G.hallPass=true;G.hallPending=false;log('Hall Pass is now active.','good');
    gridMains().forEach(function(o){if(!mateWaiting(o.c.ch)){toWaiting(o.c,'uses the Hall Pass');snap('move');}});
  }
  settle();return true;
}
function pendingCandidates(){
  if(!G.pending)return [];
  if(G.pending.type==='reghabi'){var out=[];G.stacks.forEach(function(s,n){if(s.some(function(x){return !x.faceUp;}))out.push('S'+n);});return out;}
  if(G.pending.type==='casey'&&G.pending.mode==='main')return caseyMains().map(function(o){return o.c.id;});
  return gridNums().map(function(o){return o.c.id;});
}
function actResolve(ids){
  var p=G.pending;if(!p||G.over)return false;
  var cand=pendingCandidates();
  ids=ids.filter(function(id){return cand.indexOf(id)>=0;}).slice(0,p.max);
  beginTurn();G.pending=null;
  var who=p.type==='kier'?"Kier's offer":p.type==='reghabi'?"Reghabi's search":p.who+"'s session";
  if(!ids.length){log(who+' is declined.');}
  else if(p.type==='reghabi'){
    var want=revealedMains().map(function(c){return c.ch;}),freed=[],anyMode=!want.length;
    ids.forEach(function(sid){
      G.stacks[+sid.slice(1)].filter(function(x){return !x.faceUp&&x.type==='main';}).forEach(function(x){
        if(anyMode?freed.length===0:want.indexOf(x.ch)>=0)freed.push(x);
      });
    });
    if(!freed.length)log('Reghabi searches '+ids.length+' stack'+(ids.length>1?'s':'')+' and finds no '+(anyMode?'main character':'match')+'.');
    freed.forEach(function(x){toWaiting(x,'is found by Reghabi');snap('move');});
  }
  else if(p.type==='kier'||p.mode==='num'){
    var names=ids.map(function(id){var f=takeFromGrid(id);G.gone.push(f.c);return lab(f.c);});
    log((p.type==='kier'?'Kier':p.who)+' moves away '+names.join(' and ')+'.','good');
  }else{toWaiting(findCard(ids[0]).c,'is escorted out by '+p.who);}
  snap('move');
  settle();return true;
}
function cardCount(){
  var n=G.draw.length+G.hand.length+G.waiting.length+G.gone.length;
  G.stacks.forEach(function(s){n+=s.length;});G.paired.forEach(function(p){n+=p.cards.length;});return n;
}
