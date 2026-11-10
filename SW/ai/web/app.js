'use strict';
let state, dirty = false, busy = false;
const $ = id => document.getElementById(id);
function notice(message, error = false) { $('notice').textContent = message; $('notice').classList.toggle('error', error); }
function config() { return {mask: [...document.querySelectorAll('.feature input:checked')].reduce((v, e) => v | Number(e.value), 0), score: Number($('score').value), face_threshold: Number($('face').value), period_us: Number($('period').value) * 1000}; }
function values(c) { document.querySelectorAll('.feature input').forEach(e => { e.checked = Boolean(c.mask & Number(e.value)); }); $('score').value = c.score; $('face').value = c.face_threshold; $('period').value = c.period_us / 1000; outputs(); }
function outputs() { $('score-value').textContent = (Number($('score').value)/1e6).toFixed(2); $('face-value').textContent = (Number($('face').value)/1e6).toFixed(2); }
async function post(path, body, headers = {}) { const response = await fetch(path, {method:'POST', headers:{'X-Camera-Token':state.token, 'Content-Type':'application/json', ...headers}, body: typeof body === 'string' || body instanceof File ? body : JSON.stringify(body)}); const result = await response.json(); if(!response.ok) throw Error(result.error); return result; }
async function action(fn) { if(busy) return; busy = true; try { await fn(); await refresh(); } catch(e) { notice(e.message, true); } finally { busy = false; } }
function chip(text, onClick) { const e = document.createElement(onClick ? 'button' : 'span'); e.className = 'chip'; e.textContent = text; if(onClick) e.addEventListener('click',onClick); return e; }
async function refresh() {
  try {
    const r = await fetch('/api/status'); const s = await r.json(); if(!r.ok) throw Error(s.error); state = s;
    if(!$('features').children.length) {
      s.features.forEach(f => {const label = document.createElement('label'); label.className='feature'; const title=document.createElement('strong'); title.textContent=f.name; const input=document.createElement('input'); input.type='checkbox'; input.value=f.bit; input.setAttribute('aria-label',f.name); input.addEventListener('change',()=>{dirty=true;}); const small=document.createElement('small'); small.textContent=f.description; label.append(title,input,small); $('features').append(label);});
    }
    document.querySelectorAll('.feature input').forEach(e=>{e.disabled=!(s.supported & Number(e.value)) || s.owned;});
    if(!dirty) values(s.config);
    $('apply').disabled = s.owned || s.streaming || (!s.available && !s.preview);
    $('connection').textContent = s.preview ? 'Offline preview' : s.transport === 'ethernet' ? 'GigE + HTTP console' : 'USB device console';
    $('transport').textContent = s.preview ? 'Offline preview' : s.transport === 'ethernet' ? 'GigE Ethernet' : 'USB Ethernet';
    $('address').textContent = s.preview ? location.host : s.device_address;
    $('storage-hint').textContent = (s.storage_persistent ? 'Profiles are stored on the camera’s MicroSD. ' : s.preview ? 'Preview storage is local to this computer. ' : 'No MicroSD storage. Changes are lost at reboot. ') + 'Loading fills the editor; apply when ready. Profiles never enable AI automatically at boot.';
    $('mode').textContent = s.preview ? 'OFFLINE PREVIEW' : 'LOCAL DEVICE';
    $('worker').textContent = s.preview ? 'Preview' : s.available ? 'Online' : 'Unavailable';
    $('latency').textContent = s.inference_us ? (s.inference_us / 1e6).toFixed(2)+' s' : '—';
    $('enabled').textContent = s.features.filter(f=>s.config.mask & f.bit).length+' / 8';
    $('references').textContent = s.gallery.reduce((n,g)=>n+g.images,0);
    $('generation').textContent=s.generation; $('error').textContent=s.error+' · '+(['Success','Worker exception','Rejected result','Cancelled / stale configuration'][s.error] || 'Unknown');
    $('authority').textContent=s.owned?'GenICam client · web edits locked':s.web_active?'Web console · active lease':'Idle';
    $('gallery-health').textContent=s.worker ? `${s.worker.gallery_count} enrolled references · ${s.worker.gallery_errors} rejected references${s.worker.age_s>30?' · status stale':''}` : 'Waiting for worker validation';
    $('profile-list').replaceChildren(...Object.entries(s.profiles).map(([name,c])=>chip(name,()=>{values(c);dirty=true;notice('Profile loaded into editor. Apply to camera when ready.');})));
    if(!$('profile-list').children.length) $('profile-list').append(chip('No saved profiles yet'));
    $('gallery-list').replaceChildren(...s.gallery.map(g=>chip(`${g.name} · ${g.images} reference${g.images===1?'':'s'}`)));
    if(!$('gallery-list').children.length) $('gallery-list').append(chip('Empty gallery · unmatched faces return unknown'));
    if(!dirty) notice(s.preview?'Offline preview. Changes affect preview storage only; no camera is connected.':s.owned?'A GenICam client controls this camera. Release its session before applying web settings.':s.available?'Camera connected. Configure AI here, then start FPGA RGB8/BGR8 acquisition in your GenICam client.':'The AI worker is unavailable. Check the camera console and reboot both processors if it stopped.',!s.available&&!s.preview);
  } catch(e) { $('connection').textContent='Disconnected'; $('apply').disabled=true; notice(e.message,true); }
}
$('settings').addEventListener('input',()=>{dirty=true;outputs();});
$('settings').addEventListener('submit',e=>{e.preventDefault();action(async()=>{await post('/api/config',config());dirty=false;notice('Configuration accepted by R5.');});});
$('disable').addEventListener('click',()=>{document.querySelectorAll('.feature input:not(:disabled)').forEach(e=>{e.checked=false;});dirty=true;notice('AI disabled in editor. Apply to camera to confirm.');});
$('save-profile').addEventListener('click',()=>action(async()=>{await post('/api/profiles',{name:$('profile-name').value,config:config()});notice('Profile saved to camera storage.');}));
$('export').addEventListener('click',()=>{if(!state)return;const url=URL.createObjectURL(new Blob([JSON.stringify({version:1,profiles:state.profiles},null,2)],{type:'application/json'}));const a=document.createElement('a');a.href=url;a.download='maxcamera-profiles.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);});
$('import').addEventListener('change',()=>action(async()=>{const file=$('import').files[0];if(!file)return;if(file.size>65536)throw Error('Profile JSON must be smaller than 64 KiB');const data=JSON.parse(await file.text());if(data.version!==1 || typeof data.profiles!=='object' || !data.profiles)throw Error('Unsupported profile file');for(const [name,c] of Object.entries(data.profiles))await post('/api/profiles',{name,config:c});notice('Profiles imported. Select a profile to edit.');}));
$('enroll').addEventListener('submit',e=>{e.preventDefault();action(async()=>{const file=$('reference').files[0];if(!file)throw Error('Choose a reference image');const r=await post('/api/gallery',file,{'Content-Type':'application/octet-stream','X-Identity':$('identity').value});notice(r.message);$('reference').value='';});});
refresh();setInterval(()=>{if(!busy)refresh();},3000);
