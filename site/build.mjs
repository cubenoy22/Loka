#!/usr/bin/env node
// Loka website generator — zero-dependency placeholder until Loka can generate
// this site itself. Reads data/site.json, writes a static page into dist/.
//
//   node site/build.mjs

import { cpSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = dirname(fileURLToPath(import.meta.url));
const repoRoot = join(root, '..');
const dist = join(root, 'dist');
const data = JSON.parse(readFileSync(join(root, 'data', 'site.json'), 'utf8'));

const esc = (s) =>
  String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

rmSync(dist, { recursive: true, force: true });
mkdirSync(join(dist, 'images'), { recursive: true });

// ---------------------------------------------------------------------------
// Placeholder images: entries without a real image get a generated SVG.
// Drop a real image into static/images/<kind>/<slug>.<ext> (or set the
// "image" field in site.json) and it replaces the placeholder on next build.
// ---------------------------------------------------------------------------

const IMAGE_EXTENSIONS = ['png', 'jpg', 'jpeg', 'webp'];

function placeholderSvg(name) {
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 640 480" role="img" aria-label="${esc(name)} screenshot placeholder">
  <rect width="640" height="480" fill="#eef1f3"/>
  <rect x="20" y="20" width="600" height="440" rx="10" fill="#fffffe" stroke="#c6ccd2"/>
  <rect x="20" y="20" width="600" height="36" rx="10" fill="#dde3e8"/>
  <circle cx="44" cy="38" r="6" fill="#b3bcc4"/>
  <circle cx="64" cy="38" r="6" fill="#b3bcc4"/>
  <rect x="60" y="120" width="520" height="18" rx="9" fill="#e2e7eb"/>
  <rect x="60" y="160" width="380" height="18" rx="9" fill="#e2e7eb"/>
  <rect x="60" y="200" width="450" height="18" rx="9" fill="#e2e7eb"/>
  <text x="320" y="330" text-anchor="middle" font-family="ui-monospace, monospace" font-size="26" fill="#5b666f">${esc(name)}</text>
  <text x="320" y="366" text-anchor="middle" font-family="ui-monospace, monospace" font-size="15" fill="#8a949c">image coming soon</text>
</svg>
`;
}

function resolveImage(kind, item) {
  if (item.image) return item.image;
  for (const ext of IMAGE_EXTENSIONS) {
    const real = `images/${kind}/${item.slug}.${ext}`;
    if (existsSync(join(root, 'static', real))) return real;
  }
  const placeholder = `images/${kind}/${item.slug}-placeholder.svg`;
  mkdirSync(join(dist, 'images', kind), { recursive: true });
  writeFileSync(join(dist, placeholder), placeholderSvg(item.name));
  return placeholder;
}

// ---------------------------------------------------------------------------
// Sections
// ---------------------------------------------------------------------------

const { meta, hero, codeExample, examples, hardware, features, platforms, quickstart, footer } = data;

const header = `
<header class="site-header">
  <div class="container header-inner">
    <a class="brand" href="#top">Loka</a>
    <nav class="site-nav">
      <a href="#examples">Examples</a>
      <a href="#features">Features</a>
      <a href="#platforms">Platforms</a>
      <a href="#get-started">Get Started</a>
      <a href="${esc(meta.repoUrl)}" rel="noopener">GitHub</a>
      <button id="theme-toggle" type="button" aria-label="Toggle color scheme">◐</button>
    </nav>
  </div>
</header>`;

const heroSection = `
<section class="hero" id="top">
  <div class="container">
    <picture class="hero-art">
      <source media="(prefers-color-scheme: dark)" srcset="images/Hero-dark.svg">
      <img src="images/Hero.svg" alt="Loka cover artwork" width="960">
    </picture>
    <p class="badge">v${esc(meta.version)} · ${esc(meta.versionNote)}</p>
    <h1>${esc(hero.title)}</h1>
    <p class="lede">${esc(hero.subtitle)}</p>
    <p class="cta-row">
      <a class="button primary" href="${esc(hero.primaryCta.href)}">${esc(hero.primaryCta.label)}</a>
      <a class="button" href="${esc(hero.secondaryCta.href)}" rel="noopener">${esc(hero.secondaryCta.label)}</a>
    </p>
  </div>
</section>`;

const codeSection = `
<section class="code-example">
  <div class="container split">
    <div class="split-text">
      <h2>${esc(codeExample.heading)}</h2>
      <p>${esc(codeExample.body)}</p>
    </div>
    <figure class="code-card">
      <figcaption>${esc(codeExample.filename)}</figcaption>
      <pre><code>${esc(codeExample.code)}</code></pre>
    </figure>
  </div>
</section>`;

const exampleCards = examples.items
  .map((item) => {
    const img = resolveImage('examples', item);
    return `
    <a class="card" href="${esc(meta.repoUrl)}/tree/main/${esc(item.source)}" rel="noopener">
      <img src="${esc(img)}" alt="${esc(item.name)} screenshot" loading="lazy" width="640" height="480">
      <h3>${esc(item.name)}</h3>
      <p>${esc(item.description)}</p>
    </a>`;
  })
  .join('\n');

const examplesSection = `
<section class="examples" id="examples">
  <div class="container">
    <h2>${esc(examples.heading)}</h2>
    <p class="section-lede">${esc(examples.body)}</p>
    <div class="card-grid">${exampleCards}
    </div>
  </div>
</section>`;

const galleryFigures = (hardware.gallery || [])
  .map((item) => {
    const img = resolveImage('hardware', item);
    return `
    <figure>
      <img src="${esc(img)}" alt="${esc(item.name)}" loading="lazy" width="640" height="480">
      <figcaption>${esc(item.caption)}</figcaption>
    </figure>`;
  })
  .join('\n');

const hardwareSection = `
<section class="hardware">
  <div class="container">
    <h2>${esc(hardware.heading)}</h2>
    <p class="section-lede">${esc(hardware.body)}</p>
    <img class="hardware-photo" src="${esc(hardware.image)}" alt="${esc(hardware.imageAlt)}" loading="lazy">
    <div class="gallery">${galleryFigures}
    </div>
  </div>
</section>`;

const featureCards = features.items
  .map(
    (item, i) => `
    <div class="feature">
      <p class="feature-index">${String(i + 1).padStart(2, '0')} / ${String(features.items.length).padStart(2, '0')}</p>
      <h3>${esc(item.title)}</h3>
      <p>${esc(item.body)}</p>
    </div>`
  )
  .join('\n');

const featuresSection = `
<section class="features" id="features">
  <div class="container">
    <h2>${esc(features.heading)}</h2>
    <div class="feature-grid">${featureCards}
    </div>
  </div>
</section>`;

const platformRows = platforms.items
  .map(
    (p) => `
      <tr>
        <td>${esc(p.name)}</td>
        <td><span class="status status-${esc(p.status)}">${esc(p.status)}</span></td>
        <td>${esc(p.notes)}</td>
      </tr>`
  )
  .join('\n');

const platformsSection = `
<section class="platforms" id="platforms">
  <div class="container">
    <h2>${esc(platforms.heading)}</h2>
    <p class="section-lede">${esc(platforms.body)}</p>
    <div class="table-scroll">
      <table>
        <thead><tr><th>Environment</th><th>Status</th><th>Notes</th></tr></thead>
        <tbody>${platformRows}
        </tbody>
      </table>
    </div>
  </div>
</section>`;

const quickstartSteps = quickstart.steps
  .map(
    (step, i) => `
    <li>
      <h3><span class="step-number">${i + 1}</span> ${esc(step.title)}</h3>
      ${step.body ? `<p>${esc(step.body)}</p>` : ''}
      ${step.code ? `<pre><code>${esc(step.code)}</code></pre>` : ''}
    </li>`
  )
  .join('\n');

const quickstartSection = `
<section class="quickstart" id="get-started">
  <div class="container">
    <h2>${esc(quickstart.heading)}</h2>
    <ol class="steps">${quickstartSteps}
    </ol>
  </div>
</section>`;

const footerSection = `
<footer class="site-footer">
  <div class="container footer-inner">
    <nav>${footer.links.map((l) => `<a href="${esc(l.href)}" rel="noopener">${esc(l.label)}</a>`).join('\n')}</nav>
    <p>${esc(footer.note)}</p>
  </div>
</footer>`;

// Theme toggle: follows the OS scheme until the visitor picks one explicitly.
// The hero <picture> switches on prefers-color-scheme only, so an explicit
// choice also has to swap its sources by hand.
const themeScript = `
(function () {
  function apply(theme) {
    document.documentElement.dataset.theme = theme;
    var art = document.querySelector('.hero-art');
    art.querySelector('source').media = theme === 'dark' ? 'all' : 'not all';
  }
  var stored = localStorage.getItem('loka-theme');
  if (stored) apply(stored);
  document.getElementById('theme-toggle').addEventListener('click', function () {
    var dark = document.documentElement.dataset.theme
      ? document.documentElement.dataset.theme === 'dark'
      : matchMedia('(prefers-color-scheme: dark)').matches;
    var next = dark ? 'light' : 'dark';
    apply(next);
    localStorage.setItem('loka-theme', next);
  });
})();`;

const html = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${esc(meta.title)}</title>
<meta name="description" content="${esc(meta.description)}">
<link rel="stylesheet" href="styles.css">
</head>
<body>
${header}
<main>
${heroSection}
${codeSection}
${examplesSection}
${hardwareSection}
${featuresSection}
${platformsSection}
${quickstartSection}
</main>
${footerSection}
<script>${themeScript}</script>
</body>
</html>
`;

// ---------------------------------------------------------------------------
// Emit
// ---------------------------------------------------------------------------

writeFileSync(join(dist, 'index.html'), html);
cpSync(join(root, 'src', 'styles.css'), join(dist, 'styles.css'));
if (existsSync(join(root, 'static'))) cpSync(join(root, 'static'), dist, { recursive: true });
for (const asset of ['Hero.svg', 'Hero-dark.svg', 'LokaLaptops.jpg']) {
  cpSync(join(repoRoot, 'assets', asset), join(dist, 'images', asset));
}

console.log(`built site/dist (${examples.items.length} examples)`);
