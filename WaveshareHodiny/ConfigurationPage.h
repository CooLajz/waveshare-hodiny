#pragma once

#include <pgmspace.h>

const char CONFIGURATION_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="cs">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <meta name="color-scheme" content="dark">
  <title>Waveshare Hodiny</title>
  <style>
    :root{--bg:#101418;--surface:#171c20;--field:#1c2227;--line:#3b444b;--text:#f3f6f8;--muted:#9da7ae;--cyan:#4ccbec;--amber:#ffb843;--green:#65c744;--error:#ff6262;--radius:10px;--space:24px}
    *{box-sizing:border-box}
    html{background:var(--bg);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;color:var(--text);font-size:16px}
    body{margin:0;background:var(--bg)}
    main{width:min(940px,calc(100% - 40px));margin:0 auto;padding:48px 0 64px}
    header{margin-bottom:34px}
    h1{font-size:34px;line-height:1.15;margin:0 0 10px;letter-spacing:-.025em}
    .device-status{display:flex;align-items:center;gap:9px;color:var(--green);font-weight:650}
    .device-status::before{content:"";width:8px;height:8px;border-radius:50%;background:currentColor}
    section{padding:0 0 28px;margin:0 0 28px;border-bottom:1px solid var(--line)}
    h2{font-size:22px;line-height:1.25;margin:0 0 20px;letter-spacing:-.01em}
    .section-a h2,.section-b h2,.section-left h2,.section-right h2{padding-left:13px;border-left:3px solid var(--cyan)}
    .section-b h2{border-color:var(--amber)}
    .section-right h2{border-color:var(--amber)}
    .grid{display:grid;grid-template-columns:repeat(12,minmax(0,1fr));gap:18px 20px}
    .span-12{grid-column:span 12}.span-9{grid-column:span 9}.span-8{grid-column:span 8}.span-7{grid-column:span 7}.span-6{grid-column:span 6}.span-5{grid-column:span 5}.span-4{grid-column:span 4}.span-3{grid-column:span 3}.span-2{grid-column:span 2}.span-1{grid-column:span 1}
    label,.field-label{display:block;font-size:14px;font-weight:650;margin:0 0 8px;color:var(--text)}
    input,select,button{font:inherit}
    input,select{width:100%;height:46px;border:1px solid #505a61;border-radius:var(--radius);background:var(--field);color:var(--text);padding:0 13px;outline:none;transition:border-color .16s,box-shadow .16s}
    input:focus,select:focus{border-color:var(--cyan);box-shadow:0 0 0 3px rgba(76,203,236,.15)}
    input:disabled,select:disabled{opacity:.58;cursor:not-allowed}
    input::placeholder{color:#778189}
    .hint{display:block;color:var(--muted);font-size:13px;line-height:1.4;margin-top:7px}.hint a{color:var(--cyan);text-underline-offset:2px}
    .token-state{color:var(--green)}
    .actions-inline{display:flex;align-items:end;height:100%}
    button{min-height:46px;border-radius:var(--radius);border:1px solid var(--cyan);background:transparent;color:var(--cyan);font-weight:700;padding:0 20px;cursor:pointer;transition:background .16s,color .16s,opacity .16s}
    button:hover{background:rgba(76,203,236,.1)}button:disabled{opacity:.5;cursor:wait}
    .primary{background:var(--cyan);color:#092028;border-color:var(--cyan);min-width:190px}.primary:hover{background:#71d7f0}
    .range-row{display:grid;grid-template-columns:1fr 68px;gap:14px;align-items:center}
    input[type=range]{height:26px;padding:0;border:0;background:transparent;accent-color:var(--cyan);box-shadow:none}
    input[type=color]{padding:4px;cursor:pointer}
    .color-scale{grid-column:span 12;display:grid;gap:12px}.color-point{display:grid;grid-template-columns:minmax(0,1fr) 110px 46px;gap:12px;align-items:end}.color-point:first-child{grid-template-columns:minmax(0,1fr) 110px}.color-point label{margin:0}.color-point-remove{min-width:46px;width:46px;padding:0;border-color:var(--error);color:var(--error);font-size:21px}.color-point-remove:hover{background:rgba(255,98,98,.1)}.color-scale-actions{grid-column:span 12;display:flex;align-items:center;gap:14px}.color-scale-add{min-height:40px}.color-scale-add:disabled{cursor:not-allowed}
    .night input[type=range]{accent-color:var(--amber)}
    output{height:42px;display:grid;place-items:center;border:1px solid #505a61;border-radius:8px;background:var(--field);font-variant-numeric:tabular-nums}
    .switch-row{display:flex;align-items:center;justify-content:space-between;gap:15px;min-height:46px}
    .switch{position:relative;width:54px;height:30px;flex:0 0 auto}.switch input{position:absolute;opacity:0;width:1px;height:1px}.switch span{display:block;width:54px;height:30px;border-radius:20px;background:#465058;transition:.16s}.switch span::after{content:"";position:absolute;top:4px;left:4px;width:22px;height:22px;border-radius:50%;background:white;transition:.16s}.switch input:checked+span{background:var(--cyan)}.switch input:checked+span::after{transform:translateX(24px)}.switch input:focus-visible+span{box-shadow:0 0 0 3px rgba(76,203,236,.22)}
    .icon-choices{display:grid;grid-template-columns:repeat(6,minmax(0,1fr));gap:10px}.icon-choice{position:relative;display:grid;justify-items:center;gap:7px;margin:0;padding:11px 6px;border:1px solid #505a61;border-radius:var(--radius);background:var(--field);cursor:pointer;color:var(--muted);font-size:12px;text-align:center}.icon-choice input{position:absolute;opacity:0;width:1px;height:1px}.icon-choice svg{width:25px;height:25px;fill:currentColor}.icon-choice:has(input:checked){border-color:var(--cyan);box-shadow:0 0 0 2px rgba(76,203,236,.15);color:var(--cyan)}.section-right .icon-choice:has(input:checked){border-color:var(--amber);box-shadow:0 0 0 2px rgba(255,184,67,.15);color:var(--amber)}.icon-choice:has(input:focus-visible){outline:3px solid rgba(76,203,236,.2)}
    .danger{border-color:var(--error);color:var(--error)}.danger:hover{background:rgba(255,98,98,.1)}.device-tools{display:flex;align-items:center;justify-content:space-between;gap:24px}.device-tools h2{margin-bottom:6px}.device-tools .hint{margin:0}.device-actions{display:flex;gap:10px;flex-wrap:wrap;justify-content:flex-end}
    .firmware-value{font-size:18px;font-weight:750;font-variant-numeric:tabular-nums}.firmware-actions{display:flex;align-items:end;gap:10px;flex-wrap:wrap}.firmware-progress{margin-top:14px}
    .footer{display:flex;align-items:center;gap:20px}.feedback{font-size:14px;color:var(--muted)}.feedback.success{color:var(--green)}.feedback.error{color:var(--error)}
    .hidden{display:none!important}
    @media(max-width:760px){main{width:min(100% - 32px,620px);padding:28px 0 44px}h1{font-size:29px}header{margin-bottom:28px}section{padding-bottom:25px;margin-bottom:25px}.grid{grid-template-columns:1fr;gap:17px}.grid>*{grid-column:1}.actions-inline button{width:100%}.footer{align-items:stretch;flex-direction:column}.primary{width:100%}.feedback{text-align:center}.metric-grid{grid-template-columns:1fr 1fr}.metric-grid>*{grid-column:span 1}.metric-grid .wide{grid-column:span 2}}
    @media(max-width:760px){.device-tools{align-items:stretch;flex-direction:column}.device-actions{display:grid;grid-template-columns:1fr 1fr}.device-actions button{padding:0 12px}.device-actions .danger{grid-column:span 2}.icon-choices{grid-template-columns:repeat(3,minmax(0,1fr))}}
    @media(max-width:430px){main{width:calc(100% - 28px)}.metric-grid{grid-template-columns:1fr}.metric-grid>* , .metric-grid .wide{grid-column:1}.color-point,.color-point:first-child{grid-template-columns:minmax(0,1fr) 82px 46px}.color-point:first-child::after{content:""}.color-scale-actions{align-items:stretch;flex-direction:column}.color-scale-add{width:100%}.icon-choices{grid-template-columns:repeat(2,minmax(0,1fr))}.switch-row{border:1px solid #505a61;border-radius:var(--radius);padding:0 12px}.switch-row .field-label{margin:0}}
  </style>
</head>
<body>
<main>
  <header><h1>Waveshare Hodiny</h1><div class="device-status" id="deviceStatus">Zařízení je připojeno</div></header>
  <form id="configForm">
    <section>
      <h2>Home Assistant</h2>
      <div class="grid">
        <div class="span-4"><label for="haUrl">Adresa Home Assistantu</label><input id="haUrl" name="haUrl" type="url" placeholder="http://homeassistant.local:8123" autocomplete="url"></div>
        <div class="span-5"><label for="haToken">Long-lived access token</label><input id="haToken" name="haToken" type="password" placeholder="Zadej nový token" autocomplete="new-password"><span class="hint token-state" id="tokenState">Token zatím není uložen</span></div>
        <div class="span-3 actions-inline"><button type="button" id="testConnection">Otestovat připojení</button></div>
      </div>
      <p class="hint" id="haFeedback">Ověření zkontroluje adresu a token. ID entit zadej ručně; token se po uložení už nezobrazí.</p>
    </section>
    <section>
      <h2>Globální entity</h2>
      <div class="grid">
        <div class="span-12"><label for="weatherEntity">Entita počasí</label><input id="weatherEntity" name="weatherEntity" placeholder="weather.domov"></div>
        <div class="span-6"><div class="switch-row"><span class="field-label">Animované ikony počasí</span><label class="switch" aria-label="Používat animované ikony počasí z Firmware Hubu"><input id="animatedWeatherIcons" name="animatedWeatherIcons" type="checkbox" checked><span></span></label></div><span class="hint">Vypnutím se použijí statické ikony uložené ve firmware.</span></div>
        <div class="span-6"><label for="weatherIconStyle">Styl animovaných ikon</label><select id="weatherIconStyle" name="weatherIconStyle"><option value="monochrome">Monochrome</option><option value="flat">Flat</option><option value="line">Line</option></select><span class="hint">V nočním režimu se vždy použije Monochrome. <a id="meteoconsLink" href="https://meteocons.com/icons/?style=monochrome" target="_blank" rel="noopener noreferrer">Prohlédnout tento styl</a></span></div>
      </div>
    </section>
    <section class="section-left">
      <h2>Místnost vlevo</h2>
      <div class="grid">
        <div class="span-4"><label for="leftName">Název</label><input id="leftName" name="leftName" maxlength="31" placeholder="VENKU"></div>
        <div class="span-4"><label for="leftTemperatureEntity">Entita teploty</label><input id="leftTemperatureEntity" name="leftTemperatureEntity" placeholder="sensor.teplota"></div>
        <div class="span-2"><label for="leftColor">Barva místnosti</label><input id="leftColor" name="leftColor" type="color" value="#4ccbec"></div>
        <div class="span-2"><label for="leftWeatherIconColor">Barva ikony</label><input id="leftWeatherIconColor" name="leftWeatherIconColor" type="color" value="#ffffff"></div>
        <div class="span-12"><span class="field-label">Ikona</span><div class="icon-choices">
          <label class="icon-choice"><input type="radio" name="leftIcon" value="weather" checked><svg viewBox="0 0 24 24"><path d="M6.76 4.84 5.35 3.43 3.93 4.84l1.42 1.42zM1 10h3V8H1zm10-9H9v3h2zm5.66 2.42-1.41 1.42 1.41 1.41 1.42-1.41zM17.5 12a5.5 5.5 0 0 0-10.83-1.35A4.5 4.5 0 0 0 7.5 19h9a3.5 3.5 0 0 0 1-7z"/></svg><span>Počasí</span></label>
          <label class="icon-choice"><input type="radio" name="leftIcon" value="home"><svg viewBox="0 0 24 24"><path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/></svg><span>Domeček</span></label>
          <label class="icon-choice"><input type="radio" name="leftIcon" value="living-room"><svg viewBox="0 0 24 24"><path d="M21 10c-1.1 0-2 .9-2 2v3H5v-3c0-1.1-.9-2-2-2s-2 .9-2 2v5c0 1.1.9 2 2 2h1v2h2v-2h12v2h2v-2h1c1.1 0 2-.9 2-2v-5c0-1.1-.9-2-2-2zm-4 3v-3c0-1.1-.9-2-2-2H9c-1.1 0-2 .9-2 2v3z"/></svg><span>Sedačka</span></label>
          <label class="icon-choice"><input type="radio" name="leftIcon" value="bedroom"><svg viewBox="0 0 24 24"><path d="M7 13c1.66 0 3-1.34 3-3S8.66 7 7 7s-3 1.34-3 3 1.34 3 3 3zm12-6h-8v7H3V5H1v15h2v-3h18v3h2v-9c0-2.21-1.79-4-4-4z"/></svg><span>Postel</span></label>
          <label class="icon-choice"><input type="radio" name="leftIcon" value="kitchen"><svg viewBox="0 0 24 24"><path d="M11 9H9V2H7v7H5V2H3v7c0 2.12 1.66 3.84 3.75 3.97V22h2.5v-9.03C11.34 12.84 13 11.12 13 9V2h-2zm5-3v8h2.5v8H21V2c-2.76 0-5 2.24-5 4z"/></svg><span>Kuchyně</span></label>
          <label class="icon-choice"><input type="radio" name="leftIcon" value="none"><svg viewBox="0 0 24 24"><path d="M5.27 4 4 5.27 8.73 10H4v2h6.73l8 8L20 18.73zM20 12v-2h-5.18l2 2z"/></svg><span>Bez ikony</span></label>
        </div></div>
      </div>
    </section>
    <section class="section-right">
      <h2>Místnost vpravo</h2>
      <div class="grid">
        <div class="span-4"><label for="rightName">Název</label><input id="rightName" name="rightName" maxlength="31" placeholder="MÍSTNOST"></div>
        <div class="span-4"><label for="rightTemperatureEntity">Entita teploty</label><input id="rightTemperatureEntity" name="rightTemperatureEntity" placeholder="sensor.teplota"></div>
        <div class="span-2"><label for="rightColor">Barva místnosti</label><input id="rightColor" name="rightColor" type="color" value="#ffb843"></div>
        <div class="span-2"><label for="rightWeatherIconColor">Barva ikony</label><input id="rightWeatherIconColor" name="rightWeatherIconColor" type="color" value="#ffffff"></div>
        <div class="span-12"><span class="field-label">Ikona</span><div class="icon-choices">
          <label class="icon-choice"><input type="radio" name="rightIcon" value="weather"><svg viewBox="0 0 24 24"><path d="M6.76 4.84 5.35 3.43 3.93 4.84l1.42 1.42zM1 10h3V8H1zm10-9H9v3h2zm5.66 2.42-1.41 1.42 1.41 1.41 1.42-1.41zM17.5 12a5.5 5.5 0 0 0-10.83-1.35A4.5 4.5 0 0 0 7.5 19h9a3.5 3.5 0 0 0 1-7z"/></svg><span>Počasí</span></label>
          <label class="icon-choice"><input type="radio" name="rightIcon" value="home" checked><svg viewBox="0 0 24 24"><path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/></svg><span>Domeček</span></label>
          <label class="icon-choice"><input type="radio" name="rightIcon" value="living-room"><svg viewBox="0 0 24 24"><path d="M21 10c-1.1 0-2 .9-2 2v3H5v-3c0-1.1-.9-2-2-2s-2 .9-2 2v5c0 1.1.9 2 2 2h1v2h2v-2h12v2h2v-2h1c1.1 0 2-.9 2-2v-5c0-1.1-.9-2-2-2zm-4 3v-3c0-1.1-.9-2-2-2H9c-1.1 0-2 .9-2 2v3z"/></svg><span>Sedačka</span></label>
          <label class="icon-choice"><input type="radio" name="rightIcon" value="bedroom"><svg viewBox="0 0 24 24"><path d="M7 13c1.66 0 3-1.34 3-3S8.66 7 7 7s-3 1.34-3 3 1.34 3 3 3zm12-6h-8v7H3V5H1v15h2v-3h18v3h2v-9c0-2.21-1.79-4-4-4z"/></svg><span>Postel</span></label>
          <label class="icon-choice"><input type="radio" name="rightIcon" value="kitchen"><svg viewBox="0 0 24 24"><path d="M11 9H9V2H7v7H5V2H3v7c0 2.12 1.66 3.84 3.75 3.97V22h2.5v-9.03C11.34 12.84 13 11.12 13 9V2h-2zm5-3v8h2.5v8H21V2c-2.76 0-5 2.24-5 4z"/></svg><span>Kuchyně</span></label>
          <label class="icon-choice"><input type="radio" name="rightIcon" value="none"><svg viewBox="0 0 24 24"><path d="M5.27 4 4 5.27 8.73 10H4v2h6.73l8 8L20 18.73zM20 12v-2h-5.18l2 2z"/></svg><span>Bez ikony</span></label>
        </div></div>
      </div>
    </section>
    <section class="section-a metric" data-prefix="metricA">
      <h2>Měřená hodnota A</h2>
      <div class="grid metric-grid">
        <div class="span-2"><label for="metricAMode">Režim</label><select id="metricAMode" name="metricAMode"><option value="preset">Předvolba</option><option value="custom">Vlastní</option></select></div>
        <div class="span-2"><label for="metricAPreset">Typ</label><select id="metricAPreset" name="metricAPreset" class="preset-select"></select></div>
        <div class="span-3 wide"><label for="metricAEntity">Home Assistant entita</label><input id="metricAEntity" name="metricAEntity" placeholder="sensor.hodnota"></div>
        <div class="span-2"><label for="metricAName">Vlastní název</label><input id="metricAName" name="metricAName" maxlength="23"></div>
        <div class="span-2"><label for="metricASuffix">Suffix</label><input id="metricASuffix" name="metricASuffix" maxlength="15"></div>
        <div class="span-1"><label for="metricADecimals">Desetinná místa</label><select id="metricADecimals" name="metricADecimals"><option>0</option><option>1</option><option>2</option></select></div>
        <div class="color-scale" id="metricAColorScale"></div>
        <div class="color-scale-actions"><button class="color-scale-add" type="button" data-color-add="metricAColor">+ Přidat barvu</button><span class="hint">Maximálně 10 bodů. Mezi body se barvy plynule prolínají.</span></div>
      </div>
    </section>
    <section class="section-b metric" data-prefix="metricB">
      <h2>Měřená hodnota B</h2>
      <div class="grid metric-grid">
        <div class="span-2"><label for="metricBMode">Režim</label><select id="metricBMode" name="metricBMode"><option value="preset">Předvolba</option><option value="custom">Vlastní</option></select></div>
        <div class="span-2"><label for="metricBPreset">Typ</label><select id="metricBPreset" name="metricBPreset" class="preset-select"></select></div>
        <div class="span-3 wide"><label for="metricBEntity">Home Assistant entita</label><input id="metricBEntity" name="metricBEntity" placeholder="sensor.hodnota"></div>
        <div class="span-2"><label for="metricBName">Vlastní název</label><input id="metricBName" name="metricBName" maxlength="23"></div>
        <div class="span-2"><label for="metricBSuffix">Suffix</label><input id="metricBSuffix" name="metricBSuffix" maxlength="15"></div>
        <div class="span-1"><label for="metricBDecimals">Desetinná místa</label><select id="metricBDecimals" name="metricBDecimals"><option>0</option><option>1</option><option>2</option></select></div>
        <div class="color-scale" id="metricBColorScale"></div>
        <div class="color-scale-actions"><button class="color-scale-add" type="button" data-color-add="metricBColor">+ Přidat barvu</button><span class="hint">Maximálně 10 bodů. Mezi body se barvy plynule prolínají.</span></div>
      </div>
    </section>
    <section>
      <h2>Displej</h2>
      <div class="grid">
        <div class="span-4"><label for="dayBrightness">Denní jas</label><div class="range-row"><input id="dayBrightness" name="dayBrightness" type="range" min="1" max="100"><output id="dayBrightnessValue">35 %</output></div></div>
        <div class="span-4 night"><label for="nightBrightness">Noční jas</label><div class="range-row"><input id="nightBrightness" name="nightBrightness" type="range" min="1" max="100"><output id="nightBrightnessValue">10 %</output></div></div>
        <div class="span-4"><div class="switch-row"><span class="field-label">Automaticky den/noc</span><label class="switch" aria-label="Automaticky den/noc"><input id="automaticDayNight" name="automaticDayNight" type="checkbox"><span></span></label></div><span class="hint">Podle vybrané entity slunce</span></div>
        <div class="span-12 grid hidden" id="automaticDayNightSettings">
          <div class="span-6"><label for="sunEntity">SUN entita</label><input id="sunEntity" name="sunEntity" placeholder="sun.sun"></div>
          <div class="span-3"><label for="sunriseOffsetMinutes">Offset ráno</label><select id="sunriseOffsetMinutes" name="sunriseOffsetMinutes"><option value="-60">−60 min</option><option value="-45">−45 min</option><option value="-30">−30 min</option><option value="-15">−15 min</option><option value="0">0 min</option><option value="15">+15 min</option><option value="30">+30 min</option><option value="45">+45 min</option><option value="60">+60 min</option></select><span class="hint">− dříve, + později</span></div>
          <div class="span-3"><label for="sunsetOffsetMinutes">Offset večer</label><select id="sunsetOffsetMinutes" name="sunsetOffsetMinutes"><option value="-60">−60 min</option><option value="-45">−45 min</option><option value="-30">−30 min</option><option value="-15">−15 min</option><option value="0">0 min</option><option value="15">+15 min</option><option value="30">+30 min</option><option value="45">+45 min</option><option value="60">+60 min</option></select><span class="hint">− dříve, + později</span></div>
          <div class="span-12 hint token-state" id="sunTransitionTimes">Časy přechodů zatím nejsou načtené.</div>
          <div class="span-6"><label for="dayNightLightEntity">Entita světla nebo skupiny (volitelné)</label><input id="dayNightLightEntity" name="dayNightLightEntity" placeholder="light.loznice"><span class="hint">V nočním čase stav ON dočasně použije denní režim; OFF vrátí noční režim.</span></div>
          <div class="span-6"><label for="nightVisualMode">Vzhled nočního režimu</label><select id="nightVisualMode" name="nightVisualMode"><option value="red">Červený displej</option><option value="brightness">Pouze snížit jas</option></select><span class="hint">Nemění časování ani napojení na světla, pouze vzhled displeje.</span></div>
        </div>
        <div class="span-6"><label for="timeFont">Font hodin</label><select id="timeFont" name="timeFont"><option value="barlow">Barlow Bold</option><option value="liberation">Liberation Sans Bold</option><option value="lcd">DSEG7 Modern Bold (LCD)</option><option value="doto">Doto Bold (Dot Matrix)</option></select><span class="hint">Změna se projeví po uložení bez restartu.</span></div>
        <div class="span-3"><label for="timeColor">Barva hodin</label><input id="timeColor" name="timeColor" type="color" value="#f6f6f6"></div>
        <div class="span-3"><label for="dateColor">Barva data</label><input id="dateColor" name="dateColor" type="color" value="#b5b5b5"></div>
      </div>
    </section>
    <section>
      <h2>Vteřiny</h2>
      <div class="grid">
        <div class="span-4"><div class="switch-row"><span class="field-label">Zobrazit vteřiny</span><label class="switch" aria-label="Zobrazit vteřiny"><input id="secondRingEnabled" name="secondRingEnabled" type="checkbox"><span></span></label></div><span class="hint">Lze přepnout také přímo na hodinách</span></div>
        <div class="span-4"><label for="secondEffect">Efekt</label><select id="secondEffect" name="secondEffect"><option value="dots">Klasické tečky</option><option value="line">Plynulá čára</option><option value="comet">Kometa</option></select></div>
        <div class="span-4"><label for="secondRingBackgroundDotSize">Velikost pozadí</label><select id="secondRingBackgroundDotSize" name="secondRingBackgroundDotSize"><option value="1">1 px</option><option value="2">2 px</option><option value="3">3 px</option><option value="4">4 px</option><option value="5">5 px</option><option value="6">6 px</option><option value="7">7 px</option><option value="8">8 px</option><option value="9">9 px</option><option value="10">10 px</option></select></div>
        <div class="span-4"><label for="secondDotSize">Velikost aktivní</label><select id="secondDotSize" name="secondDotSize"><option value="1">1 px</option><option value="2">2 px</option><option value="3">3 px</option><option value="4">4 px</option><option value="5">5 px</option><option value="6">6 px</option><option value="7">7 px</option><option value="8">8 px</option><option value="9">9 px</option><option value="10">10 px</option></select></div>
        <div class="span-3"><label for="secondRingBackgroundColor">Barva pozadí</label><input id="secondRingBackgroundColor" name="secondRingBackgroundColor" type="color" value="#ffffff"></div>
        <div class="span-9"><label for="secondRingBackgroundBrightness">Jas pozadí</label><div class="range-row"><input id="secondRingBackgroundBrightness" name="secondRingBackgroundBrightness" type="range" min="0" max="255"><output id="secondRingBackgroundBrightnessValue">0</output></div><span class="hint">0 = pozadí vypnuto, 255 = maximální jas</span></div>
        <div class="span-3"><label for="secondDotColor">Barva aktivní</label><input id="secondDotColor" name="secondDotColor" type="color" value="#ffffff"></div>
        <div class="span-9"><label for="secondDotBrightness">Jas aktivní</label><div class="range-row"><input id="secondDotBrightness" name="secondDotBrightness" type="range" min="0" max="255"><output id="secondDotBrightnessValue">175</output></div></div>
      </div>
    </section>
    <section>
      <h2>Firmware</h2>
      <div class="grid">
        <div class="span-3"><span class="field-label">Aktuální verze</span><div class="firmware-value" id="currentFirmwareVersion">—</div></div>
        <div class="span-3"><span class="field-label">Verze na serveru</span><div class="firmware-value" id="serverFirmwareVersion">Nezkontrolováno</div></div>
        <div class="span-3"><div class="switch-row"><span class="field-label">Automatická aktualizace</span><label class="switch" aria-label="Automatická aktualizace firmware"><input id="automaticFirmwareUpdate" name="automaticFirmwareUpdate" type="checkbox"><span></span></label></div><span class="hint">Po zapnutí denně po 4:10</span></div>
        <div class="span-3 firmware-actions"><button id="checkFirmware" type="button">Zkontrolovat</button><button class="primary hidden" id="installFirmware" type="button">Aktualizovat</button></div>
        <div class="span-4"><label for="webMode">Webový server</label><select id="webMode" name="webMode"><option value="timed">Zapnout na 10 minut</option><option value="always">Vždy zapnutý</option><option value="disabled">Vypnutý</option></select><span class="hint">Při vypnutí jej lze znovu povolit na displeji hodin.</span></div>
        <div class="span-8 wide"><label for="controlApiBase">Ovládací API pro Home Assistant</label><input id="controlApiBase" readonly><span class="hint">Secret je uložený v zařízení a není součástí exportované zálohy.</span></div>
        <div class="span-4 firmware-actions"><button id="displayOff" type="button">Vypnout podsvícení</button><button id="displayOn" type="button">Zapnout podsvícení</button></div>
      </div>
      <p class="hint firmware-progress" id="firmwareFeedback">Načítám stav firmware…</p><p class="hint" id="controlFeedback"></p>
    </section>
    <section class="device-tools"><div><h2>Zařízení</h2><span class="hint" id="restartFeedback">Záloha neobsahuje Home Assistant token. Restart nevymaže uložené nastavení.</span></div><div class="device-actions"><button id="exportConfig" type="button">Exportovat zálohu</button><button id="importConfig" type="button">Importovat zálohu</button><button class="danger" id="restartDevice" type="button">Restartovat zařízení</button><input class="hidden" id="importConfigFile" type="file" accept="application/json,.json"></div></section>
    <div class="footer"><button class="primary" id="saveButton" type="submit">Uložit nastavení</button><span class="feedback" id="saveFeedback" role="status"></span></div>
  </form>
</main>
<script>
const presets={co2:{label:"CO₂",suffix:"ppm",decimals:0,title:"CO₂"},voc:{label:"VOC",suffix:"ppb",decimals:0,title:"VOC"},pm25:{label:"PM2.5",suffix:"µg/m³",decimals:0,title:"PM2.5"},pm10:{label:"PM10",suffix:"µg/m³",decimals:0,title:"PM10"},humidity:{label:"VLHKOST",suffix:"%",decimals:0,title:"Vlhkost"},pressure:{label:"TLAK",suffix:"hPa",decimals:0,title:"Tlak"},aqi:{label:"AQI",suffix:"",decimals:0,title:"AQI"},illuminance:{label:"SVĚTLO",suffix:"lx",decimals:0,title:"Osvětlení"},noise:{label:"HLUK",suffix:"dB",decimals:0,title:"Hlučnost"},battery:{label:"BATERIE",suffix:"%",decimals:0,title:"Baterie"}};
const $=id=>document.getElementById(id);
const colorScales={metricAColor:[],metricBColor:[]};
const sunTimes={sunrise:0,sunset:0};
function encode(data){const body=new URLSearchParams();Object.entries(data).forEach(([k,v])=>body.set(k,v??""));return body}
async function request(url,data){const options=data?{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body:encode(data)}:{};const response=await fetch(url,options);const payload=await response.json().catch(()=>({ok:false,message:"Neplatná odpověď zařízení"}));if(!response.ok)throw new Error(payload.message||"Požadavek se nezdařil");return payload}
function populatePresetSelects(){document.querySelectorAll(".preset-select").forEach(select=>{select.innerHTML=Object.entries(presets).map(([id,p])=>`<option value="${id}">${p.title}</option>`).join("")})}
function updateMetric(prefix,forcePreset=false){const custom=$(prefix+"Mode").value==="custom";const preset=$(prefix+"Preset");preset.disabled=custom;["Name","Suffix"].forEach(part=>$(prefix+part).disabled=!custom);if(!custom&&(forcePreset||!$(prefix+"Name").value)){const p=presets[preset.value]||presets.co2;$(prefix+"Name").value=p.label;$(prefix+"Suffix").value=p.suffix;$(prefix+"Decimals").value=p.decimals}}
function setMetric(prefix,data){$(prefix+"Mode").value=data.custom?"custom":"preset";$(prefix+"Preset").value=data.preset||"co2";$(prefix+"Entity").value=data.entityId||"";$(prefix+"Name").value=data.name||"";$(prefix+"Suffix").value=data.suffix||"";$(prefix+"Decimals").value=String(data.decimals??0);updateMetric(prefix)}
function setRange(id,value){$(id).value=value;$(id+"Value").textContent=value+" %"}
function setSide(prefix,data){$(prefix+"Name").value=data.name||"";$(prefix+"TemperatureEntity").value=data.temperatureEntityId||"";$(prefix+"Color").value=data.color||"#ffffff";const icon=document.querySelector(`input[name="${prefix}Icon"][value="${data.icon||"home"}"]`);if(icon)icon.checked=true}
function normalizedColorPoint(point,fallbackColor){const value=Number(point?.value);const color=typeof point?.color==="string"&&/^#[0-9a-f]{6}$/i.test(point.color)?point.color:fallbackColor;return{value:Number.isFinite(value)?String(value):"",color}}
function readColorScale(prefix){return[...$(prefix+"Scale").querySelectorAll(".color-point")].map(row=>({value:row.querySelector("input[type=number]").value,color:row.querySelector("input[type=color]").value}))}
function renderColorScale(prefix,points){const fallback=prefix==="metricAColor"?"#65c744":"#ffb843";colorScales[prefix]=(Array.isArray(points)&&points.length?points:[{value:0,color:fallback}]).slice(0,10).map(point=>normalizedColorPoint(point,fallback));const container=$(prefix+"Scale");container.replaceChildren();const count=document.createElement("input");count.type="hidden";count.name=prefix+"Count";count.value=String(colorScales[prefix].length);container.appendChild(count);colorScales[prefix].forEach((point,index)=>{const row=document.createElement("div");row.className="color-point";const valueLabel=document.createElement("label");valueLabel.textContent="Hodnota";const value=document.createElement("input");value.type="number";value.step="any";value.required=true;value.name=prefix+"Value"+index;value.value=point.value;valueLabel.appendChild(value);const colorLabel=document.createElement("label");colorLabel.textContent="Barva";const color=document.createElement("input");color.type="color";color.name=prefix+"Color"+index;color.value=point.color;colorLabel.appendChild(color);row.append(valueLabel,colorLabel);if(index>0){const remove=document.createElement("button");remove.type="button";remove.className="color-point-remove";remove.textContent="×";remove.title="Odstranit barevný bod";remove.setAttribute("aria-label","Odstranit barevný bod");remove.addEventListener("click",()=>{const current=readColorScale(prefix);current.splice(index,1);renderColorScale(prefix,current)});row.appendChild(remove)}container.appendChild(row)});document.querySelector(`[data-color-add="${prefix}"]`).disabled=colorScales[prefix].length>=10}
document.querySelectorAll("[data-color-add]").forEach(button=>button.addEventListener("click",()=>{const prefix=button.dataset.colorAdd;const current=readColorScale(prefix);if(current.length>=10)return;current.push({value:"",color:current.at(-1)?.color||(prefix==="metricAColor"?"#65c744":"#ffb843")});renderColorScale(prefix,current);$(prefix+"Scale").querySelector(".color-point:last-child input[type=number]").focus()}));
function updateWeatherIconControls(){const enabled=$("animatedWeatherIcons").checked;$("weatherIconStyle").disabled=!enabled;const style=$("weatherIconStyle").value||"monochrome";$("meteoconsLink").href=`https://meteocons.com/icons/?style=${style}`}
function formatSunTime(timestamp,offset){if(!timestamp)return null;return new Intl.DateTimeFormat("cs-CZ",{hour:"2-digit",minute:"2-digit"}).format(new Date((timestamp+Number(offset)*60)*1000))}
function updateAutomaticDayNightControls(){const enabled=$("automaticDayNight").checked;$("automaticDayNightSettings").classList.toggle("hidden",!enabled);if(!enabled)return;const sunrise=formatSunTime(sunTimes.sunrise,$("sunriseOffsetMinutes").value);const sunset=formatSunTime(sunTimes.sunset,$("sunsetOffsetMinutes").value);$("sunTransitionTimes").textContent=sunrise&&sunset?`Denní režim od ${sunrise} · Noční režim od ${sunset}`:"Časy přechodů zatím nejsou načtené."}
function applyConfig(config){$("haUrl").value=config.homeAssistantUrl||"";$("tokenState").textContent=config.tokenConfigured?"Token je uložen":"Token není součástí zálohy";$("haToken").placeholder=config.tokenConfigured?"Zadej pouze při změně":"Zadej token";$("weatherEntity").value=config.weatherEntityId||"";$("sunEntity").value=config.sunEntityId||"";$("sunriseOffsetMinutes").value=String(config.sunriseOffsetMinutes??0);$("sunsetOffsetMinutes").value=String(config.sunsetOffsetMinutes??config.sunOffsetMinutes??0);$("nightVisualMode").value=config.nightVisualMode||"red";sunTimes.sunrise=Number(config.nextSunriseTimestamp)||0;sunTimes.sunset=Number(config.nextSunsetTimestamp)||0;$("animatedWeatherIcons").checked=config.animatedWeatherIcons!==false;$("weatherIconStyle").value=config.weatherIconStyle||"monochrome";updateWeatherIconControls();setSide("left",config.leftSide);setSide("right",config.rightSide);setMetric("metricA",config.metricA);setMetric("metricB",config.metricB);renderColorScale("metricAColor",config.metricAColorScale);renderColorScale("metricBColor",config.metricBColorScale);setRange("dayBrightness",config.dayBrightness);setRange("nightBrightness",config.nightBrightness);$("automaticDayNight").checked=config.automaticDayNight;updateAutomaticDayNightControls();$("automaticFirmwareUpdate").checked=config.automaticFirmwareUpdate;$("webMode").value=config.webMode||"timed";$("timeFont").value=config.timeFont||"barlow";$("timeColor").value=config.timeColor||"#f6f6f6";$("dateColor").value=config.dateColor||"#b5b5b5";$("leftWeatherIconColor").value=config.leftWeatherIconColor||"#ffffff";$("rightWeatherIconColor").value=config.rightWeatherIconColor||"#ffffff";$("secondRingEnabled").checked=config.secondRingEnabled;$("secondEffect").value=config.secondEffect||"dots";$("secondRingBackgroundDotSize").value=String(config.secondRingBackgroundDotSize??3);$("secondDotSize").value=String(config.secondDotSize??3);$("secondRingBackgroundColor").value=config.secondRingBackgroundColor||"#ffffff";$("secondRingBackgroundBrightness").value=String(config.secondRingBackgroundBrightness??0);$("secondRingBackgroundBrightnessValue").textContent=String(config.secondRingBackgroundBrightness??0);$("secondDotColor").value=config.secondDotColor||"#ffffff";$("secondDotBrightness").value=String(config.secondDotBrightness??175);$("secondDotBrightnessValue").textContent=String(config.secondDotBrightness??175)}
function applyCompleteConfig(config){applyConfig(config);$("dayNightLightEntity").value=config.dayNightLightEntityId||"";$("controlApiBase").value=config.controlSecret?`${location.origin}/api/control/${config.controlSecret}`:"Secret se nepodařilo načíst"}
async function loadConfig(){let lastError;for(let attempt=0;attempt<3;attempt++){try{const config=await request("/api/config");applyCompleteConfig(config);return}catch(error){lastError=error;if(attempt<2)await new Promise(resolve=>setTimeout(resolve,250*(attempt+1)))}}throw lastError}
function applyFirmwareStatus(status){$("currentFirmwareVersion").textContent=status.currentVersion||"—";$("serverFirmwareVersion").textContent=status.serverVersion||"Nezkontrolováno";const progress=status.totalBytes&&status.busy?` (${Math.floor(100*status.downloadedBytes/status.totalBytes)} %)` :"";$("firmwareFeedback").textContent=(status.message||"")+progress;$("firmwareFeedback").className="hint firmware-progress"+(status.state==="failed"?" feedback error":status.state==="current"||status.state==="available"?" token-state":"");$("checkFirmware").disabled=status.busy;$("installFirmware").disabled=status.busy;$("installFirmware").classList.toggle("hidden",!status.updateAvailable||!status.installationSupported)}
async function loadFirmwareStatus(){let lastError;for(let attempt=0;attempt<3;attempt++){try{const status=await request("/api/update-status");applyFirmwareStatus(status);if(status.busy)setTimeout(()=>loadFirmwareStatus().catch(()=>{}),1000);return status}catch(error){lastError=error;if(attempt<2)await new Promise(resolve=>setTimeout(resolve,250*(attempt+1)))}}throw lastError}
function connectionData(){return{haUrl:$("haUrl").value.trim(),haToken:$("haToken").value,haEntity:$("weatherEntity").value.trim()}}
$("testConnection").addEventListener("click",async()=>{const button=$("testConnection");button.disabled=true;$("haFeedback").className="hint";$("haFeedback").textContent="Ověřuji spojení…";try{await request("/api/ha/test",connectionData());$("haFeedback").className="hint token-state";$("haFeedback").textContent="Spojení s Home Assistantem je v pořádku."}catch(error){$("haFeedback").className="hint feedback error";$("haFeedback").textContent=error.message}finally{button.disabled=false}});
$("animatedWeatherIcons").addEventListener("change",updateWeatherIconControls);$("weatherIconStyle").addEventListener("change",updateWeatherIconControls);
$("automaticDayNight").addEventListener("change",updateAutomaticDayNightControls);["sunriseOffsetMinutes","sunsetOffsetMinutes"].forEach(id=>$(id).addEventListener("change",updateAutomaticDayNightControls));
document.querySelectorAll(".metric").forEach(section=>{const prefix=section.dataset.prefix;$(prefix+"Mode").addEventListener("change",()=>updateMetric(prefix,true));$(prefix+"Preset").addEventListener("change",()=>updateMetric(prefix,true))});
["dayBrightness","nightBrightness"].forEach(id=>$(id).addEventListener("input",()=>$(id+"Value").textContent=$(id).value+" %"));
$("secondRingBackgroundBrightness").addEventListener("input",()=>$("secondRingBackgroundBrightnessValue").textContent=$("secondRingBackgroundBrightness").value);
$("secondDotBrightness").addEventListener("input",()=>$("secondDotBrightnessValue").textContent=$("secondDotBrightness").value);
$("exportConfig").addEventListener("click",async()=>{const button=$("exportConfig");button.disabled=true;try{const current=await request("/api/config");const{ok,tokenConfigured,controlSecret,nextSunriseTimestamp,nextSunsetTimestamp,...config}=current;const backup={format:"waveshare-hodiny-settings",version:2,exportedAt:new Date().toISOString(),config};const blob=new Blob([JSON.stringify(backup,null,2)+"\n"],{type:"application/json"});const url=URL.createObjectURL(blob);const link=document.createElement("a");const stamp=new Date().toISOString().slice(0,19).replaceAll(":","-");link.href=url;link.download=`waveshare-hodiny-${stamp}.json`;document.body.appendChild(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);$("saveFeedback").className="feedback success";$("saveFeedback").textContent="Záloha byla exportována bez Home Assistant tokenu a secretu ovládacího API."}catch(error){$("saveFeedback").className="feedback error";$("saveFeedback").textContent=error.message}finally{button.disabled=false}});
$("importConfig").addEventListener("click",()=>$("importConfigFile").click());
$("importConfigFile").addEventListener("change",async event=>{const file=event.currentTarget.files[0];event.currentTarget.value="";if(!file)return;const button=$("importConfig");button.disabled=true;try{if(file.size>131072)throw new Error("Soubor zálohy je příliš velký");const backup=JSON.parse(await file.text());if(backup?.format!=="waveshare-hodiny-settings"||backup?.version!==2||!backup.config||typeof backup.config!=="object"||!backup.config.leftSide||!backup.config.rightSide||!backup.config.metricA||!backup.config.metricB||!Array.isArray(backup.config.metricAColorScale)||!Array.isArray(backup.config.metricBColorScale))throw new Error("Soubor není platná záloha Waveshare Hodiny");const current=await request("/api/config");applyCompleteConfig({...backup.config,tokenConfigured:current.tokenConfigured,webMode:backup.config.webMode??current.webMode});$("saveFeedback").className="feedback";$("saveFeedback").textContent="Záloha byla načtena, ukládám nastavení…";$("configForm").requestSubmit()}catch(error){$("saveFeedback").className="feedback error";$("saveFeedback").textContent=error.message}finally{button.disabled=false}});
$("configForm").addEventListener("submit",async event=>{event.preventDefault();if($("automaticDayNight").checked&&!$("sunEntity").value.trim()){$("saveFeedback").className="feedback error";$("saveFeedback").textContent="Pro automatický režim DEN/NOC musí být vyplněna SUN entita.";$("sunEntity").focus();return}const button=$("saveButton");button.disabled=true;$("saveFeedback").className="feedback";$("saveFeedback").textContent="Ukládám…";const data=Object.fromEntries(new FormData(event.currentTarget).entries());["leftName","rightName"].forEach(field=>data[field]=(data[field]||"").toLocaleUpperCase("cs-CZ"));["metricA","metricB"].forEach(prefix=>{if(data[prefix+"Name"])data[prefix+"Name"]=data[prefix+"Name"].toLocaleUpperCase("cs-CZ")});data.automaticDayNight=$("automaticDayNight").checked?"1":"0";data.automaticFirmwareUpdate=$("automaticFirmwareUpdate").checked?"1":"0";data.animatedWeatherIcons=$("animatedWeatherIcons").checked?"1":"0";data.weatherIconStyle=$("weatherIconStyle").value||"monochrome";data.secondRingEnabled=$("secondRingEnabled").checked?"1":"0";try{await request("/api/config",data);if(data.webMode!=="disabled")await loadConfig();$("haToken").value="";$("tokenState").textContent="Token je uložen";$("saveFeedback").className="feedback success";$("saveFeedback").textContent=data.webMode==="disabled"?"Nastavení bylo uloženo a webový server byl vypnut. Znovu jej můžeš povolit na displeji hodin.":"Nastavení bylo úspěšně uloženo a barevné body byly seřazeny."}catch(error){$("saveFeedback").className="feedback error";$("saveFeedback").textContent=error.message}finally{button.disabled=false}});
async function setDisplayPower(command){const base=$("controlApiBase").value;const feedback=$("controlFeedback");try{const response=await fetch(`${base}/display/${command}`,{method:"POST"});const payload=await response.json().catch(()=>({}));if(!response.ok)throw new Error(payload.message||`HTTP ${response.status}`);feedback.className="hint token-state";feedback.textContent=command==="off"?"Podsvícení je vynuceně vypnuté.":"Podsvícení je zapnuté a používá aktuální jas DEN/NOC."}catch(error){feedback.className="hint feedback error";feedback.textContent=error.message}}
$("displayOff").addEventListener("click",()=>setDisplayPower("off"));$("displayOn").addEventListener("click",()=>setDisplayPower("on"));
$("checkFirmware").addEventListener("click",async()=>{try{await request("/api/check-update",{});await loadFirmwareStatus()}catch(error){$("firmwareFeedback").className="hint firmware-progress feedback error";$("firmwareFeedback").textContent=error.message}});
$("installFirmware").addEventListener("click",async()=>{if(!confirm("Nainstalovat novou verzi firmware a restartovat zařízení?"))return;try{await request("/api/install-update",{});await loadFirmwareStatus()}catch(error){$("firmwareFeedback").className="hint firmware-progress feedback error";$("firmwareFeedback").textContent=error.message}});
$("restartDevice").addEventListener("click",async()=>{if(!confirm("Opravdu restartovat zařízení? Uložené nastavení zůstane zachováno."))return;const button=$("restartDevice");button.disabled=true;$("restartFeedback").className="hint token-state";$("restartFeedback").textContent="Zařízení se restartuje…";try{await request("/api/restart",{});$("deviceStatus").textContent="Zařízení se restartuje";setTimeout(()=>location.reload(),5000)}catch(error){$("restartFeedback").className="hint feedback error";$("restartFeedback").textContent=error.message;button.disabled=false}});
async function loadInitialData(){try{await loadConfig()}catch(error){$("saveFeedback").className="feedback error";$("saveFeedback").textContent="Nastavení se nepodařilo načíst: "+error.message}await new Promise(resolve=>setTimeout(resolve,300));try{await loadFirmwareStatus()}catch(error){$("firmwareFeedback").className="hint firmware-progress feedback error";$("firmwareFeedback").textContent=error.message}}
populatePresetSelects();loadInitialData();
</script>
</body>
</html>
)HTML";
