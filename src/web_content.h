#pragma once

#include <Arduino.h>

// Kompaktes Single-Page Web UI (PROGMEM)
static const char WEB_INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ROTOR WLAN Bridge</title>
<style>
:root{
  --bg:#070b14;
  --panel:#101827;
  --panel2:#152033;
  --line:#243044;
  --text:#e8eef7;
  --muted:#93a4b8;
  --blue:#3b82f6;
  --blue2:#60a5fa;
  --ok:#34d399;
  --warn:#fbbf24;
  --err:#f87171;
  --white:#ffffff;
  --gray:#9aa8bc;
}
*{box-sizing:border-box}
body{
  margin:0;min-height:100vh;color:var(--text);
  font:15px/1.45 "Segoe UI",system-ui,sans-serif;
  background:
    radial-gradient(1200px 600px at 10% -10%,#132038 0%,transparent 55%),
    radial-gradient(900px 500px at 100% 0%,#0d1a33 0%,transparent 50%),
    var(--bg);
}
header{
  display:flex;align-items:center;justify-content:space-between;gap:1rem;
  padding:1.1rem 1.4rem;border-bottom:1px solid var(--line);
  background:rgba(16,24,39,.82);backdrop-filter:blur(10px);
  position:sticky;top:0;z-index:5;
}
.brand{display:flex;flex-direction:column;gap:.15rem}
.brand b{font-size:1.15rem;letter-spacing:.02em;color:var(--white)}
.brand span{color:var(--muted);font-size:.85rem}
.pill{
  display:inline-flex;align-items:center;gap:.4rem;
  padding:.35rem .7rem;border-radius:999px;border:1px solid var(--line);
  background:var(--panel2);color:var(--gray);font-size:.8rem
}
.dot{width:.55rem;height:.55rem;border-radius:50%;background:var(--warn)}
.dot.on{background:var(--ok);box-shadow:0 0 10px rgba(52,211,153,.55)}
main{max-width:920px;margin:0 auto;padding:1.25rem}
nav{display:flex;gap:.5rem;margin-bottom:1rem;flex-wrap:wrap}
nav button{
  appearance:none;border:1px solid var(--line);background:var(--panel);
  color:var(--muted);padding:.55rem 1rem;border-radius:.7rem;cursor:pointer
}
nav button.active{color:var(--white);border-color:#2f5fad;background:linear-gradient(180deg,#1a2d4d,#14233d)}
.card{
  background:linear-gradient(180deg,rgba(21,32,51,.95),rgba(16,24,39,.95));
  border:1px solid var(--line);border-radius:1rem;padding:1.1rem 1.2rem;margin-bottom:1rem
}
.card h2{margin:0 0 .85rem;font-size:1rem;color:var(--blue2);font-weight:600}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:.75rem}
.kv{background:rgba(7,11,20,.45);border:1px solid var(--line);border-radius:.75rem;padding:.7rem .8rem}
.kv .k{color:var(--muted);font-size:.75rem;text-transform:uppercase;letter-spacing:.04em}
.kv .v{margin-top:.2rem;color:var(--white);word-break:break-all}
label{display:block;color:var(--muted);font-size:.8rem;margin:0 0 .35rem}
input,select{
  width:100%;padding:.65rem .75rem;border-radius:.65rem;border:1px solid var(--line);
  background:#0c1320;color:var(--text);outline:none;margin-bottom:.85rem
}
input:focus,select:focus{border-color:var(--blue)}
.row{display:grid;grid-template-columns:1fr 1fr;gap:.75rem}
@media(max-width:640px){.row{grid-template-columns:1fr}}
.actions{display:flex;flex-wrap:wrap;gap:.55rem;margin-top:.4rem}
.btn{
  appearance:none;border:0;border-radius:.7rem;padding:.65rem 1rem;cursor:pointer;
  font-weight:600;color:var(--white);background:var(--blue);
  display:inline-flex;align-items:center;justify-content:center;
  font:inherit;font-size:.95rem;line-height:1.35;text-decoration:none;white-space:nowrap
}
.btn.sec{background:#223049;color:var(--text);border:1px solid var(--line)}
.btn.sec:hover{border-color:var(--blue);background:#263248}
.btn.danger{background:var(--err);color:var(--white)}
.btn.danger:hover{filter:brightness(1.08)}
button.btn:disabled{opacity:.5;cursor:wait}
.msg{min-height:1.2rem;color:var(--muted);font-size:.9rem;margin-top:.5rem}
.msg.ok{color:var(--ok)}.msg.err{color:var(--err)}
.scan{max-height:280px;overflow:auto;border:1px solid var(--line);border-radius:.75rem}
.scan table{width:100%;border-collapse:collapse;font-size:.88rem}
.scan th,.scan td{padding:.55rem .65rem;border-bottom:1px solid var(--line);text-align:left}
.scan th{color:var(--muted);position:sticky;top:0;background:#121c2d}
.scan tr{cursor:pointer}.scan tr:hover td{background:rgba(59,130,246,.08)}
.tag{display:inline-block;padding:.1rem .45rem;border-radius:.4rem;font-size:.72rem;background:#1c2b45;color:var(--blue2)}
.tag.g5{background:#1a2740;color:#93c5fd}
.filepick{
  display:flex;align-items:stretch;gap:.65rem;flex-wrap:wrap;
  margin-bottom:.85rem
}
.filepick input[type=file]{
  position:absolute;width:1px;height:1px;padding:0;margin:-1px;
  overflow:hidden;clip:rect(0,0,0,0);white-space:nowrap;border:0
}
.filepick-btn{min-width:10rem;flex-shrink:0}
.filepick-name{
  flex:1;min-width:12rem;display:flex;align-items:center;
  padding:.65rem .85rem;border:1px solid var(--line);border-radius:.65rem;
  background:#0c1320;color:var(--muted);font-size:.9rem;word-break:break-all
}
.filepick-name.on{color:var(--text);border-color:#2f5fad;background:rgba(26,45,77,.35)}
.foot{color:var(--muted);font-size:.8rem;text-align:center;padding:1rem 0 2rem}
</style>
</head>
<body>
<header>
  <div class="brand">
    <b>ROTOR WLAN Bridge</b>
    <span id="sub">ESP32-C5 · Wi‑Fi 6 · 2.4 / 5 GHz</span>
  </div>
  <div class="pill"><span class="dot" id="ldot"></span><span id="ltext">…</span></div>
</header>
<main>
  <nav>
    <button class="active" data-tab="status">Status</button>
    <button data-tab="wifi">WLAN</button>
    <button data-tab="rs485">RS485</button>
    <button data-tab="security">Sicherheit</button>
    <button data-tab="update">Update</button>
    <button data-tab="reset">Reset</button>
  </nav>

  <section id="tab-status">
    <div class="card">
      <h2>Gerät</h2>
      <div class="grid" id="statusGrid"></div>
    </div>
    <div class="card">
      <h2>Netzwerk & Bridge</h2>
      <div class="grid" id="netGrid"></div>
    </div>
  </section>

  <section id="tab-wifi" hidden>
    <div class="card">
      <h2>Mit Access Point verbinden</h2>
      <p style="margin:0 0 1rem;color:var(--muted);font-size:.9rem">
        Es gibt immer nur <b>einen</b> WLAN-Weg: SoftAP <b>oder</b> Infrastruktur — nie beides.
        Nach dem Verbinden mit einem Router ist der Modul‑AP aus; TCP läuft nur über die STA‑IP.
        Auslieferung / Werksreset: SoftAP auf <b>2,4 GHz</b> (<code>192.168.4.1</code>). SoftAP kann auf <b>5 GHz</b> umgestellt werden.
      </p>
      <div class="actions" style="margin-bottom:.8rem">
        <button class="btn sec" id="btnScan">Netze scannen</button>
      </div>
      <div class="scan" id="scanBox"><table><thead><tr><th>SSID</th><th>RSSI</th><th>Band</th><th>CH</th></tr></thead><tbody id="scanBody"><tr><td colspan="4" style="color:var(--muted)">Noch kein Scan</td></tr></tbody></table></div>
    </div>
    <div class="card">
      <h2>Einstellungen</h2>
      <div class="row">
        <div>
          <label>WLAN‑Betrieb</label>
          <select id="wifi_mode">
            <option value="0">SoftAP (Modul‑AP)</option>
            <option value="1">Infrastruktur (Router)</option>
          </select>
        </div>
        <div>
          <label>Band</label>
          <select id="wifi_band">
            <option value="1">2,4 GHz</option>
            <option value="2">5 GHz</option>
            <option value="0">Auto (nur Infrastruktur)</option>
          </select>
        </div>
      </div>
      <p style="margin:0 0 .85rem;color:var(--muted);font-size:.85rem">
        SoftAP: Band wählt 2,4 oder 5 GHz für den Modul‑AP. Infrastruktur: Band für den Router‑Scan/Join.
      </p>
      <label>SSID (Router)</label>
      <input id="ssid" maxlength="32" placeholder="Netzwerkname"/>
      <label>Passwort</label>
      <input id="pass" type="password" maxlength="64" placeholder="leer = unverändert / offen"/>
      <label><input type="checkbox" id="dhcp" checked style="width:auto;margin-right:.4rem"/>DHCP verwenden</label>
      <div id="staticBox" hidden>
        <div class="row">
          <div><label>IP</label><input id="ip"/></div>
          <div><label>Netzmaske</label><input id="mask"/></div>
        </div>
        <div class="row">
          <div><label>Gateway</label><input id="gw"/></div>
          <div><label>DNS</label><input id="dns"/></div>
        </div>
      </div>
      <div class="actions">
        <button class="btn" id="btnApply">Speichern / Verbinden</button>
      </div>
      <div class="msg" id="msg"></div>
    </div>
  </section>

  <section id="tab-rs485" hidden>
    <div class="card">
      <h2>Netzwerkprotokoll (Nutzdatenkanal)</h2>
      <p style="margin:0 0 1rem;color:var(--muted);font-size:.9rem">
        Transparenter Bytestrom laut Spezifikation: WLAN ↔ UART0/RS485 unverändert.
        TCP Server/Client sowie UDP Server/Client (UDP als Erweiterung zur Spec v4).
      </p>
      <div class="row">
        <div>
          <label>Betriebsart</label>
          <select id="net_mode">
            <option value="0">TCP Server</option>
            <option value="1">TCP Client</option>
            <option value="3">UDP Server</option>
            <option value="4">UDP Client</option>
            <option value="2">Disabled</option>
          </select>
        </div>
        <div>
          <label>Lokaler Port</label>
          <input id="local_port" type="number" min="1" max="65535" value="8886"/>
        </div>
      </div>
      <div class="row">
        <div>
          <label>Ziel‑IP für Client</label>
          <input id="remote_ip" placeholder="192.168.1.100"/>
        </div>
        <div>
          <label>Ziel‑Port für Client</label>
          <input id="remote_port" type="number" min="1" max="65535" value="8886"/>
        </div>
      </div>
      <label>Ziel‑Hostname (optional, TCP Client)</label>
      <input id="remote_host" placeholder="rotorserver.local"/>
      <div class="row">
        <div>
          <label>Reconnect (ms, TCP Client)</label>
          <input id="reconnect_ms" type="number" min="500" step="100" value="5000"/>
        </div>
        <div style="display:flex;flex-direction:column;justify-content:flex-end;padding-bottom:.85rem">
          <label><input type="checkbox" id="tcp_keepalive" checked style="width:auto;margin-right:.4rem"/>TCP Keepalive</label>
        </div>
      </div>
    </div>
    <div class="card">
      <h2>RS485 / Packetizer</h2>
      <div class="row">
        <div>
          <label>Baudrate</label>
          <input id="rs485_baud" value="115200" disabled/>
        </div>
        <div>
          <label>Busadresse (1–247)</label>
          <input id="bus_address" type="number" min="1" max="247" value="17"/>
        </div>
      </div>
      <div class="row">
        <div>
          <label>Packet‑Timeout (ms, 1–100)</label>
          <input id="packet_timeout_ms" type="number" min="1" max="100" value="2"/>
        </div>
        <div>
          <label>Packet‑Größe (32–1460)</label>
          <input id="packet_size" type="number" min="32" max="1460" value="1024"/>
        </div>
      </div>
      <label><input type="checkbox" id="bridge_enabled" checked style="width:auto;margin-right:.4rem"/>Bridge aktiv</label>
      <label><input type="checkbox" id="rs485_tx_allowed" checked style="width:auto;margin-right:.4rem"/>Netz → Serial (WLAN-Empfang auf UART ausgeben)</label>
      <label><input type="checkbox" id="rs485_rx_allowed" checked style="width:auto;margin-right:.4rem"/>Serial → Netz (UART-Empfang per WLAN senden)</label>
      <div class="actions">
        <button class="btn" id="btnRs485Save">Übernehmen &amp; speichern</button>
      </div>
      <div class="msg" id="msgRs485"></div>
    </div>
  </section>

  <section id="tab-security" hidden>
    <div class="card">
      <h2>Web‑Zugang</h2>
      <p style="margin:0 0 1rem;color:var(--muted);font-size:.9rem">
        Die Konfigurationsseite ist mit HTTP‑Basic‑Auth geschützt.
        Benutzername: <b>admin</b> · Werkspasswort: <b>Rotorconfig</b>
      </p>
      <label>Aktuelles Passwort</label>
      <input id="web_pass_cur" type="password" maxlength="64" autocomplete="current-password"/>
      <label>Neues Passwort</label>
      <input id="web_pass_new" type="password" maxlength="64" autocomplete="new-password"/>
      <label>Neues Passwort wiederholen</label>
      <input id="web_pass_rep" type="password" maxlength="64" autocomplete="new-password"/>
      <div class="actions">
        <button class="btn" id="btnWebPass">Passwort ändern</button>
      </div>
      <div class="msg" id="msgWebPass"></div>
    </div>
  </section>

  <section id="tab-update" hidden>
    <div class="card">
      <h2>Firmware‑Update (OTA)</h2>
      <p style="margin:0 0 1rem;color:var(--muted);font-size:.9rem">
        Nur <code>firmware.bin</code> aus dem PlatformIO‑Build hochladen (App‑Partition, <b>nicht</b> <code>firmware.factory.bin</code>).
        Nach erfolgreichem Upload startet das Modul neu und bootet aus dem zweiten OTA‑Slot.
      </p>
      <div class="grid" id="updateGrid"></div>
      <div class="filepick">
        <input id="fwFile" type="file" accept=".bin,application/octet-stream"/>
        <label for="fwFile" class="btn sec filepick-btn">Datei auswählen</label>
        <span class="filepick-name" id="fwFileName">Wählen Sie eine Firmwaredatei (.bin) aus</span>
      </div>
      <div class="actions">
        <button class="btn" id="btnFwUpload">Firmware hochladen</button>
      </div>
      <div class="msg" id="msgUpdate"></div>
    </div>
  </section>

  <section id="tab-reset" hidden>
    <div class="card">
      <h2>Neustart &amp; Werkseinstellung</h2>
      <p style="margin:0 0 1rem;color:var(--muted);font-size:.9rem">
        <b>Neu starten:</b> Firmware neu booten, gespeicherte Einstellungen bleiben erhalten.<br/>
        <b>Werkseinstellung:</b> Alle Konfiguration löschen (WLAN, Netzwerk, Passwort auf Werk —
        Web-Login <b>admin</b> / <b>Rotorconfig</b>, SoftAP <b>2,4 GHz</b>).
      </p>
      <div class="actions">
        <button class="btn sec" id="btnReboot">Modul neu starten</button>
        <button class="btn danger" id="btnFactoryReset">Werkseinstellung</button>
      </div>
      <div class="msg" id="msgReset"></div>
    </div>
  </section>

  <div class="foot" id="foot"></div>
</main>
<script>
const $=s=>document.querySelector(s);
const $$=s=>[...document.querySelectorAll(s)];
function kv(k,v){return `<div class="kv"><div class="k">${k}</div><div class="v">${v??'—'}</div></div>`}
async function api(path,opt){
  const r=await fetch(path,{credentials:'same-origin',...(opt||{})});
  if(r.status===401) throw new Error('Anmeldung erforderlich');
  if(!r.ok) throw new Error(await r.text());
  const ct=r.headers.get('content-type')||'';
  return ct.includes('json')?r.json():r.text();
}
function setLink(ok,text){
  $('#ldot').classList.toggle('on',!!ok);
  $('#ltext').textContent=text;
}
function fmtUptime(sec){
  sec=Math.max(0,Math.floor(Number(sec)||0));
  const h=Math.floor(sec/3600);
  const m=Math.floor((sec%3600)/60);
  const s=sec%60;
  const p=[];
  if(h>0) p.push(h+' h');
  p.push(m+' min');
  p.push(s+' s');
  return p.join(' ');
}
async function loadStatus(){
  const s=await api('/api/status');
  setLink(s.wifi_up,s.link_text);
  $('#sub').textContent=`${s.device_name} · ${s.uid} · FW ${s.fw}`;
  $('#statusGrid').innerHTML=[
    kv('Gerätename',s.device_name),kv('UID',s.uid),kv('MAC',s.mac),
    kv('Busadresse',s.bus_address),kv('Firmware',s.fw),kv('Hardware',s.hw),
    kv('Uptime',fmtUptime(s.uptime_s)),kv('Reset',s.reset_reason)
  ].join('');
  $('#netGrid').innerHTML=[
    kv('WLAN‑Modus',s.wifi_mode_name),kv('Band',s.wifi_band_name),
    kv('SSID (STA)',s.sta_ssid||'—'),kv('RSSI',s.rssi?s.rssi+' dBm':'—'),
    kv('STA‑IP',s.sta_ip||'—'),kv('AP‑SSID',s.ap_ssid),kv('AP‑IP',s.ap_ip),
    kv('Net‑Modus',s.net_mode_name),kv('Link',s.tcp_connected?'aktiv':'getrennt'),
    kv('Port lokal',s.local_port),kv('Remote', (s.remote_ip||'')+':'+s.remote_port),
    kv('Bridge',s.bridge?'an':'aus'),
    kv('Packetizer',s.packet_timeout_ms+' ms / '+s.packet_size+' B'),
    kv('RS485 RX/TX',s.rs485_rx+' / '+s.rs485_tx),
    kv('Net RX/TX', (s.net_rx||0)+' / '+(s.net_tx||0)),
    kv('Richtung', 'N→S:'+(s.rs485_tx_allowed===false?'aus':'an')+' · S→N:'+(s.rs485_rx_allowed===false?'aus':'an')),
    kv('Discovery UDP', s.discovery_udp_port||8880),
    kv('AT-Session', s.config_session?'aktiv':'aus')
  ].join('');
  $('#foot').textContent=`Heap frei: ${s.free_heap} · Drops net ${s.net_tx_drops}/${s.net_rx_drops}`;
  return s;
}
async function loadConfig(){
  const c=await api('/api/config');
  $('#wifi_mode').value=String(c.wifi_mode===2?1:c.wifi_mode);
  $('#wifi_band').value=String(c.wifi_band);
  $('#ssid').value=c.wifi_ssid||'';
  $('#pass').value='';
  $('#pass').placeholder=c.has_password?'gespeichert — neu eingeben zum Ändern':'Passwort';
  $('#dhcp').checked=!!c.wifi_dhcp;
  $('#ip').value=c.wifi_ip||'';$('#mask').value=c.wifi_mask||'';
  $('#gw').value=c.wifi_gw||'';$('#dns').value=c.wifi_dns||'';
  $('#staticBox').hidden=$('#dhcp').checked;
  $('#net_mode').value=String(c.net_mode);
  $('#local_port').value=c.local_port;
  $('#remote_ip').value=c.remote_ip||'';
  $('#remote_host').value=c.remote_host||'';
  $('#remote_port').value=c.remote_port;
  $('#reconnect_ms').value=c.reconnect_ms;
  $('#tcp_keepalive').checked=!!c.tcp_keepalive;
  $('#bus_address').value=c.bus_address;
  $('#packet_timeout_ms').value=c.packet_timeout_ms;
  $('#packet_size').value=c.packet_size;
  $('#bridge_enabled').checked=!!c.bridge_enabled;
  $('#rs485_tx_allowed').checked=!!c.rs485_tx_allowed;
  $('#rs485_rx_allowed').checked=!!c.rs485_rx_allowed;
}
async function pollWifiScan(deadlineMs){
  while(Date.now()<deadlineMs){
    await new Promise(r=>setTimeout(r,700));
    try{
      const j=await api('/api/wifi/scan/status');
      if(j.status==='scanning'||j.status==='idle') continue;
      if(j.status==='error') throw new Error(j.error||'Scan fehlgeschlagen');
      if(j.status==='done') return j;
    }catch(e){
      const msg=String(e);
      if(msg.includes('Failed to fetch')||msg.includes('NetworkError')||msg.includes('Load failed')){
        continue;
      }
      throw e;
    }
  }
  throw new Error('Scan-Timeout (45 s)');
}
async function doScan(){
  const band=Number($('#wifi_band').value);
  const btn=$('#btnScan');btn.disabled=true;btn.textContent='Scanne…';
  const infra=$('#wifi_mode').value==='1';
  $('#msg').textContent=infra
    ? (band===1?'2,4-GHz-Scan (Infrastruktur, kann bis 30 s dauern)…':(band===2?'5-GHz-Scan (Infrastruktur, kann bis 30 s dauern)…':'Auto-Scan (Infrastruktur)…'))
    : (band===1?'2,4-GHz-Scan…':(band===2?'5-GHz-Scan…':'Auto-Scan…'));
  $('#msg').className='msg';
  try{
    await api('/api/wifi/scan/start?band='+band);
    const j=await pollWifiScan(Date.now()+45000);
    const rows=j.networks||[];
    if(!rows.length){
      $('#scanBody').innerHTML='<tr><td colspan="4" style="color:var(--muted)">Keine Netze gefunden</td></tr>';
    }else{
      $('#scanBody').innerHTML=rows.map(n=>{
        const bandTag=n.band==='5G'?'<span class="tag g5">5 GHz</span>':'<span class="tag">2.4 GHz</span>';
        const ssid=n.ssid||'(versteckt)';
        return `<tr data-ssid="${ssid.replace(/"/g,'&quot;')}" data-band="${n.band_code}"><td>${ssid}</td><td>${n.rssi}</td><td>${bandTag}</td><td>${n.channel}</td></tr>`;
      }).join('');
      $$('#scanBody tr').forEach(tr=>tr.onclick=()=>{
        const ssid=tr.getAttribute('data-ssid');
        if(ssid&&ssid!=='(versteckt)') $('#ssid').value=ssid;
        const bc=tr.getAttribute('data-band');
        if(bc==='2') $('#wifi_band').value='2';
        else if(bc==='1') $('#wifi_band').value='1';
        $('#wifi_mode').value='1';
      });
    }
    let note=rows.length+' Netze gefunden';
    $('#msg').textContent=note;$('#msg').className='msg ok';
  }catch(e){
    $('#msg').textContent=String(e);
    $('#msg').className='msg err';
  }
  finally{btn.disabled=false;btn.textContent='Netze scannen'}
}
function wifiBody(extra){
  return Object.assign({
    wifi_mode:Number($('#wifi_mode').value),
    wifi_band:Number($('#wifi_band').value),
    wifi_ssid:$('#ssid').value,
    wifi_pass:$('#pass').value,
    wifi_dhcp:$('#dhcp').checked,
    wifi_ip:$('#ip').value,wifi_mask:$('#mask').value,
    wifi_gw:$('#gw').value,wifi_dns:$('#dns').value
  },extra||{});
}
function isTransientNetErr(e){
  const msg=String(e);
  return msg.includes('Failed to fetch')||msg.includes('NetworkError')||msg.includes('Load failed');
}
async function waitForDevice(deadlineMs){
  while(Date.now()<deadlineMs){
    await new Promise(r=>setTimeout(r,1200));
    try{
      return await api('/api/status');
    }catch(e){
      if(!isTransientNetErr(e)) throw e;
    }
  }
  throw new Error('Gerät nicht erreichbar — Router-IP prüfen oder Seite neu laden');
}
async function applyWifiSave(){
  const mode=Number($('#wifi_mode').value);
  const btn=$('#btnApply');btn.disabled=true;
  $('#msg').className='msg';

  if(mode===0){
    let band=Number($('#wifi_band').value);
    if(band===0) band=1;
    $('#wifi_band').value=String(band);
    $('#msg').textContent='SoftAP wird gespeichert…';
    try{
      await api('/api/config/wifi',{method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify(wifiBody({wifi_mode:0,wifi_band:band,connect_sta:false}))});
      $('#msg').textContent='SoftAP aktiv ('+(band===2?'5 GHz':'2,4 GHz')+') — http://192.168.4.1/';
      $('#msg').className='msg ok';
      loadStatus().then(loadConfig).catch(()=>{});
    }catch(e){
      if(isTransientNetErr(e)){
        $('#msg').textContent='SoftAP aktiv — http://192.168.4.1/ (Seite ggf. neu öffnen)';
        $('#msg').className='msg ok';
      }else{
        $('#msg').textContent=String(e);$('#msg').className='msg err';
      }
    }finally{btn.disabled=false}
    return;
  }

  if(!$('#ssid').value.trim()){
    $('#msg').textContent='SSID für Router-Verbindung erforderlich';
    $('#msg').className='msg err';
    btn.disabled=false;
    return;
  }
  if(!$('#dhcp').checked){
    if(!$('#ip').value.trim()||!$('#mask').value.trim()||!$('#gw').value.trim()){
      $('#msg').textContent='Bei fester IP: IP, Netzmaske und Gateway angeben';
      $('#msg').className='msg err';
      btn.disabled=false;
      return;
    }
  }
  let prior=null;
  try{ prior=await api('/api/status'); }catch(_){}
  const alreadySta=!!(prior&&prior.sta_ip);
  const useStatic=!$('#dhcp').checked;
  $('#msg').textContent=alreadySta
    ? (useStatic
      ? 'Feste IP wird gespeichert — Verbindung kurz unterbrochen…'
      : 'Band/Konfiguration wird gespeichert — Verbindung kurz unterbrochen…')
    : (useStatic
      ? 'Verbinde mit Router (feste IP)…'
      : 'Verbinde mit Router — SoftAP wird ausgeschaltet…');
  const body=wifiBody({connect_sta:true,wifi_mode:1});
  const finishOk=async()=>{
    if(alreadySta){
      $('#msg').textContent='Gespeichert — warte auf erneute Verbindung…';
      const st=await waitForDevice(Date.now()+45000);
      if(useStatic){
        $('#msg').textContent='Feste IP aktiv — '+($('#ip').value||st.sta_ip||'?');
      }else{
        $('#msg').textContent='Verbindung wiederhergestellt — IP: '+(st.sta_ip||'?');
      }
    }else{
      $('#msg').textContent=useStatic
        ? 'Gespeichert — verbinde mit fester IP '+$('#ip').value+'…'
        : 'Gespeichert — verbinde mit Router (SoftAP aus). STA-IP erscheint im Status.';
      setTimeout(loadStatus,1500);setTimeout(loadStatus,4000);setTimeout(loadStatus,12000);
    }
    $('#msg').className='msg ok';
    loadStatus().then(loadConfig).catch(()=>{});
  };
  try{
    await api('/api/config/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    await finishOk();
  }catch(e){
    if(alreadySta&&isTransientNetErr(e)){
      try{
        await finishOk();
      }catch(e2){
        $('#msg').textContent=String(e2);$('#msg').className='msg err';
      }
    }else{
      $('#msg').textContent=String(e);$('#msg').className='msg err';
    }
  }
  finally{btn.disabled=false}
}
async function applyRs485(){
  const btn=$('#btnRs485Save');btn.disabled=true;
  $('#msgRs485').textContent='Speichern…';$('#msgRs485').className='msg';
  const body={
    net_mode:Number($('#net_mode').value),
    local_port:Number($('#local_port').value),
    remote_ip:$('#remote_ip').value,
    remote_host:$('#remote_host').value,
    remote_port:Number($('#remote_port').value),
    reconnect_ms:Number($('#reconnect_ms').value),
    tcp_nodelay:true,
    tcp_keepalive:$('#tcp_keepalive').checked,
    packet_timeout_ms:Number($('#packet_timeout_ms').value),
    packet_size:Number($('#packet_size').value),
    delimiter:0,
    delimiter_custom:0,
    echo_suppress:false,
    bridge_enabled:$('#bridge_enabled').checked,
    rs485_tx_allowed:$('#rs485_tx_allowed').checked,
    rs485_rx_allowed:$('#rs485_rx_allowed').checked,
    bus_address:Number($('#bus_address').value)
  };
  try{
    await api('/api/config/rs485',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    await api('/api/save',{method:'POST'});
    $('#msgRs485').textContent='Gespeichert — Bridge übernimmt die neuen Einstellungen';
    $('#msgRs485').className='msg ok';
    setTimeout(loadStatus,800);
  }catch(e){$('#msgRs485').textContent=String(e);$('#msgRs485').className='msg err'}
  finally{btn.disabled=false}
}
function fmtBytes(n){
  if(n>=1048576) return (n/1048576).toFixed(2)+' MB';
  if(n>=1024) return (n/1024).toFixed(1)+' kB';
  return n+' B';
}
async function loadUpdateInfo(){
  try{
    const j=await api('/api/update/info');
    $('#updateGrid').innerHTML=
      kv('Firmware',j.fw)+kv('Hardware',j.hw)+
      kv('Laufende Partition',j.running)+
      kv('OTA‑Ziel max.',fmtBytes(j.max_bytes))+
      kv('OTA bereit',j.ota_ready?'ja':'nein');
  }catch(e){
    $('#updateGrid').innerHTML=kv('Fehler',String(e));
  }
}
async function doFirmwareUpload(){
  const file=$('#fwFile').files[0];
  if(!file){
    $('#msgUpdate').textContent='Bitte eine .bin‑Datei wählen';
    $('#msgUpdate').className='msg err';
    return;
  }
  if(!file.name.toLowerCase().endsWith('.bin')){
    $('#msgUpdate').textContent='Nur .bin‑Dateien erlaubt';
    $('#msgUpdate').className='msg err';
    return;
  }
  const btn=$('#btnFwUpload');btn.disabled=true;
  $('#msgUpdate').textContent='Upload '+file.name+' ('+fmtBytes(file.size)+')…';
  $('#msgUpdate').className='msg';
  try{
    const fd=new FormData();
    fd.append('firmware',file);
    const r=await fetch('/api/update',{method:'POST',credentials:'same-origin',body:fd});
    const t=await r.text();
    if(!r.ok) throw new Error(t||('HTTP '+r.status));
    $('#msgUpdate').textContent='Update OK — Neustart…';
    $('#msgUpdate').className='msg ok';
    setTimeout(()=>location.reload(),8000);
  }catch(e){
    $('#msgUpdate').textContent=String(e);
    $('#msgUpdate').className='msg err';
  }finally{btn.disabled=false}
}
$$('nav button').forEach(b=>b.onclick=()=>{
  $$('nav button').forEach(x=>x.classList.remove('active'));b.classList.add('active');
  const t=b.dataset.tab;
  $('#tab-status').hidden=t!=='status';
  $('#tab-wifi').hidden=t!=='wifi';
  $('#tab-rs485').hidden=t!=='rs485';
  $('#tab-security').hidden=t!=='security';
  $('#tab-update').hidden=t!=='update';
  $('#tab-reset').hidden=t!=='reset';
  if(t==='update') loadUpdateInfo();
});
async function doReboot(){
  if(!confirm('Modul wirklich neu starten?')) return;
  const btn=$('#btnReboot');btn.disabled=true;
  $('#msgReset').textContent='Neustart…';$('#msgReset').className='msg';
  try{
    await api('/api/reboot',{method:'POST'});
    $('#msgReset').textContent='Neustart — warte auf Modul…';
    await waitForDevice(Date.now()+45000);
    $('#msgReset').textContent='Modul wieder online.';$('#msgReset').className='msg ok';
    location.reload();
  }catch(e){
    if(isTransientNetErr(e)){
      try{
        await waitForDevice(Date.now()+45000);
        $('#msgReset').textContent='Modul wieder online.';$('#msgReset').className='msg ok';
        location.reload();
        return;
      }catch(e2){e=e2}
    }
    $('#msgReset').textContent=String(e);$('#msgReset').className='msg err';
  }finally{btn.disabled=false}
}
async function doFactoryReset(){
  if(!confirm('Werkseinstellung wirklich ausführen?\n\nAlle Einstellungen werden gelöscht (WLAN, TCP, Passwort). Danach SoftAP 2,4 GHz — http://192.168.4.1/')) return;
  const btn=$('#btnFactoryReset');btn.disabled=true;
  $('#msgReset').textContent='Werkseinstellung — Modul startet neu…';$('#msgReset').className='msg';
  try{
    await api('/api/factory-reset',{method:'POST'});
  }catch(e){
    if(!isTransientNetErr(e)){
      $('#msgReset').textContent=String(e);$('#msgReset').className='msg err';
      btn.disabled=false;
      return;
    }
  }
  $('#msgReset').textContent='Werkseinstellung — bitte mit SoftAP ROTOR-… verbinden: http://192.168.4.1/';
  $('#msgReset').className='msg ok';
  try{
    await waitForDevice(Date.now()+30000);
    location.reload();
  }catch(_){}
  finally{btn.disabled=false}
}
$('#dhcp').onchange=()=>{$('#staticBox').hidden=$('#dhcp').checked};
$('#btnScan').onclick=doScan;
$('#btnApply').onclick=applyWifiSave;
$('#btnRs485Save').onclick=applyRs485;
$('#btnWebPass').onclick=async()=>{
  const btn=$('#btnWebPass');btn.disabled=true;
  $('#msgWebPass').textContent='';$('#msgWebPass').className='msg';
  const cur=$('#web_pass_cur').value;
  const nw=$('#web_pass_new').value;
  const rep=$('#web_pass_rep').value;
  try{
    if(!cur||!nw) throw new Error('Bitte alle Felder ausfüllen');
    if(nw!==rep) throw new Error('Neues Passwort stimmt nicht überein');
    if(nw.length>64) throw new Error('Passwort zu lang (max. 64)');
    await api('/api/config/webpass',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({current:cur,next:nw})});
    $('#msgWebPass').textContent='Passwort gespeichert — bei der nächsten Anmeldung neues Passwort nutzen';
    $('#msgWebPass').className='msg ok';
    $('#web_pass_cur').value='';$('#web_pass_new').value='';$('#web_pass_rep').value='';
  }catch(e){$('#msgWebPass').textContent=String(e);$('#msgWebPass').className='msg err'}
  finally{btn.disabled=false}
};
$('#btnFwUpload').onclick=doFirmwareUpload;
$('#fwFile').onchange=()=>{
  const f=$('#fwFile').files[0];
  const el=$('#fwFileName');
  if(f){
    el.textContent=f.name+' ('+fmtBytes(f.size)+')';
    el.classList.add('on');
  }else{
    el.textContent='Wählen Sie eine Firmwaredatei (.bin) aus';
    el.classList.remove('on');
  }
};
$('#btnReboot').onclick=doReboot;
$('#btnFactoryReset').onclick=doFactoryReset;
loadStatus().then(loadConfig).catch(e=>{setLink(false,'Fehler');$('#msg').textContent=String(e)});
setInterval(()=>{if(!$('#tab-status').hidden) loadStatus().catch(()=>{});},5000);
</script>
</body>
</html>)HTML";
