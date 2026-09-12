const { chromium } = require('playwright');
const fs=require('fs'),assert=require('assert');
// Run with Playwright available through NODE_PATH; uses an isolated Chrome profile.
const root=require('path').resolve(__dirname,'..')+'/';
const html=fs.readFileSync(root+'WaveshareHodiny/ConfigurationPage.h','utf8').match(/R"\w*\(([\s\S]*)\)\w*";/)[1];
const localization=fs.readFileSync(root+'WaveshareHodiny/ConfigurationLocalization.h','utf8').match(/R"\w*\(([\s\S]*)\)\w*";/)[1];
const generated=fs.readFileSync(root+'WaveshareHodiny/local/ConfigurationAssets.h','utf8');
function asset(symbol,source){
  const array=generated.match(new RegExp(symbol+'_GZIP\\[\\].*?\\{([\\s\\S]*?)\\};'))[1];
  const bytes=Buffer.from(array.match(/0x[0-9a-f]{2}/g).map(value=>parseInt(value,16)));
  assert.equal(require('zlib').gunzipSync(bytes).toString(),source,'stale compressed asset');
  return bytes;
}
const compressedHtml=asset('CONFIGURATION_PAGE',html),compressedLocalization=asset('CONFIGURATION_LOCALIZATION_JS',localization);
(async()=>{
// Serve compressed assets over HTTP: Playwright fulfill bypasses content decoding.
const server=require('http').createServer((req,res)=>{
  const script=req.url==='/ui-language.js';
  res.writeHead(200,{'Content-Type':script?'text/javascript':'text/html','Content-Encoding':'gzip'});
  res.end(script?compressedLocalization:compressedHtml);
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const origin='http://127.0.0.1:'+server.address().port;
const browser=await chromium.launch({headless:true,channel:"chrome"});
const page=await browser.newPage({viewport:{width:1360,height:1000}});const errors=[];page.on('pageerror',e=>errors.push(e.message));
const requests=[];let config={ok:true,language:'cs',dataSource:'open-meteo',leftSide:{},rightSide:{},metricA:{},metricB:{},dayBrightness:80,nightBrightness:10,clockStyle:'digital',timeFont:'barlow',saveConfirmationId:''};let saves=0,active=0,maxActive=0,saveMode="ok";
await page.route('**/api/**',async route=>{const req=route.request(),url=new URL(req.url());if(url.pathname==='/')return route.fulfill({contentType:'text/html',headers:{'Content-Encoding':'gzip'},body:compressedHtml});if(url.pathname==='/ui-language.js')return route.fulfill({contentType:'text/javascript',headers:{'Content-Encoding':'gzip'},body:compressedLocalization});let data={ok:true};if(url.pathname==='/api/config'){if(req.method()==='POST'){saves++;const body=Object.fromEntries(new URLSearchParams(req.postData()));config={...config,...body,use12HourFormat:body.use12HourFormat==="1",automaticRadarRotation:body.automaticRadarRotation==="1"};if(saveMode==="lost")return route.abort("failed");if(saveMode==="wrong")config.saveConfirmationId="different";}data=config;}else if(url.pathname.endsWith('/preview')){active++;maxActive=Math.max(maxActive,active);requests.push({url:url.pathname,data:Object.fromEntries(new URLSearchParams(req.postData()))});await new Promise(r=>setTimeout(r,240));active--;}else if(url.pathname==='/api/update-status'){await new Promise(r=>setTimeout(r,2500));data={ok:true,currentVersion:'test',state:'current'};}return route.fulfill({contentType:'application/json',body:JSON.stringify(data)});});
const start=Date.now();await page.goto(origin+'/');await page.locator('#configForm').waitFor({state:'visible'});assert(Date.now()-start<2400,'form blocked by firmware');
console.log('form visible before delayed firmware status');console.log(await page.title());
await page.locator('[data-tab="display"]').click();
await page.locator('#timeFont').selectOption('doto');await page.waitForTimeout(150);await page.locator('#timeFont').selectOption('lcd');
await page.locator('#timeColor').evaluate(el=>{el.value='#123456';el.dispatchEvent(new Event('input',{bubbles:true}));});
await page.waitForTimeout(650);assert.equal(requests.at(-1).data.timeFont,'lcd');assert.equal(requests.at(-1).data.timeColor,'#123456');assert.equal(saves,0);assert.equal(maxActive,1);
await page.locator('#secondDotBrightness').evaluate(el=>{for(let i=0;i<60;i++){el.value=String(i);el.dispatchEvent(new Event('input',{bubbles:true}));}});
await page.locator('#headerSaveButton').click();await page.waitForFunction(()=>!settingsSaving);assert.equal(saves,1);assert.equal(config.timeFont,'lcd');assert.equal(config.secondDotBrightness,'59');assert.equal(config.timeColor,'#123456');
assert.equal(await page.locator('#timeFont').inputValue(),'lcd');assert.equal(await page.locator('#configForm').evaluate(e=>e.inert),false);
// A lost POST response must recover using the durable receipt.
saveMode="lost";await page.locator('#headerSaveButton').click();await page.waitForFunction(()=>!settingsSaving);assert(await page.locator('#saveFeedback').evaluate(el=>el.classList.contains('success')));
// A mismatched receipt must never report success or discard the edited value.
saveMode="wrong";await page.locator('#timeFont').selectOption('doto');await page.locator('#headerSaveButton').click();await page.waitForFunction(()=>!settingsSaving);assert(await page.locator('#saveFeedback').evaluate(el=>el.classList.contains('error')));assert.equal(await page.locator('#timeFont').inputValue(),'doto');
saveMode="ok";await page.locator('#headerSaveButton').click();await page.waitForFunction(()=>!settingsSaving);
// Style changes share the same queue and cannot reset newer local selections.
await page.locator('input[name="clockStyle"][value="analog"]').check();await page.waitForTimeout(150);await page.locator('input[name="clockStyle"][value="digital"]').check();await page.waitForTimeout(600);assert.equal(await page.locator('input[name="clockStyle"]:checked').inputValue(),'digital');assert.equal(maxActive,1);
await page.locator('#headerSaveButton').click();await page.waitForFunction(()=>!settingsSaving);
assert.equal(await page.locator('input[name="clockStyle"]').count(),3);
assert.equal(await page.locator('input[name="clockStyle"][value="forecast"]').count(),0);
await page.locator('#automaticRadarRotation').check();
for (const id of ['clockDisplaySeconds','forecastDisplaySeconds','radarDisplaySeconds']) {
  await page.locator('#'+id).fill('0');
  assert.equal(await page.locator('#'+id).getAttribute('min'),'0');
}
await page.evaluate(()=>saveConfiguration());
assert.equal(Number(config.forecastDisplaySeconds),0);
assert.equal(Number(config.clockDisplaySeconds),0);
assert.equal(Number(config.radarDisplaySeconds),0);
await page.locator('#forecastDisplaySeconds').fill('37');
await page.evaluate(()=>saveConfiguration());
assert.equal(Number(config.forecastDisplaySeconds),37);
await page.evaluate(()=>{updateRadarAvailability(false)});
assert(await page.locator('#forecastDisplaySeconds').isVisible());
assert.equal(await page.locator('#automaticRadarRotation').isDisabled(),false);
await page.evaluate(()=>applyDeviceLanguage('en'));await page.waitForTimeout(150);
assert(await page.getByText('Show forecast for',{exact:true}).isVisible());
await page.screenshot({path:'/tmp/clock-web-desktop-'+Date.now()+'.png'});
await page.setViewportSize({width:390,height:844});await page.screenshot({path:'/tmp/clock-web-mobile-'+Date.now()+'.png'});
assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
assert.deepEqual(errors,[]);console.log(JSON.stringify({passed:true,previews:requests.length,saves,maxActive,errors}));await browser.close();await new Promise(resolve=>server.close(resolve));
})().catch(e=>{console.error(e);process.exit(1)});
