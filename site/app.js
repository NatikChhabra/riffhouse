/* Riffhouse website. No libraries: the page reads fine without this file. */
(() => {
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
      $('[data-rig-name]', root).textContent = t.n;
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
    const fine = matchMedia('(hover: hover) and (pointer: fine)').matches;
    let peek = 0;
    const few = () => (matchMedia('(max-width: 699px)').matches ? 6 : 12);
    let all = false;
    const draw = () => {
      const q = search.value.trim().toLowerCase();
      const hits = tones.filter((t) => (kind === 'All' || t.c === kind || t.g.includes(kind))
        && (!q || `${t.n} ${t.a} ${t.g.join(' ')} ${t.d} ${t.c}`.toLowerCase().includes(q)));
      list.replaceChildren(...(all ? hits : hits.slice(0, few())).map((t) => {
        const b = el('button', { type: 'button', className: 'tone' }, [
          el('span', { className: 'tone-n', textContent: t.n }), el('span', { className: 'tone-d', textContent: t.d })]);
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
    try { const r = await fetch('demos.json'); if (!r.ok) return; clips = await r.json(); } catch { return; }
    if (!Array.isArray(clips) || !clips.length) return;
    const audio = new Audio();
    let live = null;
    const icon = (playing) => playing
      ? '<svg viewBox="0 0 20 20" aria-hidden="true"><path fill="currentColor" d="M4 3h4v14H4zM12 3h4v14h-4z"/></svg>'
      : '<svg viewBox="0 0 20 20" aria-hidden="true"><path fill="currentColor" d="M5 2.5v15l12-7.5z"/></svg>';
    const set = (b, playing) => { b.innerHTML = icon(playing); b.setAttribute('aria-pressed', playing); };
    $('[data-demo-list]', sec).replaceChildren(...clips.map((c) => {
      const b = el('button', { type: 'button' });
      b.setAttribute('aria-label', `Play ${c.name}`);
      set(b, false);
      b.addEventListener('click', () => {
        if (live === b && !audio.paused) { audio.pause(); set(b, false); return; }
        if (live) set(live, false);
        live = b; audio.src = c.file; audio.play(); set(b, true);
      });
      return el('li', {}, [b, el('b', { textContent: c.name }), el('span', { textContent: c.note || '' })]);
    }));
    audio.addEventListener('ended', () => live && set(live, false));
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

  /* The pedal in the first screen: it leans toward the pointer, and pressing it puts fuzz on the headline */
  function stomp() {
    const pedal = $('[data-stomp]'), hero = pedal && pedal.closest('.hero');
    if (!pedal) return;
    const say = $('[data-stomp-say]'), map = $('#fuzz feDisplacementMap'), small = $('#fuzz-s feDisplacementMap');
    const page = document.documentElement;
    const calm = matchMedia('(prefers-reduced-motion: reduce)').matches;
    let on = false, raf = 0;
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
    pedal.addEventListener('click', () => {
      on = !on;
      pedal.setAttribute('aria-pressed', on);
      page.classList.add('is-fuzz');
      if (say) say.textContent = on ? 'Fuzz on. Press again for clean.' : 'Press the pedal for fuzz.';
      pedal.classList.add('is-down');
      setTimeout(() => pedal.classList.remove('is-down'), 170);
      settle(on ? Math.max(3, parseFloat(getComputedStyle($('h1', hero)).fontSize) * 0.045) : 0);
      if (!on) setTimeout(() => { if (!on) page.classList.remove('is-fuzz'); }, calm ? 0 : 560);
    });
    if (calm || !matchMedia('(hover: hover) and (pointer: fine)').matches) return;
    let queued = false;
    hero.addEventListener('pointermove', (e) => {
      if (queued) return;
      queued = true;
      requestAnimationFrame(() => {
        queued = false;
        const r = pedal.getBoundingClientRect();
        const x = (e.clientX - (r.left + r.width / 2)) / innerWidth, y = (e.clientY - (r.top + r.height / 2)) / innerHeight;
        hero.style.setProperty('--tx', Math.max(-1, Math.min(1, x * 2.4)).toFixed(3));
        hero.style.setProperty('--ty', Math.max(-1, Math.min(1, y * 2.4)).toFixed(3));
      });
    });
    hero.addEventListener('pointerleave', () => { hero.style.setProperty('--tx', 0); hero.style.setProperty('--ty', 0); });
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

  const SPONSOR_URL = '';
  const sponsor = $('[data-sponsor]');
  if (sponsor && SPONSOR_URL) { sponsor.href = SPONSOR_URL; sponsor.hidden = false; }

  tabs(); reveals(); download(); finder(); demos(); copyId(); stomp(); counts(); play();
})();
