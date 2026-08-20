#pragma once

#include <pgmspace.h>

const char LOGIN_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="cs">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <meta name="color-scheme" content="dark">
  <title>Přihlášení · Waveshare Hodiny</title>
  <style>
    :root{--bg:#101418;--surface:#171c20;--field:#1c2227;--line:#3b444b;--text:#f3f6f8;--muted:#9da7ae;--cyan:#4ccbec;--error:#ff6262;--radius:10px}
    *{box-sizing:border-box}html{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;background:var(--bg);color:var(--text);font-size:16px}body{min-height:100vh;display:grid;place-items:center;margin:0;padding:24px;background:var(--bg)}
    main{width:min(440px,100%);padding:30px;border:1px solid var(--line);border-radius:14px;background:var(--surface);box-shadow:0 18px 50px rgba(0,0,0,.32)}
    h1{margin:0 0 10px;font-size:28px;letter-spacing:-.025em}p{margin:0 0 24px;color:var(--muted);line-height:1.45}label{display:block;margin:0 0 8px;font-size:14px;font-weight:700}
    input,button{width:100%;height:48px;border-radius:var(--radius);font:inherit}input{padding:0 13px;border:1px solid #505a61;outline:0;background:var(--field);color:var(--text)}input:focus{border-color:var(--cyan);box-shadow:0 0 0 3px rgba(76,203,236,.15)}
    button{margin-top:16px;border:1px solid var(--cyan);background:var(--cyan);color:#092028;font-weight:750;cursor:pointer}button:disabled{opacity:.55;cursor:wait}.feedback{min-height:20px;margin:14px 0 0;color:var(--muted);font-size:14px}.feedback.error{color:var(--error)}
  </style>
</head>
<body>
<main>
  <h1>Waveshare Hodiny</h1>
  <p>Nastavení je chráněné heslem.</p>
  <form id="loginForm">
    <label for="password">Heslo</label>
    <input id="password" type="password" minlength="6" maxlength="20" autocomplete="current-password" autofocus required>
    <button id="loginButton" type="submit">Přihlásit</button>
    <div class="feedback" id="feedback" role="status"></div>
  </form>
</main>
<script>
const form=document.getElementById("loginForm"),button=document.getElementById("loginButton"),feedback=document.getElementById("feedback"),password=document.getElementById("password");
form.addEventListener("submit",async event=>{event.preventDefault();button.disabled=true;feedback.className="feedback";feedback.textContent="Přihlašuji…";try{const body=new URLSearchParams({password:password.value});const response=await fetch("/api/auth/login",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body});const payload=await response.json().catch(()=>({message:"Neplatná odpověď zařízení"}));if(!response.ok)throw new Error(payload.message||"Přihlášení se nezdařilo");location.replace("/")}catch(error){feedback.className="feedback error";feedback.textContent=error.message;password.select()}finally{button.disabled=false}});
</script>
</body>
</html>
)HTML";
