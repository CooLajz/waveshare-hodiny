const {chromium}=require('playwright');
const fs=require('fs'),assert=require('assert'),http=require('http'),path=require('path');
const root=path.resolve(__dirname,'..');
const source=name=>fs.readFileSync(path.join(root,'WaveshareHodiny',name),'utf8').match(/R"\w*\(([\s\S]*)\)\w*";/)[1];
(async()=>{
  const server=http.createServer((req,res)=>{res.setHeader('Content-Type',req.url==='/ui-language.js'?'text/javascript':'text/html');res.end(source(req.url==='/ui-language.js'?'ConfigurationLocalization.h':'ConfigurationPage.h'));});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  const browser=await chromium.launch({channel:'chrome',headless:true});
  try {
    const page=await browser.newPage(); const errors=[];page.on('pageerror',e=>errors.push(e.message));
    const settings=JSON.stringify({format:'waveshare-hodiny-encrypted',project:'waveshare-hodiny',version:1,settingsSchema:1,configSchema:30,firmwareVersion:'development',createdAt:0})+'\nAAAA';
    let importing=false,complete=false,bytes=460800,offset=0,parts=0,sawBlanking=false,interrupt=false,cancelled=false;
    await page.route('**/api/**',async route=>{
      const request=route.request(),url=new URL(request.url()),data=Object.fromEntries(new URLSearchParams(request.postData()||''));let result={ok:true};
      if(url.pathname==='/api/config')result={ok:true,language:'cs',dataSource:'open-meteo',leftSide:{},rightSide:{},metricA:{},metricB:{},supportedBackupSchemas:[30]};
      if(url.pathname==='/api/background')result={ok:true,present:false,enabled:false,opacity:80,shadow:100,shadowSize:0,shadowSpread:2};
      if(url.pathname==='/api/backup/export'||url.pathname==='/api/backup/import'){importing=url.pathname.endsWith('import');complete=false;offset=0;parts=0;if(importing){assert.equal(data.backup,settings);assert.equal(data.format,'2');assert.equal(Number(data.imageBytes),bytes)}}
      if(url.pathname==='/api/backup/status')result=importing?(complete?{ok:true,state:'committed',id:url.searchParams.get('id'),restarting:false}:{ok:true,state:'upload'}):{ok:true,state:'ready',file:settings,imageHeader:'ab'.repeat(28),imageBytes:bytes};
      if(url.pathname==='/api/backup/cancel')cancelled=true;
      if(url.pathname==='/api/backup/part'){
        if(interrupt)return route.abort('failed');
        assert.equal(Number(data.offset),offset);const size=Math.min(4096,bytes-offset),last=offset+size===bytes;
        if(importing){assert.equal(data.data.length,(size+(last?16:0))*2);sawBlanking=await page.locator('#backupFeedback').innerText().then(v=>v.includes('dočasně vypnutý'));}
        result={ok:true,data:'ab'.repeat(size+(last?16:0))};offset+=size;parts++;complete=last;
      }
      await route.fulfill({contentType:'application/json',body:JSON.stringify(result)});
    });
    await page.goto('http://127.0.0.1:'+server.address().port);await page.locator('#configForm').waitFor({state:'visible'});
    for(const size of [460800,0]){
      bytes=size;
      await page.evaluate(()=>openBackupDialog(false));await page.locator('#backupPassword').fill('synthetic-password');await page.locator('#backupPasswordConfirm').fill('synthetic-password');
      const download=page.waitForEvent('download');await page.locator('#backupSubmit').click();const file=await download.catch(async e=>{console.error('Feedback:',await page.locator('#backupFeedback').innerText(),'parts:',parts,'errors:',errors);throw e});const text=fs.readFileSync(await file.path(),'utf8');const bundle=JSON.parse(text);
      assert.equal(bundle.version,2);assert.equal(bundle.image.bytes,bytes);assert.equal(parts,Math.max(1,Math.ceil(bytes/4096)));assert.equal(bundle.settings,settings);
      await page.waitForFunction(()=>!backupRunning);await page.locator('#backupCancel').click();
      await page.evaluate(()=>openBackupDialog(true));await page.evaluate(text=>selectBackupFile(new File([text],'test.whbackup')),text);await page.locator('#backupPassword').fill('synthetic-password');assert.equal(await page.locator('#backupSubmit').isEnabled(),true);
      await page.locator('#backupSubmit').click();await page.waitForFunction(()=>!backupRunning);assert((await page.locator('#backupFeedback').innerText()).includes('ověřená po restartu'));assert(sawBlanking);await page.locator('#backupCancel').click();
      if(bytes){bundle.image.parts.pop();await page.evaluate(()=>openBackupDialog(true));await page.evaluate(text=>selectBackupFile(new File([text],'broken.whbackup')),JSON.stringify(bundle));assert(await page.locator('#backupSubmit').isDisabled());await page.locator('#backupCancel').click();}
    }
    interrupt=true;await page.evaluate(()=>openBackupDialog(false));await page.locator('#backupPassword').fill('synthetic-password');await page.locator('#backupPasswordConfirm').fill('synthetic-password');await page.locator('#backupSubmit').click();await page.waitForFunction(()=>!backupRunning);assert(cancelled);assert.equal(await page.evaluate(()=>sessionStorage.getItem(backupPendingKey)),null);
    assert.deepEqual(errors,[]);console.log('PASS: encrypted bundle download/upload, 460800-byte and absent image, progress, blanking notice, malformed bundle rejection');
  } finally {await browser.close();server.close();}
})().catch(error=>{console.error(error);process.exit(1)});
