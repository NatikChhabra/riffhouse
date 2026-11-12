// Renders the stills that stand in for the 3D pedal in the first screen: hero-pedal.webp (fuzz off) and
// hero-pedal-on.webp (fuzz on), from the same model the page draws (site/pedal3d.js), at the same framing.
// The page shows the still until the model is live, and keeps it where there is no WebGL.
//
//   cd site && python3 -m http.server 8765 &
//   node tools/site/hero_pedal.mjs            # needs playwright and Pillow (python3 -m pip install pillow)
//
// Run it again whenever pedal3d.js changes how the pedal looks at rest.
import { createRequire } from 'node:module';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const { chromium } = createRequire(import.meta.url)('playwright');
const base = process.argv[2] || 'http://localhost:8765';
const img = path.join(path.dirname(fileURLToPath(import.meta.url)), '..', '..', 'site', 'img');
// The canvas is 170% by 150% of a 620 by 777 button (styles.css, .pedal-live): 1054 by 1166, drawn here at 980 wide.
const W = 980, H = 1084;

const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
const page = await browser.newPage({ viewport: { width: W, height: H }, deviceScaleFactor: 1 });
await page.goto(base + '/roadmap.html');
await page.evaluate(async ([w, h]) => {
  document.documentElement.style.background = 'none';
  document.body.style.cssText = 'margin:0;background:none';
  document.body.innerHTML = `<canvas style="display:block;width:${w}px;height:${h}px"></canvas>`;
  await document.fonts.load('800 96px "Big Shoulders"');
  const { mount } = await import('./pedal3d.js');
  window.rig = mount(document.querySelector('canvas'), { calm: true });
  window.rig.start();
}, [W, H]);
for (const [name, on] of [['hero-pedal', false], ['hero-pedal-on', true]]) {
  await page.evaluate((lit) => window.rig.light(lit), on);
  await page.waitForTimeout(300);
  const png = path.join(img, name + '.png');
  await page.screenshot({ path: png, omitBackground: true });
  execFileSync('python3', ['-c', 'import sys\nfrom PIL import Image\nImage.open(sys.argv[1]).save(sys.argv[2], quality=88, method=6)\n', png, path.join(img, name + '.webp')]);
  execFileSync('rm', [png]);
  console.log('wrote', name + '.webp');
}
await browser.close();
