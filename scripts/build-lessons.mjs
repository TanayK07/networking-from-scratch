#!/usr/bin/env node
// SSG build: generates static HTML for each lesson with unique SEO metadata
// and pre-rendered markdown content for Googlebot.

import { readFileSync, writeFileSync, mkdirSync, existsSync } from 'fs';
import { join, dirname, relative } from 'path';
import { Marked } from 'marked';

const ROOT = join(import.meta.dirname, '..');
const PHASES_DIR = join(ROOT, 'phases');
const WEBSITE_DIR = join(ROOT, 'website');
const OUT_DIR = join(WEBSITE_DIR, 'lessons');
const SITE = 'https://networkingfromscratch.vercel.app';

const marked = new Marked({ gfm: true, breaks: false });

// ---------- Parse data.js by evaluating it in a minimal sandbox ----------

function parseDataJs() {
  const src = readFileSync(join(WEBSITE_DIR, 'js', 'data.js'), 'utf8');

  const sandbox = { window: { NFS: {} } };
  sandbox.NFS = sandbox.window.NFS;
  const fn = new Function('window', 'NFS', 'slugify', 'L', src + '\nreturn window.NFS;');

  function slugifyLocal(num, title) {
    const s = title.toLowerCase()
      .replace(/[^a-z0-9\s-]/g, '')
      .replace(/\s+/g, '-')
      .replace(/-+/g, '-')
      .replace(/^-|-$/g, '');
    return String(num).padStart(2, '0') + '-' + s;
  }
  function L(num, title, lang, type, explicitSlug) {
    return [title, lang, type, explicitSlug || slugifyLocal(num, title)];
  }

  const nfs = fn(sandbox.window, sandbox.window.NFS, slugifyLocal, L);

  return nfs.PHASES.map(p => ({
    id: p.id,
    title: p.title,
    slug: p.slug,
    desc: p.desc,
    lessons: p.lessons.map((l, i) => ({
      num: i + 1,
      title: l[0],
      lang: l[1],
      type: l[2],
      slug: l[3]
    }))
  }));
}

const LANG_LABELS = { c: 'C', py: 'Python', bpf: 'eBPF', lua: 'Lua' };
const TYPE_LABELS = { theory: 'Theory', implement: 'Build', capstone: 'Capstone' };

// ---------- HTML template ----------

function escHtml(s) {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function buildPage({ phase, phaseIdx, lesson, lessonIdx, contentHtml, phases }) {
  const num = String(lessonIdx + 1).padStart(2, '0');
  const pageTitle = `${lesson.title} — Networking from Scratch`;
  const pageDesc = `${phase.id}.${num}: ${lesson.title}. Hands-on ${LANG_LABELS[lesson.lang] || lesson.lang} lesson from Networking from Scratch.`;
  const canonicalPath = `/lessons/${phase.slug}/${lesson.slug}/`;
  const canonicalUrl = `${SITE}${canonicalPath}`;
  const langLabel = LANG_LABELS[lesson.lang] || lesson.lang;
  const typeLabel = TYPE_LABELS[lesson.type] || lesson.type;

  // CSS path depth: /lessons/PHASE/LESSON/index.html -> 3 levels deep
  const cssPath = '../../../css/style.css';
  const fontsUrl = 'https://fonts.googleapis.com/css2?family=VT323&family=Source+Serif+4:ital,wght@0,300..900;1,300..900&family=JetBrains+Mono:wght@400;500;600&display=swap';

  // Build sidebar HTML
  let sidebarHtml = '';
  phase.lessons.forEach((l, i) => {
    const lnum = String(i + 1).padStart(2, '0');
    const isActive = i === lessonIdx;
    const href = `/lessons/${phase.slug}/${l.slug}/`;
    sidebarHtml += `<a href="${href}" class="sidebar-item${isActive ? ' active' : ''}">` +
      `<span class="sidebar-dot"></span>` +
      `<span class="sidebar-num">${lnum}</span>` +
      `<span class="sidebar-title">${escHtml(l.title)}</span></a>\n`;
  });

  // Build prev/next
  let prevNextHtml = '<div class="prev-next">';
  if (lessonIdx > 0) {
    const prev = phase.lessons[lessonIdx - 1];
    prevNextHtml += `<a href="/lessons/${phase.slug}/${prev.slug}/" class="pn-link prev">` +
      `<span class="pn-arrow">&larr;</span><span class="pn-label">Previous</span>` +
      `<span class="pn-title">${escHtml(prev.title)}</span></a>`;
  } else {
    prevNextHtml += '<span></span>';
  }
  if (lessonIdx < phase.lessons.length - 1) {
    const next = phase.lessons[lessonIdx + 1];
    prevNextHtml += `<a href="/lessons/${phase.slug}/${next.slug}/" class="pn-link next">` +
      `<span class="pn-arrow">&rarr;</span><span class="pn-label">Next</span>` +
      `<span class="pn-title">${escHtml(next.title)}</span></a>`;
  }
  prevNextHtml += '</div>';

  // Breadcrumb
  const breadcrumbHtml = `<a href="/">Home</a> &rsaquo; ` +
    `<a href="/#${phase.id}">${phase.id}: ${escHtml(phase.title)}</a> &rsaquo; ` +
    `<span>${num}. ${escHtml(lesson.title)}</span>`;

  // Structured data
  const ldJson = JSON.stringify({
    '@context': 'https://schema.org',
    '@type': 'LearningResource',
    name: lesson.title,
    description: pageDesc,
    url: canonicalUrl,
    learningResourceType: typeLabel,
    programmingLanguage: langLabel,
    isPartOf: {
      '@type': 'Course',
      name: 'Networking from Scratch',
      url: SITE
    },
    provider: {
      '@type': 'Organization',
      name: 'Networking from Scratch',
      url: SITE
    }
  });

  return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>${escHtml(pageTitle)}</title>
<meta name="description" content="${escHtml(pageDesc)}">
<meta property="og:title" content="${escHtml(pageTitle)}">
<meta property="og:description" content="${escHtml(pageDesc)}">
<meta property="og:type" content="article">
<meta property="og:url" content="${canonicalUrl}">
<meta property="og:image" content="${SITE}/og.png">
<meta name="twitter:card" content="summary_large_image">
<meta name="twitter:title" content="${escHtml(pageTitle)}">
<meta name="twitter:description" content="${escHtml(pageDesc)}">
<meta name="twitter:image" content="${SITE}/og.png">
<meta name="theme-color" content="#00897B">
<meta name="google-site-verification" content="0wstVvV2yfk22BVUKDKAiLU3llaYtNL0mdeo5vN2gPo">
<meta name="robots" content="index, follow">
<meta property="og:site_name" content="Networking from Scratch">
<meta property="og:locale" content="en_US">
<link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>🌐</text></svg>">
<link rel="canonical" href="${canonicalUrl}">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="${fontsUrl}">
<link rel="stylesheet" href="${cssPath}">
<script type="application/ld+json">${ldJson}</script>
<script>
var _t=localStorage.getItem('nfs_theme');if(_t==='dark'||(!_t&&matchMedia('(prefers-color-scheme:dark)').matches)){document.documentElement.classList.add('dark');document.addEventListener('DOMContentLoaded',function(){var t=document.querySelector('.theme-toggle');if(t){t.textContent='☀';t.title='Switch to light mode'}})}else if(_t==='light'){document.documentElement.classList.add('light')}
</script>
</head>
<body>

<div id="scroll-progress" class="scroll-progress"></div>

<nav class="topnav">
  <a href="/" class="brand">NETWORKING / FROM SCRATCH</a>
  <div class="links">
    <a href="/">Contents</a>
    <a href="/catalog.html">Catalog</a>
    <a href="/roadmap.html">Roadmap</a>
    <a href="/glossary.html">Glossary</a>
    <a href="https://github.com/TanayK07/networking-from-scratch" class="gh-link">GitHub <span class="gh-stars" style="display:none"></span></a>
    <button class="theme-toggle" aria-label="Toggle theme">☾</button>
  </div>
</nav>

<div class="lesson-layout">
  <button id="sidebar-toggle" class="sidebar-toggle" aria-label="Toggle sidebar">&#9776;</button>

  <aside id="sidebar" class="lesson-sidebar">
    <div class="sidebar-header">
      <h3 id="sidebar-phase-title">${phase.id}: ${escHtml(phase.title)}</h3>
    </div>
    <div id="sidebar-lessons" class="sidebar-lessons">
${sidebarHtml}
    </div>
  </aside>

  <main class="lesson-main">
    <div id="breadcrumb" class="breadcrumb">${breadcrumbHtml}</div>
    <div id="lesson-content" class="lesson-content">
      <article class="lesson-article">
${contentHtml}
      </article>
    </div>
    <div id="share-bar" class="share-bar">
      <span class="share-bar-label">SHARE</span>
      <a id="share-twitter" class="share-btn" href="https://twitter.com/intent/tweet?text=${encodeURIComponent(pageTitle + ' ' + canonicalUrl)}" target="_blank" rel="noopener">Twitter/X</a>
      <a id="share-reddit" class="share-btn" href="https://reddit.com/submit?url=${encodeURIComponent(canonicalUrl)}&title=${encodeURIComponent(pageTitle)}" target="_blank" rel="noopener">Reddit</a>
      <button id="share-copy" class="share-btn" onclick="navigator.clipboard.writeText('${canonicalUrl}').then(function(){this.textContent='Copied!';var b=this;setTimeout(function(){b.textContent='Copy link'},1500)}.bind(this))">Copy link</button>
    </div>
    <div id="completion" class="lesson-completion"></div>
    <div id="prev-next" class="lesson-nav">${prevNextHtml}</div>
  </main>
</div>

<footer>
  <span><a href="https://github.com/TanayK07/networking-from-scratch">GitHub</a> &middot; open source &middot; free forever</span>
  <div class="footer-links">
    <a href="/">Home</a>
    <a href="/catalog.html">Catalog</a>
    <a href="/glossary.html">Glossary</a>
    <a href="https://github.com/TanayK07/networking-from-scratch/discussions">Discussions</a>
  </div>
</footer>

<script src="/js/data.js"></script>
<script src="/js/progress.js"></script>
<script src="/js/header.js"></script>
<script src="/js/app.js"></script>
<script>
// Hydrate completion checkbox and sidebar active states from localStorage
(function() {
  var phaseIdx = ${phaseIdx}, lessonIdx = ${lessonIdx};
  document.addEventListener('DOMContentLoaded', function() {
    if (!NFS.progress) return;
    // Completion checkbox
    var comp = document.getElementById('completion');
    if (comp) {
      var isDone = NFS.progress.isDone(phaseIdx, lessonIdx);
      comp.innerHTML = '<label class="complete-toggle"><input type="checkbox" ' +
        (isDone ? 'checked' : '') +
        ' onchange="NFS.progress.toggle(' + phaseIdx + ',' + lessonIdx + ');location.reload()"><span>Mark as complete</span></label>';
    }
    // Sidebar done states
    var items = document.querySelectorAll('.sidebar-item');
    items.forEach(function(el, i) {
      if (NFS.progress.isDone(phaseIdx, i)) {
        el.classList.add('done');
        el.querySelector('.sidebar-dot').classList.add('done');
      }
    });
    // Scroll progress bar
    var bar = document.getElementById('scroll-progress');
    if (bar) {
      window.addEventListener('scroll', function() {
        var h = document.documentElement.scrollHeight - window.innerHeight;
        bar.style.width = (h > 0 ? (window.scrollY / h) * 100 : 0) + '%';
      });
    }
    // Sidebar toggle
    var toggle = document.getElementById('sidebar-toggle');
    var sidebar = document.getElementById('sidebar');
    if (toggle && sidebar) {
      toggle.addEventListener('click', function() {
        sidebar.classList.toggle('open');
        toggle.classList.toggle('open');
      });
    }
    // Theme toggle
    NFS.initThemeToggle && NFS.initThemeToggle();
    // GitHub stars
    NFS.fetchStars && NFS.fetchStars();
  });
})();
</script>

</body>
</html>`;
}

// ---------- Main build ----------

function build() {
  const phases = parseDataJs();
  if (phases.length === 0) {
    console.error('ERROR: No phases parsed from data.js');
    process.exit(1);
  }

  let generated = 0;
  let missing = 0;
  const sitemapUrls = [];

  // Static pages for sitemap
  ['/', '/catalog.html', '/roadmap.html', '/glossary.html'].forEach(p => {
    sitemapUrls.push({ loc: `${SITE}${p}`, priority: p === '/' ? '1.0' : '0.8', changefreq: 'weekly' });
  });

  for (let pi = 0; pi < phases.length; pi++) {
    const phase = phases[pi];
    for (let li = 0; li < phase.lessons.length; li++) {
      const lesson = phase.lessons[li];
      const readmePath = join(PHASES_DIR, phase.slug, lesson.slug, 'README.md');

      let contentHtml;
      if (existsSync(readmePath)) {
        const md = readFileSync(readmePath, 'utf8');
        contentHtml = marked.parse(md);
      } else {
        missing++;
        const langLabel = LANG_LABELS[lesson.lang] || lesson.lang;
        const typeLabel = TYPE_LABELS[lesson.type] || lesson.type;
        contentHtml = `<h1>${String(li + 1).padStart(2, '0')}. ${escHtml(lesson.title)}</h1>
<p class="mono text-muted">${phase.id} &middot; ${escHtml(phase.title)} &middot; ${typeLabel} &middot; ${langLabel}</p>
<div class="lesson-coming-soon">
<p style="font-size:1.1rem;margin-bottom:16px">This lesson is planned but not written yet.</p>
<p>The curriculum is open source — anyone can write a lesson.</p>
<div style="margin-top:24px;display:flex;gap:12px;flex-wrap:wrap">
<a href="https://github.com/TanayK07/networking-from-scratch/blob/main/CONTRIBUTING.md" class="btn">Contribute this lesson</a>
<a href="/catalog.html" class="btn btn-outline">Browse catalog</a>
</div>
</div>`;
      }

      const html = buildPage({
        phase, phaseIdx: pi,
        lesson, lessonIdx: li,
        contentHtml, phases
      });

      const outPath = join(OUT_DIR, phase.slug, lesson.slug, 'index.html');
      mkdirSync(dirname(outPath), { recursive: true });
      writeFileSync(outPath, html);
      generated++;

      sitemapUrls.push({
        loc: `${SITE}/lessons/${phase.slug}/${lesson.slug}/`,
        priority: '0.6',
        changefreq: 'monthly'
      });
    }
  }

  // Generate sitemap
  const sitemapXml = `<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
${sitemapUrls.map(u => `  <url><loc>${u.loc}</loc><changefreq>${u.changefreq}</changefreq><priority>${u.priority}</priority></url>`).join('\n')}
</urlset>`;

  writeFileSync(join(WEBSITE_DIR, 'sitemap.xml'), sitemapXml);

  console.log(`Built ${generated} lesson pages (${missing} without README)`);
  console.log(`Sitemap: ${sitemapUrls.length} URLs`);
}

build();
