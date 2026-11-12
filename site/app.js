/* Riffhouse website. The page reads fine without this file. Its one library is three.js, kept in vendor/
   and fetched after the page has loaded, for the pedal in the first screen. */
(() => {
  const VER = document.currentScript && document.currentScript.src.includes('?') ? '?' + document.currentScript.src.split('?')[1] : '';
  const REPO = 'NatikChhabra/riffhouse';
  const $ = (s, el = document) => el.querySelector(s);
  const $$ = (s, el = document) => [...el.querySelectorAll(s)];
  const calm = matchMedia('(prefers-reduced-motion: reduce)').matches;
  const el = (tag, props = {}, kids = []) => {
    const n = Object.assign(document.createElement(tag), props);
    kids.forEach((k) => n.append(k));
    return n;
  };

  /* The download button goes straight to the newest public installer. */
  async function download() {
    const note = $('[data-release]');
    try {
      const res = await fetch(`https://api.github.com/repos/${REPO}/releases?per_page=20`, { headers: { Accept: 'application/vnd.github+json' } });
      if (!res.ok) return;
      const releases = (await res.json()).filter((r) => !r.draft && !r.prerelease);
      for (const r of releases) {
        const file = r.assets.find((a) => /windows-setup\.exe$/i.test(a.name));
        if (!file) continue;
        $$('[data-download]').forEach((a) => { a.href = file.browser_download_url; });
        const mb = Math.round(file.size / 1048576);
        const day = new Date(r.published_at).toLocaleDateString('en-GB', { day: 'numeric', month: 'long', year: 'numeric' });
        if (note) note.textContent = `Version ${r.tag_name.replace(/^v/, '')}, ${mb} MB, released ${day}.`;
        return;
      }
      if (note) note.textContent = releases.length
        ? `Version ${releases[0].tag_name.replace(/^v/, '')} is out. Its installer is still being built, try again in a few minutes.`
        : 'The first public version is in final testing.';
    } catch { /* offline or rate limited: the link to GitHub Releases still works */ }
  }

  /* Tone finder */
  async function finder() {
    const root = $('[data-finder]');
    if (!root) return;
    let tones;
    try { tones = await (await fetch('tones.json')).json(); } catch { return; }
    const list = $('[data-list]', root), cats = $('[data-cats]', root), line = $('[data-count-line]', root);
    const search = $('[data-search]', root), chain = $('[data-chain]', root);
    const kinds = ['All', 'Inspired By', 'Essentials', 'Clean', 'Crunch', 'Rock', 'Blues', 'Metal', 'Lead', 'Funk', 'Ambient', 'Lo-Fi', 'Bass'];
    let kind = 'All', current = tones.find((t) => t.n === 'First Light Clean') || tones[0];

    const show = (t, fresh) => {
      current = t;
      $('[data-rig-name]', root).textContent = t.w ? `${t.w}: ${t.t}` : t.n;
      $('[data-rig-desc]', root).textContent = t.d;
      $('[data-rig-kind]', root).textContent = [t.c, ...t.g].join(', ');
      chain.replaceChildren(...t.r.map(([slot, i, name], n) => {
                const li = el('li');
        li.style.setProperty('--i', Math.min(n, 5));
        li.style.setProperty('--w', slot === 'amp' ? 2.5 : slot === 'cab' ? 1.7 : 1);
        const hasArt = !(slot === 'cab' && i > 7);
        if (hasArt) li.append(el('img', { src: `img/gear/${slot}-${i}-s.webp`, alt: '', loading: 'lazy' }));
        li.append(el('span', { textContent: name }));
        return li;
      }));
      if (fresh && !calm) { chain.classList.remove('is-fresh'); void chain.offsetWidth; chain.classList.add('is-fresh'); }
      $$('.tone', list).forEach((b) => {
        const on = b.dataset.n === t.n;
        b.classList.toggle('is-on', on);
        b.setAttribute('aria-pressed', on);
      });
    };

    const more = $('[data-more]', root);
    /* A tone made after someone shows who first, the way the app lists it: the band, then the title */
    const label = (t) => (t.w ? [el('b', { textContent: t.w }), ` ${t.t}`] : [t.n]);
    const fine = matchMedia('(hover: hover) and (pointer: fine)').matches;
    let peek = 0;
    const few = () => (matchMedia('(max-width: 699px)').matches ? 5 : 12);
    let all = false;
    const draw = () => {
      const q = search.value.trim().toLowerCase();
      const hits = tones.filter((t) => (kind === 'All' || t.c === kind || t.g.includes(kind))
        && (!q || `${t.n} ${t.a} ${t.b || ''} ${t.s || ''} ${t.g.join(' ')} ${t.d} ${t.c}`.toLowerCase().includes(q)));
      list.replaceChildren(...(all ? hits : hits.slice(0, few())).map((t) => {
        const b = el('button', { type: 'button', className: 'tone' }, [
          el('span', { className: 'tone-n' }, label(t)), el('span', { className: 'tone-d', textContent: (t.x || '') + t.d })]);
        b.dataset.n = t.n;
        b.setAttribute('aria-pressed', t.n === current.n);
        b.classList.toggle('is-on', t.n === current.n);
        b.addEventListener('click', () => show(t, true));
        if (fine) b.addEventListener('pointerenter', () => { clearTimeout(peek); peek = setTimeout(() => { if (current !== t) show(t, true); }, 90); });
        return el('li', {}, [b]);
      }));
      line.textContent = hits.length === 0 ? 'No tone matches that. Try a genre, like blues or metal.'
        : hits.length === 1 ? '1 tone' : `${hits.length} tones`;
      more.hidden = hits.length <= few();
      more.textContent = all ? 'Show fewer' : `Show all ${hits.length}`;
      more.setAttribute('aria-expanded', all);
    };
    more.addEventListener('click', () => { all = !all; draw(); if (!all) root.scrollIntoView({ block: 'nearest' }); });

    cats.replaceChildren(...kinds.map((k) => {
      const b = el('button', { type: 'button', textContent: k === 'Inspired By' ? 'Inspired by' : k });
      b.setAttribute('aria-pressed', k === kind);
      b.addEventListener('click', () => {
        kind = k;
        $$('button', cats).forEach((o) => o.setAttribute('aria-pressed', o === b));
        draw();
      });
      return b;
    }));
    search.addEventListener('input', draw);
    draw();
    show(current, false);
  }

  /* Record and practise: tabs with arrow keys */
  function tabs() {
    $$('[data-switch]').forEach((root) => {
      const all = $$('[role="tab"]', root);
      const pick = (tab, focus) => {
        all.forEach((t) => {
          const on = t === tab;
          t.setAttribute('aria-selected', on);
          t.tabIndex = on ? 0 : -1;
          $(`#${t.getAttribute('aria-controls')}`).hidden = !on;
        });
        if (focus) tab.focus();
      };
      all.forEach((t, i) => {
        t.addEventListener('click', () => pick(t));
        t.addEventListener('keydown', (e) => {
          const step = { ArrowRight: 1, ArrowLeft: -1 }[e.key];
          if (step) { e.preventDefault(); pick(all[(i + step + all.length) % all.length], true); }
          if (e.key === 'Home') { e.preventDefault(); pick(all[0], true); }
          if (e.key === 'End') { e.preventDefault(); pick(all[all.length - 1], true); }
        });
      });
      /* The pictures behind the other tabs load as the section comes near, so a first press never shows an empty frame */
      if ('IntersectionObserver' in window && !(navigator.connection && navigator.connection.saveData)) {
        const io = new IntersectionObserver(([e]) => {
          if (!e.isIntersecting) return;
          io.disconnect();
          $$('img[loading="lazy"]', root).forEach((im) => { im.loading = 'eager'; });
        }, { rootMargin: '600px 0px' });
        io.observe(root);
      }
    });
  }

  /* Sections open once, when they are reached */
  /* The tall top margin counts anything already scrolled past as seen, so a fast scroll never leaves a blank. */
  function reveals() {
    const items = $$('[data-reveal]');
    if (!('IntersectionObserver' in window)) { items.forEach((n) => n.classList.add('is-in')); return; }
    const io = new IntersectionObserver((seen) => seen.forEach((s) => {
      if (s.isIntersecting) { s.target.classList.add('is-in'); io.unobserve(s.target); }
    }), { rootMargin: '100000px 0px -10% 0px', threshold: 0.1 });
    items.forEach((n) => io.observe(n));
  }

  /* Sound demos: the section stays hidden until demos.json lists real clips */
  async function demos() {
    const sec = $('[data-demos]');
    if (!sec) return;
    let clips;
    try { const r = await fetch('demos.json' + VER); if (!r.ok) return; clips = await r.json(); } catch { return; }
    if (!Array.isArray(clips) || !clips.length) return;
    const play = $('[data-deck-play]', sec), wave = $('[data-deck-wave]', sec), seek = $('input', wave);
    const time = $('[data-deck-time]', sec), note = $('[data-deck-note]', sec), lines = $$('path', wave);
    const len = clips[0].seconds || 20;
    /* Every clip is the same take, so each tone gets its own player and they all keep one place in the music */
    const players = clips.map((c) => Object.assign(new Audio(), { preload: 'none', loop: true, src: c.file + VER }));
    let cur = 0, on = false, frame = 0, warm = false;
    const icon = (playing) => playing
      ? '<svg viewBox="0 0 20 20" aria-hidden="true"><path fill="currentColor" d="M4 3h4v14H4zM12 3h4v14h-4z"/></svg>'
      : '<svg viewBox="0 0 20 20" aria-hidden="true"><path fill="currentColor" d="M5 2.5v15l12-7.5z"/></svg>';
    const clock = (s) => `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, '0')}`;
    const chips = clips.map((c, i) => {
      const b = el('button', { type: 'button', textContent: c.name });
      b.addEventListener('click', () => start(i));
      return b;
    });
    const place = () => {
      const t = Math.min(players[cur].currentTime, len);
      wave.style.setProperty('--p', (t / len).toFixed(4));
      seek.value = t;
      const say = `${clock(t)} / ${clock(len)}`;
      if (time.textContent !== say) time.textContent = say;
    };
    const tick = () => { place(); frame = on ? requestAnimationFrame(tick) : 0; };
    const show = () => {
      play.innerHTML = icon(on);
      play.setAttribute('aria-label', on ? 'Pause' : 'Play');
      chips.forEach((b, i) => b.setAttribute('aria-pressed', i === cur));
      /* The bars are the real shape of each clip: a clean tone has tall peaks, a driven one is squashed flat */
      const d = (clips[cur].peaks || []).map((h, i) => `M${i + 0.5} ${50 - h / 2}V${50 + h / 2}`).join('');
      lines.forEach((p) => { p.setAttribute('d', d); p.style.setProperty('d', `path("${d}")`); });
      note.textContent = clips[cur].note || '';
      place();
    };
    function start(i) {
      const t = players[cur].currentTime;
      if (i !== cur) {
        const next = players[i];
        players[cur].pause();
        next.currentTime = t;
        /* A player that has not loaded yet forgets the place it was given, so it gets it again once it knows its length */
        if (next.readyState < 1) next.addEventListener('loadedmetadata', () => { next.currentTime = t; }, { once: true });
        cur = i;
      }
      on = true;
      players[cur].play().catch(() => { on = false; show(); });
      if (!warm) {
        warm = true;
        if (!(navigator.connection && navigator.connection.saveData)) players.forEach((a, k) => { if (k !== cur) { a.preload = 'auto'; a.load(); } });
      }
      show();
      if (!frame) tick();
    }
    play.addEventListener('click', () => {
      if (!on) { start(cur); return; }
      on = false; players[cur].pause(); show();
    });
    seek.max = len;
    seek.addEventListener('input', () => { players[cur].currentTime = Number(seek.value); place(); });
    $('[data-deck-tones]', sec).replaceChildren(...chips);
    show();
    sec.hidden = false;
  }

  /* The support link appears once a real page exists for it */
  function copyId() {
    const btn = $('[data-copy]'), id = $('[data-upi]'), said = $('[data-copied]');
    if (!btn || !id || !navigator.clipboard) return;
    btn.hidden = false;
    btn.addEventListener('click', () => {
      navigator.clipboard.writeText(id.textContent.trim()).then(
        () => { said.textContent = 'Copied. Paste it into any UPI app.'; },
        () => { said.textContent = 'Could not copy. Select the ID and copy it by hand.'; }
      );
    });
  }

  /* The pedal in the first screen: a real model that leans toward the pointer, sinks when pressed and puts
     fuzz on the headline. The still in the page stands in until the model is drawn, and for good when
     there is no WebGL or the visitor asked to save data. */
  function stomp() {
    const pedal = $('[data-stomp]'), hero = pedal && pedal.closest('.hero');
    if (!pedal) return;
    const say = $('[data-stomp-say]'), map = $('#fuzz feDisplacementMap'), small = $('#fuzz-s feDisplacementMap');
    const still = $('img', pedal), canvas = $('canvas', pedal);
    const page = document.documentElement;
    const calm = matchMedia('(prefers-reduced-motion: reduce)').matches;
    let on = false, raf = 0, rig = null, up = 0;
    const settle = (to) => {
      cancelAnimationFrame(raf);
      if (calm) { map.setAttribute('scale', to); small.setAttribute('scale', to * 0.4); return; }
      const t0 = performance.now(), peak = on ? to * 3.6 : 0, from = Number(map.getAttribute('scale'));
      const step = (now) => {
        const k = Math.min(1, (now - t0) / 520);
        const kick = on ? Math.sin(k * Math.PI) * peak * (1 - k) : 0;
        const v = from + (to - from) * k + kick;
        map.setAttribute('scale', v.toFixed(1));
        small.setAttribute('scale', (v * 0.4).toFixed(1));
        if (k < 1) raf = requestAnimationFrame(step);
      };
      raf = requestAnimationFrame(step);
    };
    const press = (down) => {
      clearTimeout(up);
      if (rig) rig.press(down);
      if (down) up = setTimeout(() => press(false), 600);
    };
    pedal.addEventListener('pointerdown', () => press(true));
    pedal.addEventListener('pointerleave', () => press(false));
    pedal.addEventListener('pointercancel', () => press(false));
    pedal.addEventListener('click', () => {
      on = !on;
      pedal.setAttribute('aria-pressed', on);
      page.classList.add('is-fuzz');
      if (say) say.textContent = on ? 'Fuzz on. Press again for clean.' : 'Press the pedal for fuzz.';
      press(true);
      clearTimeout(up);
      up = setTimeout(() => press(false), 150);
      if (rig) rig.light(on);
      else if (still) still.src = on ? 'img/hero-pedal-on.webp' : 'img/hero-pedal.webp';
      settle(on ? Math.max(3, parseFloat(getComputedStyle($('h1', hero)).fontSize) * 0.045) : 0);
      if (!on) setTimeout(() => { if (!on) page.classList.remove('is-fuzz'); }, calm ? 0 : 560);
    });

    const wake = () => {
      if (!canvas || !window.WebGL2RenderingContext || (navigator.connection && navigator.connection.saveData)) return;
      import('./pedal3d.js' + VER).then(({ mount }) => {
        canvas.addEventListener('pedal-live', () => pedal.classList.add('is-live'), { once: true });
        canvas.addEventListener('webglcontextlost', () => { pedal.classList.remove('is-live'); rig = null; still.src = on ? 'img/hero-pedal-on.webp' : 'img/hero-pedal.webp'; });
        rig = mount(canvas, { calm, phone: innerWidth < 700, progress: () => Math.min(1, scrollY / innerHeight) });
        rig.light(on);
        let seen = true;
        const run = () => { if (!rig) return; if (seen && !document.hidden) rig.start(); else rig.stop(); };
        if ('IntersectionObserver' in window) new IntersectionObserver(([e]) => { seen = e.isIntersecting; run(); }).observe(canvas);
        document.addEventListener('visibilitychange', run);
        if ('ResizeObserver' in window) new ResizeObserver(() => rig && rig.resize()).observe(canvas);
        run();
      }).catch(() => {});
    };
    if (document.readyState === 'complete') wake(); else addEventListener('load', wake, { once: true });

    if (calm || !matchMedia('(hover: hover) and (pointer: fine)').matches) return;
    let queued = false;
    hero.addEventListener('pointermove', (e) => {
      if (queued) return;
      queued = true;
      requestAnimationFrame(() => {
        queued = false;
        const r = pedal.getBoundingClientRect();
        const x = (e.clientX - (r.left + r.width / 2)) / innerWidth, y = (e.clientY - (r.top + r.height / 2)) / innerHeight;
        const tx = Math.max(-1, Math.min(1, x * 2.4)), ty = Math.max(-1, Math.min(1, y * 2.4));
        hero.style.setProperty('--tx', tx.toFixed(3));
        hero.style.setProperty('--ty', ty.toFixed(3));
        if (rig) rig.tilt(tx, ty);
      });
    });
    hero.addEventListener('pointerleave', () => { hero.style.setProperty('--tx', 0); hero.style.setProperty('--ty', 0); if (rig) rig.tilt(0, 0); });
  }

  /* Numbers that count up when they are first seen, and grids whose pieces land one after another */
  function counts() {
    $$('[data-reveal="pop"]').forEach((list) => {
      const cols = getComputedStyle(list).gridTemplateColumns.split(' ').length || 1;
      [...list.children].forEach((li, i) => li.style.setProperty('--i', i % cols));
    });
    if (matchMedia('(prefers-reduced-motion: reduce)').matches || !('IntersectionObserver' in window)) return;
    $$('[data-count]').forEach((el) => {
      const out = $('[data-count-out]', el), end = Number(el.dataset.count), unit = out.textContent.replace(/^[0-9.]+/, '');
      const io = new IntersectionObserver((seen) => {
        if (!seen[0].isIntersecting) return;
        io.disconnect();
        const t0 = performance.now();
        const step = (now) => {
          const k = Math.min(1, (now - t0) / 900), eased = 1 - Math.pow(1 - k, 3);
          out.textContent = Math.round(end * eased) + unit;
          if (k < 1) requestAnimationFrame(step);
        };
        requestAnimationFrame(step);
      }, { threshold: 0.6 });
      io.observe(el);
    });
  }

  /* Small pleasures for a mouse: the main button leans toward the pointer, the big name ripples under it, pedals press */
  function play() {
    $$('.rail-row li:not(.rail-own), .amp-row li:not(.rail-own)').forEach((li) => {
      li.addEventListener('click', () => {
        li.classList.remove('is-hit');
        void li.offsetWidth;
        li.classList.add('is-hit');
      });
    });
    const word = $('[data-push]');
    if (matchMedia('(prefers-reduced-motion: reduce)').matches || !matchMedia('(hover: hover) and (pointer: fine)').matches) return;
    if (word) {
      /* the word stays one piece of text until a pointer first reaches it */
      let letters = [], queued = false;
      word.addEventListener('pointerenter', () => {
        if (letters.length) return;
        word.replaceChildren(...[...word.textContent].map((ch) => el('b', { textContent: ch })));
        letters = [...word.children];
      });
      word.addEventListener('pointermove', (e) => {
        if (queued || !letters.length) return;
        queued = true;
        requestAnimationFrame(() => {
          queued = false;
          letters.forEach((l) => {
            const r = l.getBoundingClientRect();
            const d = Math.abs(e.clientX - (r.left + r.width / 2)) / (r.width * 2.2);
            l.style.setProperty('--lift', `${(-9 * Math.max(0, 1 - d) ** 2).toFixed(2)}%`);
          });
        });
      });
      word.addEventListener('pointerleave', () => letters.forEach((l) => l.style.setProperty('--lift', '0%')));
    }
    $$('.hero .btn, .get .btn').forEach((btn) => {
      btn.addEventListener('pointermove', (e) => {
        const r = btn.getBoundingClientRect();
        btn.style.setProperty('--mx', `${(((e.clientX - r.left) / r.width - 0.5) * 10).toFixed(1)}px`);
        btn.style.setProperty('--my', `${(((e.clientY - r.top) / r.height - 0.5) * 8).toFixed(1)}px`);
      });
      btn.addEventListener('pointerleave', () => { btn.style.setProperty('--mx', '0px'); btn.style.setProperty('--my', '0px'); });
    });
  }


  tabs(); reveals(); download(); finder(); demos(); copyId(); stomp(); counts(); play();
})();
