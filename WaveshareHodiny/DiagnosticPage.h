#pragma once

#include <Arduino.h>

const char DIAGNOSTIC_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="cs">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Waveshare Hodiny – diagnostika</title>
  <style>
    :root{color-scheme:dark;--bg:#0d1215;--panel:#171e22;--line:#344047;--text:#f3f6f7;--muted:#98a5ac;--cyan:#4ccbec;--green:#65c744;--red:#ff6262;--radius:12px}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}main{width:min(100% - 32px,980px);margin:auto;padding:48px 0 64px}header{display:flex;align-items:flex-start;justify-content:space-between;gap:24px;margin-bottom:32px}h1{font-size:32px;margin:0 0 7px}.subtitle{color:var(--muted)}.state{display:inline-flex;align-items:center;gap:8px;color:var(--green);font-weight:700}.state::before{content:"";width:9px;height:9px;border-radius:50%;background:currentColor}.grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:14px}.card{min-width:0;padding:20px;border:1px solid var(--line);border-radius:var(--radius);background:var(--panel)}.card.wide{grid-column:span 3}.card h2{margin:0 0 16px;font-size:18px}.values{display:grid;grid-template-columns:minmax(0,1fr) auto;gap:9px 18px}.label{color:var(--muted)}.value{text-align:right;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}.ok{color:var(--green)}.error{color:var(--red)}.notice{margin-top:24px;padding:16px 18px;border:1px solid #315765;border-radius:var(--radius);background:#102129;color:#b9eafa}.footer{margin-top:20px;color:var(--muted);font-size:13px}.pulse{color:var(--cyan)}@media(max-width:760px){main{padding:28px 0 44px}.grid{grid-template-columns:1fr}.card,.card.wide{grid-column:1}header{display:block}h1{font-size:28px}.state{margin-top:12px}}
  </style>
</head>
<body>
<main>
  <header><div><h1>Waveshare Hodiny</h1><div class="subtitle">Diagnostika zařízení pouze pro čtení</div></div><div class="state" id="connectionState">Zařízení je připojeno</div></header>
  <div class="grid">
    <section class="card"><h2>Firmware</h2><div class="values"><span class="label">Verze</span><span class="value" id="firmwareVersion">—</span><span class="label">Stav aktualizace</span><span class="value" id="firmwareState">—</span><span class="label">Uptime</span><span class="value" id="uptime">—</span></div></section>
    <section class="card"><h2>Hardware</h2><div class="values"><span class="label">Čip</span><span class="value" id="chip">—</span><span class="label">CPU</span><span class="value" id="cpu">—</span><span class="label">Flash</span><span class="value" id="flash">—</span><span class="label">PSRAM</span><span class="value" id="psram">—</span></div></section>
    <section class="card"><h2>Síť</h2><div class="values"><span class="label">Wi‑Fi</span><span class="value" id="wifi">—</span><span class="label">RSSI</span><span class="value" id="rssi">—</span><span class="label">IP adresa</span><span class="value" id="ip">—</span></div></section>
    <section class="card"><h2>Paměť</h2><div class="values"><span class="label">Interní volná</span><span class="value" id="internalFree">—</span><span class="label">Největší blok</span><span class="value" id="internalLargest">—</span><span class="label">PSRAM volná</span><span class="value" id="psramFree">—</span><span class="label">Největší blok</span><span class="value" id="psramLargest">—</span></div></section>
    <section class="card wide"><h2>Home Assistant a DEN/NOC</h2><div class="values"><span class="label">Poslední výsledek</span><span class="value" id="haResult">—</span><span class="label">Úspěšné / chybné požadavky</span><span class="value" id="haCounts">—</span><span class="label">Poslední dokončení</span><span class="value" id="haFinished">—</span><span class="label">Detail</span><span class="value" id="haDetail">—</span><span class="label">Stav SUN</span><span class="value" id="sunState">—</span><span class="label">Stav světla nebo skupiny</span><span class="value" id="lightState">—</span><span class="label">Aktuální režim displeje</span><span class="value" id="displayMode">—</span></div></section>
  </div>
  <div class="notice">Konfigurace je zamčená. Aktivovat ji můžeš přímo na displeji hodin podle zvoleného režimu webového rozhraní.</div>
  <div class="footer">Údaje se automaticky obnovují každých 5 sekund. <span class="pulse" id="lastRefresh">Čekám na první načtení…</span></div>
</main>
<script>
const $=id=>document.getElementById(id);
const bytes=value=>{const number=Number(value)||0;if(number>=1048576)return(number/1048576).toFixed(1)+" MB";if(number>=1024)return Math.round(number/1024)+" kB";return number+" B"};
const duration=value=>{let seconds=Math.floor((Number(value)||0)/1000);const days=Math.floor(seconds/86400);seconds%=86400;const hours=Math.floor(seconds/3600);seconds%=3600;const minutes=Math.floor(seconds/60);return(days?days+" d ":"")+(hours?hours+" h ":"")+minutes+" min"};
const ago=(uptime,finished)=>{if(!finished)return"Dosud neproběhlo";const seconds=Math.max(0,Math.floor((uptime-finished)/1000));return seconds<60?seconds+" s zpět":Math.floor(seconds/60)+" min zpět"};
function render(data){$("firmwareVersion").textContent=data.firmwareVersion||"—";$("firmwareState").textContent=data.firmwareMessage||data.firmwareState||"—";$("uptime").textContent=duration(data.uptimeMs);$("chip").textContent=(data.chipModel||"—")+(data.chipRevision!=null?" rev. "+data.chipRevision:"");$("cpu").textContent=(data.cpuFrequencyMHz||0)+" MHz";$("flash").textContent=bytes(data.flashSize);$("psram").textContent=bytes(data.psramSize);$("wifi").textContent=data.wifiConnected?"Připojeno":"Odpojeno";$("wifi").className="value "+(data.wifiConnected?"ok":"error");$("rssi").textContent=data.wifiConnected?data.wifiRssi+" dBm":"—";$("ip").textContent=data.ipAddress||"—";const memory=data.currentMemory||{};$("internalFree").textContent=bytes(memory.internalFree);$("internalLargest").textContent=bytes(memory.internalLargest);$("psramFree").textContent=bytes(memory.psramFree);$("psramLargest").textContent=bytes(memory.psramLargest);const ha=data.homeAssistantRuntime||{};$("haResult").textContent=ha.lastResult===200?"V pořádku":ha.lastResult?"Chyba "+ha.lastResult:"Dosud neproběhlo";$("haResult").className="value "+(ha.lastResult===200?"ok":ha.lastResult?"error":"");$("haCounts").textContent=(ha.successes||0)+" / "+(ha.failures||0);$("haFinished").textContent=ago(data.uptimeMs,ha.lastFinishedAt);$("haDetail").textContent=ha.detail||"—";$("lastRefresh").textContent="Aktualizováno "+new Date().toLocaleTimeString("cs-CZ");}
function renderDayNight(data){$("firmwareState").textContent=data.firmwareState==="idle"?"Připraveno":data.firmwareState||"—";$("sunState").textContent=data.sunStateAvailable?(data.sunIsDay?"Nad horizontem / den":"Pod horizontem / noc"):"Není načten";$("lightState").textContent=data.dayNightLightStateAvailable?(data.dayNightLightOn?"ON":"OFF"):"Není načteno";$("displayMode").textContent=data.nightMode?"Noční":"Denní";$("displayMode").className="value "+(data.nightMode?"":"ok")}
async function refresh(){try{const response=await fetch("/api/status",{cache:"no-store"});if(!response.ok)throw new Error("HTTP "+response.status);const data=await response.json();render(data);renderDayNight(data);$("connectionState").textContent="Zařízení je připojeno";$("connectionState").className="state"}catch(error){$("connectionState").textContent="Zařízení neodpovídá";$("connectionState").className="state error";$("lastRefresh").textContent=error.message}}
refresh();setInterval(refresh,5000);
</script>
</body>
</html>)HTML";
