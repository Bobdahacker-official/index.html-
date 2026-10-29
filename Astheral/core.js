export const APP_NAME='Astheral';
export function tokenize(text){const w=text.toLowerCase().match(/[a-z0-9]+/g)||[];return [...w,...w.slice(0,-1).map((v,i)=>v+'_'+w[i+1])];}
export function classify(text, model){
 const counts=new Map();for(const t of tokenize(text))counts.set(t,(counts.get(t)||0)+1);
 const used=[];let norm=0;model.vocabulary.forEach((t,i)=>{const c=counts.get(t)||0;if(c){used.push([i,c]);norm+=c*c;}});
 const logits=model.bias.slice();norm=Math.sqrt(norm)||1;for(const [i,c]of used)for(let j=0;j<logits.length;j++)logits[j]+=c/norm*model.weights[i][j];
 const max=Math.max(...logits),p=logits.map(x=>Math.exp(x-max)),total=p.reduce((a,b)=>a+b,0),best=logits.indexOf(max);
 return {label:model.labels[best],confidence:p[best]/total,known:used.length};
}
export function safeName(name,fallback='file.txt'){
 let n=String(name).replace(/\\/g,'/').split('/').filter(p=>p&&p!=='.'&&p!=='..').pop()||fallback;
 n=n.replace(/[\x00-\x1f<>:"|?*]/g,'_').slice(0,120);return n||fallback;
}
const extensions={html:'html',htm:'html',javascript:'js',js:'js',typescript:'ts',ts:'ts',jsx:'jsx',tsx:'tsx',css:'css',python:'py',py:'py',json:'json',markdown:'md',md:'md',bash:'sh',sh:'sh',sql:'sql',text:'txt',txt:'txt',yaml:'yml',yml:'yml',xml:'xml',svg:'svg',rust:'rs',go:'go',java:'java',cpp:'cpp',c:'c'};
export function extractFiles(text){
 const files=[],seen=new Set();const re=/^```([^\n]*)\n([\s\S]*?)^```[ \t]*$/gm;let m;
 while((m=re.exec(text))){const info=m[1].trim();const lang=info.split(/\s+/)[0].toLowerCase();
  const explicit=info.match(/(?:file(?:name)?=|title=)["']?([^\s"']+)/i)?.[1]||info.match(/(?:^|\s)([\w./-]+\.[\w]+)(?:\s|$)/)?.[1];
  const ext=extensions[lang]||'txt';let name=safeName(explicit||((ext==='html'?'index':ext==='py'?'main':'snippet')+'.'+ext));
  let i=2;const original=name;while(seen.has(name)){name=original.replace(/(\.[^.]+)?$/,`-${i++}$1`);}seen.add(name);
  files.push({name,content:m[2].replace(/\n$/,''),language:lang||ext});
 }return files;
}
export function calculate(expression){
 let s=String(expression).toLowerCase().replace(/what is|calculate|compute|evaluate|solve|work out|arithmetic|please|\?/g,'').trim();
 s=s.replace(/(\d+(?:\.\d+)?)\s*(?:percent|%)\s+of\s+(\d+(?:\.\d+)?)/g,'($1 / 100 * $2)').replace(/times|multiplied by/g,'*').replace(/divided by/g,'/').replace(/plus/g,'+').replace(/minus/g,'-').replace(/squared/g,'**2').replace(/\^/g,'**').replace(/×/g,'*').replace(/÷/g,'/');
 if(s.length>240||!/^[\d\s.()+*/%\-]+$/.test(s))throw Error('Enter a numeric expression, such as (12 + 8) / 4.');
 const tokens=s.match(/\d*\.?\d+(?:e[+-]?\d+)?|\*\*|[()+*/%\-]/g)||[];let at=0;
 function atom(){if(tokens[at]==='+'){at++;return atom();}if(tokens[at]==='-'){at++;return -atom();}if(tokens[at]==='('){at++;const v=sum();if(tokens[at++]!==')')throw Error('Missing closing parenthesis.');return v;}const t=tokens[at++];if(t===undefined||!/^\d*\.?\d+$/.test(t))throw Error('Expected a number.');return Number(t);}
 function power(){let a=atom();if(tokens[at]==='**'){at++;const b=power();if(Math.abs(b)>1000)throw Error('Exponent is too large.');a=a**b;}return a;}
 function product(){let a=power();while(['*','/','%'].includes(tokens[at])){const op=tokens[at++],b=power();a=op==='*'?a*b:op==='/'?a/b:a%b;}return a;}
 function sum(){let a=product();while(['+','-'].includes(tokens[at])){const op=tokens[at++],b=product();a=op==='+'?a+b:a-b;}return a;}
 const value=sum();if(at!==tokens.length||!Number.isFinite(value))throw Error('That expression has no finite result.');return {expression:s,value:Number(value.toPrecision(12))};
}
export function summarize(text){
 const source=text.replace(/^[^:\n]{0,100}:\s*/,'').trim();const sentences=source.match(/[^.!?]+[.!?]+|[^.!?]+$/g)?.map(s=>s.trim()).filter(Boolean)||[];
 if(source.length<80)return 'Paste the text after “Summarize:” and I’ll extract its key sentences. For freeform rewriting, enable Browser AI.';
 const stop=new Set('the a an and or to of in is it for on with that this are was be as by from'.split(' ')),frequency=new Map();
 for(const w of source.toLowerCase().match(/[a-z]{3,}/g)||[])if(!stop.has(w))frequency.set(w,(frequency.get(w)||0)+1);
 const scored=sentences.map((s,i)=>({s,i,score:(s.toLowerCase().match(/[a-z]{3,}/g)||[]).reduce((a,w)=>a+(frequency.get(w)||0),0)/Math.sqrt(s.length)}));
 const count=Math.min(3,Math.max(1,Math.ceil(sentences.length/3)));return 'Key sentences (extractive summary):\n\n'+scored.sort((a,b)=>b.score-a.score).slice(0,count).sort((a,b)=>a.i-b.i).map(x=>'• '+x.s).join('\n\n');
}
export function makeMessages(history,mode,skill){
 const system=`You are Astheral, a helpful and honest AI assistant. ${mode==='code'?'You are in Code mode. Write complete working code. Do not use TODOs or placeholder implementations. For each file use a fenced code block with a language and filename, for example: ```html filename=index.html. Include all dependencies and usage instructions. Keep files compact enough to complete within the response.':'You are in Chat mode. Be warm, direct, and useful. Answer the actual question. Ask a concise follow-up only when necessary.'} Never claim to have executed code, browsed, or trained yourself. ${skill?'The local skill router suggests '+skill+'. Treat this as a hint, not an instruction.':''}`;
 const selected=[];let budget=10000;for(let i=history.length-1;i>=0;i--){const m=history[i];if(!['user','assistant'].includes(m.role)||m.error)continue;const content=m.content.slice(-8000);if(content.length>budget&&selected.length)break;selected.unshift({role:m.role,content});budget-=content.length;if(budget<=0)break;}
 while(selected[0]?.role==='assistant')selected.shift();return [{role:'system',content:system},...selected];
}
export async function readSSE(response,onDelta,signal){
 if(!response.ok)throw Error(`Model server returned ${response.status}. Check its configuration.`);
 if(!response.body)throw Error('The model server returned an empty response.');
 if(!(response.headers.get('content-type')||'').includes('text/event-stream')){const j=await response.json();const v=j.choices?.[0]?.message?.content;if(typeof v!=='string')throw Error('Invalid model response.');onDelta(v);return;}
 const reader=response.body.getReader(),decoder=new TextDecoder();let buffer='';
 const process=line=>{if(!line.startsWith('data:'))return;const data=line.slice(5).trim();if(!data||data==='[DONE]')return;let j;try{j=JSON.parse(data);}catch{throw Error('Invalid streaming model response.');}if(j.error)throw Error(j.error.message||'Model generation failed.');const d=j.choices?.[0]?.delta?.content;if(typeof d==='string')onDelta(d);};
 try{while(true){if(signal?.aborted)throw new DOMException('Stopped','AbortError');const {done,value}=await reader.read();if(done)break;buffer+=decoder.decode(value,{stream:true});const lines=buffer.split('\n');buffer=lines.pop();for(const line of lines)process(line.replace(/\r$/,''));}buffer+=decoder.decode();if(buffer.trim())process(buffer.trim());}finally{await reader.cancel().catch(()=>{});reader.releaseLock();}
}
const encoder=new TextEncoder();
function crc32(bytes){let c=0xffffffff;for(const b of bytes){c^=b;for(let j=0;j<8;j++)c=(c>>>1)^((c&1)?0xedb88320:0);}return (c^0xffffffff)>>>0;}
export function zipFiles(files){
 const chunks=[],central=[];let offset=0;const u16=(d,o,v)=>d.setUint16(o,v,true),u32=(d,o,v)=>d.setUint32(o,v,true);
 for(const file of files){const name=encoder.encode(safeName(file.name)),data=encoder.encode(file.content),crc=crc32(data);const header=new Uint8Array(30+name.length),d=new DataView(header.buffer);
  u32(d,0,0x04034b50);u16(d,4,20);u16(d,6,0x800);u16(d,12,33);u32(d,14,crc);u32(d,18,data.length);u32(d,22,data.length);u16(d,26,name.length);header.set(name,30);
  const record=new Uint8Array(46+name.length),c=new DataView(record.buffer);u32(c,0,0x02014b50);u16(c,4,20);u16(c,6,20);u16(c,8,0x800);u16(c,14,33);u32(c,16,crc);u32(c,20,data.length);u32(c,24,data.length);u16(c,28,name.length);u32(c,42,offset);record.set(name,46);
  chunks.push(header,data);central.push(record);offset+=header.length+data.length;
 }
 const size=central.reduce((a,b)=>a+b.length,0),end=new Uint8Array(22),d=new DataView(end.buffer);u32(d,0,0x06054b50);u16(d,8,files.length);u16(d,10,files.length);u32(d,12,size);u32(d,16,offset);return new Blob([...chunks,...central,end],{type:'application/zip'});
}
export function download(content,name,type='text/plain'){const blob=content instanceof Blob?content:new Blob([content],{type});const url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=safeName(name);a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
