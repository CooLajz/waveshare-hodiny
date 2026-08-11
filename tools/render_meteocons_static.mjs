import fs from 'node:fs/promises';
import { pathToFileURL } from 'node:url';

const [playwrightModule, source, destination] = process.argv.slice(2);
if (!playwrightModule || !source || !destination) {
  throw new Error(
    'Usage: node render_meteocons_static.mjs <playwright.mjs> <source.svg> <destination.png>',
  );
}

const { chromium } = await import(pathToFileURL(playwrightModule).href);
const svg = await fs.readFile(source, 'utf8');
const browser = await chromium.launch({
  executablePath: '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 84, height: 84 } });
await page.setContent(
  '<style>html,body,#icon{margin:0;width:84px;height:84px;background:transparent;overflow:hidden}svg{width:84px;height:84px}</style><div id="icon"></div>',
);
await page.locator('#icon').evaluate(
  (element, markup) => {
    element.innerHTML = markup;
    element.querySelectorAll('[fill="currentColor"]').forEach((node) =>
      node.setAttribute('fill', '#ffffff'),
    );
    element.querySelectorAll('[stroke="currentColor"]').forEach((node) =>
      node.setAttribute('stroke', '#ffffff'),
    );
  },
  svg,
);
await page.locator('#icon').screenshot({ path: destination, omitBackground: true });
await browser.close();
