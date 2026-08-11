import fs from 'node:fs/promises';
import { pathToFileURL } from 'node:url';

const [root, style, playwrightModule, ...icons] = process.argv.slice(2);

if (!root || !style || !playwrightModule || icons.length === 0) {
  throw new Error(
    'Usage: node render_meteocons_svg.mjs <root> <style> <playwright.mjs> <icon...>',
  );
}

const { chromium } = await import(pathToFileURL(playwrightModule).href);
const browser = await chromium.launch({
  executablePath: '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 84, height: 84 } });
await page.setContent(
  '<style>html,body,#icon{margin:0;width:84px;height:84px;background:transparent;overflow:hidden;color:#fff}svg{display:block;width:84px;height:84px}</style><div id="icon"></div>',
);

for (const icon of icons) {
  const svg = await fs.readFile(`${root}/svg/${style}/${icon}.svg`, 'utf8');
  await page.locator('#icon').evaluate((element, markup) => {
    element.innerHTML = markup;
    const root = element.querySelector('svg');
    root.pauseAnimations();
    root.setCurrentTime(0);
  }, svg);
  const framesDirectory = `${root}/frames/${style}/${icon}`;
  await fs.mkdir(framesDirectory, { recursive: true });
  for (let frame = 0; frame < 90; frame += 1) {
    await page.locator('#icon svg').evaluate(
      (root, seconds) => root.setCurrentTime(seconds),
      (frame * 6) / 90,
    );
    await page.locator('#icon').screenshot({
      path: `${framesDirectory}/${String(frame).padStart(3, '0')}.png`,
      omitBackground: true,
    });
  }
  process.stdout.write(`${style}/${icon}\n`);
}

await browser.close();
