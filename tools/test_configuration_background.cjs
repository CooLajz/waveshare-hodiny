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
let notificationStatus=200;const notifications=[];
const requests=[];let config={ok:true,controlSecret:'test-notification-secret',language:'cs',dataSource:'open-meteo',leftSide:{},rightSide:{},metricA:{},metricB:{},dayBrightness:80,nightBrightness:10,clockStyle:'digital',timeFont:'barlow',saveConfirmationId:''};let saves=0,active=0,maxActive=0,saveMode="ok";
await page.route('**/api/**',async route=>{const req=route.request(),url=new URL(req.url());if(url.pathname==='/')return route.fulfill({contentType:'text/html',headers:{'Content-Encoding':'gzip'},body:compressedHtml});if(url.pathname==='/ui-language.js')return route.fulfill({contentType:'text/javascript',headers:{'Content-Encoding':'gzip'},body:compressedLocalization});let data={ok:true};if(url.pathname.endsWith('/notification')){notifications.push(JSON.parse(req.postData()));assert(req.headers()['content-type'].includes('application/json'));await new Promise(r=>setTimeout(r,150));return route.fulfill({status:notificationStatus,contentType:'application/json',body:JSON.stringify(notificationStatus===200?{ok:true}:{ok:false,message:'Displej je vypnutý.'})})}if(url.pathname==='/api/config'){if(req.method()==='POST'){saves++;const body=Object.fromEntries(new URLSearchParams(req.postData()));if(body.backgroundEnabled!==undefined)background={...background,enabled:body.backgroundEnabled==='1',opacity:Number(body.backgroundOpacity),shadow:Number(body.backgroundShadow),shadowSize:Number(body.backgroundShadowSize),shadowSpread:Number(body.backgroundShadowSpread),clockOnly:body.backgroundClockOnly==='1'};config={...config,...body,forecastTemperatureEntityId:body.forecastTemperatureEntity??config.forecastTemperatureEntityId,use12HourFormat:body.use12HourFormat==="1",automaticDayNight:body.automaticDayNight==="1",automaticRadarRotation:body.automaticRadarRotation==="1"};if(saveMode==="lost")return route.abort("failed");if(saveMode==="wrong")config.saveConfirmationId="different";}data=config;}else if(url.pathname.endsWith('/preview')){active++;maxActive=Math.max(maxActive,active);requests.push({url:url.pathname,data:Object.fromEntries(new URLSearchParams(req.postData()))});await new Promise(r=>setTimeout(r,240));active--;}else if(url.pathname==='/api/update-status'){await new Promise(r=>setTimeout(r,2500));data={ok:true,currentVersion:'test',state:'current'};}return route.fulfill({contentType:'application/json',body:JSON.stringify(data)});});
// Exercise the actual editor and RGB565 upload contract independently.
let background={ok:true,present:false,enabled:false,opacity:35,shadow:70,shadowSize:2,shadowSpread:0};
let imageGate=Promise.resolve(),imageRequests=0;
let backgroundPreviews=[];let chunks=[],stored=null,optionsSaved=0,commits=0,cancels=0,failChunk=false,failOptions=false;
let releaseInitialBackground;const initialBackgroundGate=new Promise(resolve=>releaseInitialBackground=resolve);let backgroundRequested=false;
await page.route('**/api/background**',async route=>{
 const req=route.request(),url=new URL(req.url()),data=Object.fromEntries(new URLSearchParams(req.postData()||''));
 if(url.pathname==='/api/background'){backgroundRequested=true;await initialBackgroundGate}
 if(url.pathname.endsWith('/image')){imageRequests++;await imageGate;return route.fulfill({contentType:'application/octet-stream',body:stored})}
 if(url.pathname.endsWith('/preview'))backgroundPreviews.push(data);
 if(url.pathname.endsWith('/start')){chunks=[];return route.fulfill({json:{ok:true,token:'test-upload'}})}
 if(url.pathname.endsWith('/cancel'))cancels++;
 if(url.pathname.endsWith('/chunk')&&failChunk)return route.fulfill({status:503,json:{ok:false,message:'Test upload failure'}});
 if(url.pathname.endsWith('/chunk')){assert.equal(data.token,'test-upload');assert.equal(Number(data.offset),Buffer.concat(chunks).length);assert(data.data.length<=16384);chunks.push(Buffer.from(data.data,'hex'))}
 if(url.pathname.endsWith('/commit')){stored=Buffer.concat(chunks);assert.equal(stored.length,460800);background={...background,present:true,enabled:data.enabled==='1',opacity:Number(data.opacity),shadow:Number(data.shadow),shadowSize:Number(data.shadowSize),shadowSpread:Number(data.shadowSpread),clockOnly:data.clockOnly==='1'};commits++}
 if(url.pathname.endsWith('/options')&&failOptions)return route.fulfill({status:503,json:{ok:false,message:'Test toggle failure'}});
 if(url.pathname.endsWith('/options')){optionsSaved++;background={...background,enabled:data.enabled==='1',opacity:Number(data.opacity),shadow:Number(data.shadow),shadowSize:Number(data.shadowSize),shadowSpread:Number(data.shadowSpread),clockOnly:data.clockOnly==='1'};if(data.remove==='1'){stored=null;background.present=false}}
 await route.fulfill({json:url.pathname==='/api/background'?background:{ok:true}});
});
await page.goto(origin+'/');
while(!backgroundRequested)await page.waitForTimeout(10);
assert(!(await page.locator('#backgroundDetails').isVisible()));
assert(await page.locator('#backgroundEnabled').isDisabled());
assert(!(await page.locator('#configForm').isVisible()));
releaseInitialBackground();await page.locator('#configForm').waitFor({state:'visible'});await page.waitForFunction(()=>backgroundLoaded);
await page.locator('[data-tab="display"]').click();
assert(!(await page.locator('#backgroundDetails').isVisible()));
assert(!(await page.locator('#backgroundEnabled').isChecked()));
background.enabled=true;await page.reload();await page.locator('#configForm').waitFor({state:'visible'});
await page.locator('[data-tab="display"]').click();await page.waitForFunction(()=>backgroundLoaded);
assert(await page.locator('#backgroundDetails').isVisible());assert(await page.evaluate(()=>JSON.stringify(backgroundControls())===savedBackgroundControls));
assert.equal(await page.title(),'Waveshare Hodiny');
const png=await page.evaluate(()=>{const c=document.createElement('canvas');c.width=960;c.height=480;const x=c.getContext('2d');x.fillStyle='#f00';x.fillRect(0,0,480,480);x.fillStyle='#00f';x.fillRect(480,0,480,480);return c.toDataURL('image/png').split(',')[1]});
await page.locator('#backgroundFile').setInputFiles({name:'crop-test.png',mimeType:'image/png',buffer:Buffer.from(png,'base64')});
await page.waitForFunction(()=>backgroundChanged&&backgroundSource);
assert.equal(commits,0);assert.equal(optionsSaved,0);
assert.equal(await page.locator('#backgroundFace').count(),0);
const originalCrop=await page.locator('#backgroundCanvas').evaluate(c=>c.toDataURL());
await page.locator('#backgroundOpacity').fill('0');
await page.locator('#backgroundShadow').fill('100');
await page.locator('#backgroundShadowSize').fill('5');
await page.locator('#backgroundShadowSpread').fill('5');
await page.evaluate(()=>{document.getElementById('backgroundEnabled').checked=false;updateBackgroundControls()});
assert.equal(await page.locator('#backgroundCanvas').evaluate(c=>c.toDataURL()),originalCrop);
await page.locator('#backgroundEnabled').check();assert(!(await page.locator('#backgroundUploadDialog').isVisible()));await page.waitForFunction(()=>!backgroundBusy);assert(!(await page.locator('#backgroundUploadDialog').isVisible()));

await page.locator('#backgroundOpacity').fill('60');await page.locator('#backgroundShadow').fill('80');
await page.locator('#backgroundApply').click();assert(await page.locator('#backgroundUploadDialog').isVisible());assert((await page.locator('#backgroundUploadDescription').textContent()).includes('černý'));await page.waitForFunction(()=>!backgroundBusy);await page.locator('#backgroundUploadClose').click();
assert.equal(commits,1);assert.equal(background.opacity,60);assert.equal(background.shadow,80);
// The main settings save must also persist background controls without uploading pixels.
await page.locator('#backgroundOpacity').fill('80');
const beforeMainSave=optionsSaved;await page.evaluate(()=>saveConfiguration());assert.equal(background.opacity,80);assert.equal(commits,1);assert.equal(optionsSaved,beforeMainSave,'main save must use one config transaction');
await page.locator('#backgroundOpacity').fill('60');await page.evaluate(()=>saveConfiguration());
await page.locator('input[name="clockStyle"][value="analog"]').check();
assert(await page.locator('#backgroundClockOnly').isVisible());
await page.locator('#backgroundClockOnly').check();await page.evaluate(()=>flushBackgroundPreview());
assert.equal(backgroundPreviews.at(-1).clockOnly,'1');
await page.evaluate(()=>saveConfiguration());assert.equal(background.clockOnly,true);assert.equal(commits,1);
await page.locator('input[name="clockStyle"][value="digital"]').check();
assert(!(await page.locator('#backgroundClockOnly').isVisible()));
await page.locator('input[name="clockStyle"][value="analog"]').check();
assert(await page.locator('#backgroundClockOnly').isChecked());
// Exact center crop; raw pixels must not contain the preview clock or dimming.
assert.equal(stored.readUInt16LE((240*480+50)*2),0xf800);
assert.equal(stored.readUInt16LE((240*480+430)*2),0x001f);
await page.locator('#backgroundZoom').fill('2');
await page.locator('#backgroundCanvas').scrollIntoViewIfNeeded();
const box=await page.locator('#backgroundCanvas').boundingBox();
await page.mouse.move(box.x+box.width/2,box.y+box.height/2);await page.mouse.down();await page.mouse.move(box.x+box.width,box.y+box.height/2,{steps:8});await page.mouse.up();
await page.locator('#backgroundApply').click();await page.waitForFunction(()=>!backgroundBusy);await page.locator('#backgroundUploadClose').click();
assert.equal(commits,2);assert.equal(stored.readUInt16LE((240*480+400)*2),0xf800);
await page.locator('#backgroundEnabled').uncheck();assert(!(await page.locator('#backgroundDetails').isVisible()));assert(!(await page.locator('#backgroundUploadDialog').isVisible()));await page.waitForFunction(()=>!backgroundBusy);assert.equal(commits,2);assert.equal(background.enabled,false);
await page.locator('#backgroundEnabled').check();assert(!(await page.locator('#backgroundUploadDialog').isVisible()));await page.waitForFunction(()=>!backgroundBusy);assert(!(await page.locator('#backgroundUploadDialog').isVisible()));
await page.locator('#backgroundSection').scrollIntoViewIfNeeded();await page.screenshot({path:'/tmp/clock-background-desktop.png'});
await page.setViewportSize({width:390,height:844});await page.locator('#backgroundCanvas').scrollIntoViewIfNeeded();await page.screenshot({path:'/tmp/clock-background-mobile.png'});assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
await page.locator('#backgroundApply').click();await page.waitForFunction(()=>!backgroundBusy);await page.locator('#backgroundUploadClose').click();
let releaseImage;imageGate=new Promise(resolve=>releaseImage=resolve);const requestsBefore=imageRequests;
await page.reload();await page.locator('#configForm').waitFor({state:'visible'});
assert(await page.evaluate(()=>backgroundLoaded));assert.equal(imageRequests,requestsBefore);
await page.locator('[data-tab="display"]').click();
assert(await page.locator('#backgroundImageLoading').isVisible());assert(!(await page.locator('#backgroundOpacity').isDisabled()));
assert.equal(await page.locator('#backgroundOpacity').inputValue(),'60');assert(await page.locator('#backgroundClockOnly').isChecked());
releaseImage();await page.waitForFunction(()=>!!backgroundSource);
assert(!(await page.locator('#backgroundImageLoading').isVisible()));
page.on('dialog',dialog=>dialog.accept());await page.locator('#backgroundRemove').click();await page.waitForFunction(()=>!backgroundBusy);await page.locator('#backgroundUploadClose').click();assert.equal(background.present,false);assert.equal(await page.evaluate(()=>backgroundSource),null);
// A failed transfer must call cancel and leave the crop available for retry.
const beforeCancel=cancels,beforeOptions=optionsSaved;
await page.locator('#backgroundFile').setInputFiles({name:'retry.png',mimeType:'image/png',buffer:Buffer.from(png,'base64')});
await page.waitForFunction(()=>backgroundChanged&&backgroundSource);failChunk=true;
await page.locator('#backgroundApply').click();await page.waitForFunction(()=>!backgroundBusy);await page.locator('#backgroundUploadClose').click();
assert.equal(cancels,beforeCancel+1);assert.equal(optionsSaved,beforeOptions);
assert(await page.evaluate(()=>backgroundChanged));assert((await page.locator('#backgroundFeedback').textContent()).includes('Test upload failure'));
failOptions=true;await page.locator('#backgroundEnabled').uncheck();await page.waitForFunction(()=>!backgroundBusy);assert(await page.locator('#backgroundEnabled').isChecked());assert(!(await page.locator('#backgroundUploadDialog').isVisible()));assert((await page.locator('#backgroundToggleFeedback').textContent()).includes('Test toggle failure'));
assert.deepEqual(errors,[]);console.log(JSON.stringify({passed:true,commits,optionsSaved,errors,checks:['crop','RGB565','no preview baked in','zoom and drag','opacity','shadows','disable','reload','delete','mobile','cancel failed transfer','analog-only control','clock-only preview and persistence']}));
await browser.close();await new Promise(resolve=>server.close(resolve));
})().catch(error=>{console.error(error);process.exit(1)});
