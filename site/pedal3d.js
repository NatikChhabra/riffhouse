/* The Big Fuzz in the first screen, as a real model: a rounded steel box with its paint, three knobs,
   a footswitch, a lamp and two jacks, lit by a small studio. Measured from the app's own drawing of
   the pedal (620 by 777 face pixels; one unit here is 100 of them). */
import * as THREE from './vendor/three/three.module.min.js';
import { RoundedBoxGeometry } from './vendor/three/RoundedBoxGeometry.js';
import { RoomEnvironment } from './vendor/three/RoomEnvironment.js';

const W = 6.2, H = 7.77, D = 1.75, PLATE = 0.16;
const fx = (px) => px / 100 - W / 2, fy = (py) => H / 2 - py / 100;
const INK = { cream: '#e7dcc0', lower: '#d6cbae', band: '#53453a', bandText: '#fbf0d0', label: '#3b2c1e', dot: '#4f4330', faint: '#8f8266' };
/* Each knob: where it sits on the face, where it points at rest, and how far it turns when the fuzz comes on. */
const KNOBS = [[129, 123, -32, 128], [309, 123, 7, -34], [489, 123, 47, 30]];

function facePaint() {
  /* The printed face, drawn at 4 pixels per face pixel so the lettering stays sharp close up. */
  const S = 4, c = document.createElement('canvas');
  c.width = 620 * S; c.height = 777 * S;
  const g = c.getContext('2d');
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 8;
  t.userData.print = () => { g.setTransform(S, 0, 0, S, 0, 0); print(g); t.needsUpdate = true; };
  t.userData.print();
  return t;
}

function print(g) {
  /* The app sets this face about a fifth narrower than the font is cut, so the words are drawn large and
     squeezed to match. */
  const word = (text, x, y, px, align = 'center', gap = 0) => {
    g.save(); g.translate(x, y); g.scale(0.84, 1);
    g.font = `800 ${px}px "Big Shoulders", "Arial Narrow", sans-serif`; g.textAlign = align;
    if ('letterSpacing' in g) g.letterSpacing = gap + 'px';
    g.lineWidth = px * 0.032; g.lineJoin = 'round'; g.strokeStyle = g.fillStyle;
    g.strokeText(text, 0, 0); g.fillText(text, 0, 0); g.restore();
  };
  g.textBaseline = 'alphabetic';
  g.fillStyle = INK.cream; g.fillRect(0, 0, 620, 777);
  g.fillStyle = INK.lower; g.fillRect(0, 457, 620, 320);
  g.fillStyle = INK.band; g.fillRect(0, 309, 620, 148);
  g.fillStyle = INK.bandText; word('big fuzz', 310, 413, 90);
  g.fillStyle = INK.label; ['drive', 'tone', 'level'].forEach((t, i) => word(t, KNOBS[i][0], 243, 44));
  g.fillStyle = INK.dot;
  for (const [x, y] of KNOBS) for (let i = 0; i <= 10; i++) {
    const a = (-135 + i * 27) * Math.PI / 180, r = 75;
    g.beginPath(); g.arc(x + Math.sin(a) * r, y - Math.cos(a) * r, i % 10 === 0 ? 6 : i === 5 ? 4.6 : 3.6, 0, 7); g.fill();
  }
  /* Where a part meets the paint, the paint is a little darker: the light cannot get under it. */
  for (const [x, y, r] of [...KNOBS.map(([kx, ky]) => [kx, ky, 57]), [185, 598, 56], [445, 598, 19]]) {
    const shade = g.createRadialGradient(x, y, r * 0.86, x, y, r * 1.42);
    shade.addColorStop(0, 'rgba(48, 30, 8, .34)'); shade.addColorStop(1, 'rgba(48, 30, 8, 0)');
    g.fillStyle = shade; g.beginPath(); g.arc(x, y, r * 1.42, 0, 7); g.fill();
  }
  g.fillStyle = INK.faint;
  word('IN', 17, 300, 17, 'left', 2.5); word('OUT', 603, 300, 17, 'right', 2.5);
  word('RIFFHOUSE  B-10', 310, 733, 17, 'center', 3);
}

function grain(size, lo, hi, cell = 1) {
  /* Fine noise for the powder coat and the knob caps: it breaks the highlight the way real paint does. */
  const c = document.createElement('canvas'); c.width = c.height = size;
  const g = c.getContext('2d'), d = g.createImageData(size, size);
  let s = 7;
  const rnd = () => (s = (s * 16807) % 2147483647) / 2147483647;
  for (let y = 0; y < size; y += cell) for (let x = 0; x < size; x += cell) {
    const v = lo + rnd() * (hi - lo);
    for (let j = 0; j < cell; j++) for (let i = 0; i < cell; i++) {
      const k = ((y + j) * size + x + i) * 4; d.data[k] = d.data[k + 1] = d.data[k + 2] = v; d.data[k + 3] = 255;
    }
  }
  g.putImageData(d, 0, 0);
  const t = new THREE.CanvasTexture(c);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  return t;
}

function haloPaint() {
  /* The lamp's bloom: light falling off from the lens, added over whatever is behind it. */
  const c = document.createElement('canvas'); c.width = c.height = 128;
  const g = c.getContext('2d'), d = g.createImageData(128, 128);
  for (let y = 0; y < 128; y++) for (let x = 0; x < 128; x++) {
    const r = Math.hypot(x - 63.5, y - 63.5) / 64, v = Math.max(0, 1 - r) ** 2.4 * 255, k = (y * 128 + x) * 4;
    d.data[k] = d.data[k + 1] = d.data[k + 2] = v; d.data[k + 3] = 255;
  }
  g.putImageData(d, 0, 0);
  return new THREE.CanvasTexture(c);
}

const lathe = (pts, seg = 64) => new THREE.LatheGeometry(pts.map(([r, y]) => new THREE.Vector2(r, y)), seg);

export { print as printFace };

export function mount(canvas, opt = {}) {
  const { calm = false, phone = false, progress = null } = opt;
  const renderer = new THREE.WebGLRenderer({ canvas, alpha: true, antialias: true });
  renderer.setPixelRatio(Math.min(devicePixelRatio || 1, 2));
  renderer.toneMapping = THREE.NeutralToneMapping;
  renderer.toneMappingExposure = 1;
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = THREE.PCFSoftShadowMap;

  const scene = new THREE.Scene();
  const pmrem = new THREE.PMREMGenerator(renderer);
  scene.environment = pmrem.fromScene(new RoomEnvironment(), 0.04).texture;
  scene.environmentIntensity = 0.5;

  const camera = new THREE.PerspectiveCamera(24, 1, 10, 60);

  /* Studio: a warm key from the upper left that throws the shadow, a low fill, a cool rim from behind. */
  const key = new THREE.SpotLight(0xfff1dc, 900, 0, 0.6, 1, 2);
  key.position.set(-4, 12, 11);
  key.castShadow = true;
  key.shadow.mapSize.set(1024, 1024);
  key.shadow.camera.near = 6; key.shadow.camera.far = 30;
  key.shadow.radius = 3; key.shadow.bias = -0.0005; key.shadow.normalBias = 0.02;
  const fill = new THREE.DirectionalLight(0xffe2c0, 0.5); fill.position.set(8, -3, 10);
  const rim = new THREE.DirectionalLight(0xdfe8ff, 0.9); rim.position.set(6, 8, -6);
  scene.add(key, fill, rim);

  /* The shade the pedal throws on the page: its outline seen from the front, blurred in two passes and laid on a
     plane behind it, pushed along the light. Pressing the pedal brings it closer and tightens it. */
  const SH = 9, RT = phone ? 256 : 512;
  const shadowCam = new THREE.OrthographicCamera(-SH, SH, SH, -SH, 1, 60); shadowCam.position.z = 30;
  const rtA = new THREE.WebGLRenderTarget(RT, RT), rtB = new THREE.WebGLRenderTarget(RT, RT);
  const flat = new THREE.MeshBasicMaterial({ color: 0x000000 });
  const quad = new THREE.Mesh(new THREE.PlaneGeometry(2, 2), new THREE.ShaderMaterial({
    uniforms: { map: { value: null }, dir: { value: new THREE.Vector2() } },
    vertexShader: 'varying vec2 v; void main(){ v = uv; gl_Position = vec4(position.xy, 0., 1.); }',
    fragmentShader: 'uniform sampler2D map; uniform vec2 dir; varying vec2 v; void main(){ float a = 0., w = 0.; for (int i = -8; i <= 8; i++){ float f = float(i), k = exp(-f * f / 32.); a += texture2D(map, v + dir * f).a * k; w += k; } gl_FragColor = vec4(0., 0., 0., a / w); }',
    depthTest: false, depthWrite: false,
  }));
  const quadScene = new THREE.Scene(); quadScene.add(quad);
  const quadCam = new THREE.OrthographicCamera(-1, 1, 1, -1, 0, 1);
  const page = new THREE.Mesh(new THREE.PlaneGeometry(SH * 2, SH * 2), new THREE.ShaderMaterial({
    uniforms: { map: { value: rtA.texture }, strength: { value: 0.3 }, tint: { value: new THREE.Color(0x2b1200) } },
    vertexShader: 'varying vec2 v; void main(){ v = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.); }',
    fragmentShader: 'uniform sampler2D map; uniform float strength; uniform vec3 tint; varying vec2 v; void main(){ vec2 e = smoothstep(0., .12, v) * smoothstep(0., .12, 1. - v); gl_FragColor = vec4(tint, texture2D(map, v).a * strength * e.x * e.y); }',
    transparent: true, depthWrite: false, toneMapped: false,
  }));
  page.position.z = -2.2;
  scene.add(page);
  function throwShade(spread) {
    const bg = scene.background, env = scene.environment;
    page.visible = false; scene.overrideMaterial = flat;
    renderer.setRenderTarget(rtA); renderer.setClearColor(0x000000, 0); renderer.clear(); renderer.render(scene, shadowCam);
    scene.overrideMaterial = null; page.visible = true;
    for (let i = 0; i < 3; i++) {
      const step = spread * (i + 1) / RT;
      quad.material.uniforms.map.value = rtA.texture; quad.material.uniforms.dir.value.set(step, 0);
      renderer.setRenderTarget(rtB); renderer.clear(); renderer.render(quadScene, quadCam);
      quad.material.uniforms.map.value = rtB.texture; quad.material.uniforms.dir.value.set(0, step);
      renderer.setRenderTarget(rtA); renderer.clear(); renderer.render(quadScene, quadCam);
    }
    renderer.setRenderTarget(null);
  }

  const pedal = new THREE.Group();
  scene.add(pedal);
  const shade = (m) => { m.castShadow = true; m.receiveShadow = true; return m; };
  const paintGrain = grain(256, 96, 160); paintGrain.repeat.set(10, 12);

  /* Box: one rounded shell with the print projected straight down onto it, so the band wraps the sides. */
  const boxGeo = new RoundedBoxGeometry(W, H, D, 6, 0.16);
  const pos = boxGeo.attributes.position, uv = boxGeo.attributes.uv;
  for (let i = 0; i < pos.count; i++) uv.setXY(i, pos.getX(i) / W + 0.5, pos.getY(i) / H + 0.5);
  const face = facePaint();
  const paint = new THREE.MeshPhysicalMaterial({ map: face, roughness: 0.78, roughnessMap: paintGrain, bumpMap: paintGrain, bumpScale: 0.3, metalness: 0, specularIntensity: 0.45, clearcoat: 0.1, clearcoatRoughness: 0.55, envMapIntensity: 1 });
  const box = shade(new THREE.Mesh(boxGeo, paint));
  pedal.add(box);
  const plate = shade(new THREE.Mesh(new RoundedBoxGeometry(W - 0.16, H - 0.16, PLATE, 3, 0.07), new THREE.MeshStandardMaterial({ color: 0x24221f, roughness: 0.7, metalness: 0.4 })));
  plate.position.z = -D / 2 - PLATE / 2 + 0.02;
  pedal.add(plate);

  const top = D / 2;
  const black = new THREE.MeshPhysicalMaterial({ color: 0x0c0c0d, roughness: 0.32, clearcoat: 0.6, clearcoatRoughness: 0.25 });
  const steel = new THREE.MeshStandardMaterial({ color: 0xcfd0d3, metalness: 1, roughness: 0.3, envMapIntensity: 1.3 });
  const brushed = new THREE.MeshStandardMaterial({ color: 0xcfd0d2, metalness: 1, roughness: 0.36, envMapIntensity: 2 });
  const mark = new THREE.MeshStandardMaterial({ color: 0xf4ecd8, roughness: 0.6 });
  const darkMetal = new THREE.MeshStandardMaterial({ color: 0x343436, metalness: 0.9, roughness: 0.4, envMapIntensity: 2 });
  const chrome = new THREE.MeshStandardMaterial({ color: 0xeeeff1, metalness: 1, roughness: 0.1, envMapIntensity: 2.2 });

  /* Knobs: a black bell skirt with a cream pointer, a fluted dark collar and a domed chrome cap. */
  const skirtGeo = lathe([[0, 0], [0.555, 0], [0.57, 0.02], [0.565, 0.06], [0.53, 0.14], [0.47, 0.23], [0.4, 0.3], [0.34, 0.335], [0.3, 0.345], [0, 0.345]], 72);
  const fluteGeo = new THREE.CylinderGeometry(0.285, 0.3, 0.2, 144, 1, true);
  { const p = fluteGeo.attributes.position;
    for (let i = 0; i < p.count; i++) { const x = p.getX(i), z = p.getZ(i), k = 1 + 0.045 * Math.cos(24 * Math.atan2(z, x)); p.setX(i, x * k); p.setZ(i, z * k); }
    fluteGeo.computeVertexNormals(); }
  const domeGeo = lathe([[0.25, -0.03], [0.25, 0], [0.238, 0.03], [0.205, 0.062], [0.15, 0.085], [0.08, 0.097], [0, 0.1]], 64);
  const collarLid = new THREE.RingGeometry(0.2, 0.3, 64);
  const dials = [];
  for (const [px, py, deg] of KNOBS) {
    const k = new THREE.Group();
    k.position.set(fx(px), fy(py), top);
    k.rotation.z = -deg * Math.PI / 180;
    const skirt = shade(new THREE.Mesh(skirtGeo, black)); skirt.rotation.x = Math.PI / 2;
    const flute = shade(new THREE.Mesh(fluteGeo, darkMetal)); flute.rotation.x = Math.PI / 2; flute.position.z = 0.345 + 0.1;
    const lid = new THREE.Mesh(collarLid, darkMetal); lid.position.z = 0.345 + 0.2;
    const dome = shade(new THREE.Mesh(domeGeo, chrome)); dome.rotation.x = Math.PI / 2; dome.position.z = 0.345 + 0.22;
    const line = new THREE.Mesh(new THREE.BoxGeometry(0.052, 0.19, 0.03), mark); line.position.set(0, 0.462, 0.243); line.rotation.x = -0.89;
    k.add(skirt, flute, lid, dome, line);
    pedal.add(k); dials.push(k);
  }

  /* Footswitch: washer, hex nut, threaded collar and the plunger that sinks when it is pressed. */
  const sw = new THREE.Group(); sw.position.set(fx(185), fy(598), top);
  const washer = shade(new THREE.Mesh(new THREE.CylinderGeometry(0.56, 0.56, 0.03, 48), steel)); washer.rotation.x = Math.PI / 2; washer.position.z = 0.015;
  const nut = shade(new THREE.Mesh(new THREE.CylinderGeometry(0.5, 0.5, 0.14, 6), steel)); nut.rotation.x = Math.PI / 2; nut.rotation.y = 0.2; nut.position.z = 0.1;
  const collar = shade(new THREE.Mesh(fluteGeo, steel)); collar.rotation.x = Math.PI / 2; collar.scale.set(1.27, 0.7, 1.27); collar.position.z = 0.24;
  const collarTop = new THREE.Mesh(new THREE.RingGeometry(0.28, 0.37, 64), steel); collarTop.position.z = 0.31;
  const plunger = shade(new THREE.Mesh(lathe([[0.3, 0], [0.3, 0.17], [0.275, 0.235], [0.2, 0.28], [0.1, 0.297], [0, 0.3]], 64), chrome)); plunger.rotation.x = Math.PI / 2; plunger.position.z = 0.28;
  sw.add(washer, nut, collar, collarTop, plunger);
  pedal.add(sw);

  /* Lamp: a chrome bezel and an amber lens that lights when the fuzz is on. */
  const lamp = new THREE.Group(); lamp.position.set(fx(445), fy(598), top);
  const bezel = shade(new THREE.Mesh(lathe([[0.1, 0], [0.19, 0], [0.19, 0.05], [0.15, 0.09], [0.1, 0.09]], 40), steel)); bezel.rotation.x = Math.PI / 2;
  const lens = new THREE.Mesh(new THREE.SphereGeometry(0.115, 32, 16, 0, Math.PI * 2, 0, Math.PI / 2), new THREE.MeshPhysicalMaterial({ color: 0xb8640f, emissive: 0xff7a00, emissiveIntensity: 0.06, roughness: 0.12, clearcoat: 1 }));
  lens.rotation.x = Math.PI / 2; lens.position.z = 0.06;
  const glow = new THREE.PointLight(0xff8a1a, 0, 3.4, 2); glow.position.z = 0.4;
  const halo = new THREE.Sprite(new THREE.SpriteMaterial({ map: haloPaint(), color: 0xff7d1f, blending: THREE.AdditiveBlending, depthWrite: false, depthTest: false, transparent: true, opacity: 0 }));
  halo.position.z = 0.2; halo.scale.setScalar(1.25);
  lamp.add(bezel, lens, glow, halo);
  pedal.add(lamp);

  /* Jacks on both sides and the power socket on the top edge. */
  for (const side of [-1, 1]) {
    const j = new THREE.Group(); j.position.set(side * W / 2, fy(164), -0.1); j.rotation.z = -side * Math.PI / 2;
    const jn = shade(new THREE.Mesh(new THREE.CylinderGeometry(0.33, 0.33, 0.1, 6), steel)); jn.position.y = 0.05;
    const sleeve = shade(new THREE.Mesh(new THREE.CylinderGeometry(0.22, 0.22, 0.3, 32), brushed)); sleeve.position.y = 0.2;
    const hole = new THREE.Mesh(new THREE.CylinderGeometry(0.13, 0.13, 0.02, 24), new THREE.MeshBasicMaterial({ color: 0x050505 })); hole.position.y = 0.355;
    j.add(jn, sleeve, hole);
    pedal.add(j);
  }
  const dc = new THREE.Group(); dc.position.set(0, H / 2, -0.15);
  const dcBody = shade(new THREE.Mesh(new THREE.CylinderGeometry(0.26, 0.26, 0.12, 32), black)); dcBody.position.y = 0.05;
  const dcHole = new THREE.Mesh(new THREE.CylinderGeometry(0.14, 0.14, 0.02, 24), new THREE.MeshBasicMaterial({ color: 0x050505 })); dcHole.position.y = 0.115;
  dc.add(dcBody, dcHole);
  pedal.add(dc);

  /* State. Everything eases toward a target, so pointer, press and scroll can all pull at once. */
  const REST = { x: -0.17, y: -0.27, z: 0.09 };
  const want = { tx: 0, ty: 0, scroll: 0, down: 0, on: 0 };
  const now = { tx: 0, ty: 0, scroll: 0, down: 0, on: 0, turn: 0 };
  let t = 0, running = false, drew = false;
  const clock = new THREE.Clock();
  const ease = (a, b, k) => a + (b - a) * k;

  function size() {
    const w = canvas.clientWidth, h = canvas.clientHeight;
    if (!w || !h) return;
    renderer.setSize(w, h, false);
    camera.aspect = w / h;
    /* The canvas is one and a half times as tall as the button and sits right of and below its centre
       (styles.css, .pedal-live), so the camera frames 1.5 pedal heights and stands off by the same shift. */
    camera.position.set(0.2 * W, -0.15 * H, H * 1.5 / (2 * Math.tan(camera.fov * Math.PI / 360)));
    camera.updateProjectionMatrix();
  }

  function draw() {
    const dt = Math.min(clock.getDelta(), 0.05);
    t += dt;
    const k = calm ? 1 : 1 - Math.pow(0.0015, dt);
    now.tx = ease(now.tx, want.tx, k); now.ty = ease(now.ty, want.ty, k);
    now.scroll = ease(now.scroll, want.scroll, calm ? 1 : 1 - Math.pow(0.00001, dt));
    now.down = ease(now.down, want.down, calm ? 1 : 1 - Math.pow(1e-9, dt));
    now.on = ease(now.on, want.on, calm ? 1 : 1 - Math.pow(1e-6, dt));
    now.turn = ease(now.turn, want.on, calm ? 1 : 1 - Math.pow(0.004, dt));
    dials.forEach((k, i) => { k.rotation.z = -(KNOBS[i][2] + KNOBS[i][3] * now.turn) * Math.PI / 180; });
    if (progress && !calm) want.scroll = progress();
    const sway = calm ? 0 : Math.sin(t * 0.55);
    pedal.rotation.set(
      REST.x + now.ty * 0.16 + now.scroll * -0.85 + sway * 0.012,
      REST.y + now.tx * 0.24 + now.scroll * -0.5 + sway * 0.05,
      REST.z + now.scroll * -0.18
    );
    pedal.position.z = -now.down * 0.22 - now.scroll * 1.2;
    pedal.position.y = sway * 0.05;
    plunger.position.z = 0.28 - now.down * 0.13;
    lens.material.emissiveIntensity = 0.06 + now.on * 1.5;
    glow.intensity = now.on * 1.1; halo.material.opacity = now.on * 0.5;
    const lift = 1 - now.down * 0.45;
    page.position.x = 1.25 * lift + now.tx * -0.3; page.position.y = -1.55 * lift + now.ty * 0.3;
    page.material.uniforms.strength.value = 0.34 * (1 + now.down * 0.25) * (1 - now.scroll * 0.6);
    throwShade(3.2 * lift);
    renderer.render(scene, camera);
    if (!drew) { drew = true; canvas.dispatchEvent(new Event('pedal-live')); }
  }

  const api = {
    tilt(x, y) { want.tx = x; want.ty = y; if (!running) draw(); },
    press(down) { want.down = down ? 1 : 0; if (!running) draw(); },
    light(on) { want.on = on ? 1 : 0; if (!running) draw(); },
    start() { if (running || calm) { draw(); return; } running = true; clock.getDelta(); renderer.setAnimationLoop(draw); },
    stop() { running = false; renderer.setAnimationLoop(null); },
    resize() { size(); if (!running) draw(); },
    pose(o) { Object.assign(REST, o); draw(); },
  };
  /* The lettering needs the page's display face. If it was not in yet, print again once it is. */
  const FACE = '800 96px "Big Shoulders"';
  if (document.fonts && !document.fonts.check(FACE)) document.fonts.load(FACE).then(() => { face.userData.print(); if (!running) draw(); }, () => {});
  size();
  return api;
}
