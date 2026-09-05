// Renders the specification (README.md + docs/*.md) into self-contained HTML
// pages for mobile reading. No network at render time or read time: marked is
// vendored, CSS is inlined.
//
//   node scripts/render-docs.mjs <outdir> <file.md>...
//
// README.md becomes <outdir>/index.html; every other file keeps its path with a
// .html extension (docs/05-hardware.md -> <outdir>/docs/05-hardware.html), so the
// cross-links the markdown already uses keep working once .md is rewritten to
// .html.

import { mkdirSync, readFileSync, writeFileSync } from 'node:fs'
import { createRequire } from 'node:module'
import { dirname, join } from 'node:path'

const require = createRequire(import.meta.url)
const { marked } = require('./vendor/marked.min.js')

const [outdir, ...inputs] = process.argv.slice(2)
if (!outdir || inputs.length === 0) {
  console.error('usage: render-docs.mjs <outdir> <file.md>...')
  process.exit(2)
}

const escapeHtml = (s) =>
  s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;')

// GitHub's heading-anchor algorithm, so the cross-document links that carry an
// anchor (05-hardware.md#existing-hardware) resolve in the rendered pages.
function makeSlugger() {
  const seen = new Map()
  return (text) => {
    const base = text
      .toLowerCase()
      .replace(/[^\p{L}\p{N}\s-]/gu, '')
      .trim()
      .replace(/\s/g, '-')
    const n = seen.get(base) ?? 0
    seen.set(base, n + 1)
    return n === 0 ? base : `${base}-${n}`
  }
}

const outputPath = (input) =>
  input === 'README.md' ? 'index.html' : input.replace(/\.md$/, '.html')

// Rewrites the links between source files to their published counterparts:
// README.md -> index.html, anything else .md -> .html. Absolute URLs, mailto:
// and bare anchors are left alone. A link to a directory (docs/) has no page of
// its own, so it goes to the index instead. `rootRel` is the path back to the
// published root from the page being rendered ('' for index, '../' for docs/).
function rewriteLinks(html, rootRel) {
  return html.replace(/href="([^"]*)"/g, (whole, href) => {
    if (/^[a-z][a-z0-9+.-]*:/i.test(href) || href.startsWith('#') || href.startsWith('/')) {
      return whole
    }
    if (href.endsWith('/')) return `href="${rootRel}index.html"`
    const rewritten = href
      .replace(/(^|\/)README\.md(?=$|#)/, '$1index.html')
      .replace(/\.md(?=$|#)/, '.html')
    return `href="${escapeHtml(rewritten)}"`
  })
}

function render(input) {
  const rel = outputPath(input)
  const rootRel = '../'.repeat(rel.split('/').length - 1)
  const slugify = makeSlugger()

  marked.use({
    gfm: true,
    renderer: {
      heading(text, level, raw) {
        const id = slugify(raw)
        return `<h${level} id="${id}"><a class="anchor" href="#${id}" aria-hidden="true">#</a>${text}</h${level}>\n`
      },
    },
  })

  const markdown = readFileSync(input, 'utf8')
  const title = (markdown.match(/^#\s+(.+)$/m)?.[1] ?? input).replace(/[*_`]/g, '')

  // Wrap every table so wide spec tables scroll sideways instead of overflowing a
  // phone viewport. Done on the output string rather than via a renderer override:
  // markdown tables cannot nest, so this is unambiguous and survives marked upgrades.
  const body = rewriteLinks(
    marked
      .parse(markdown)
      .replace(/<table>/g, '<div class="table-wrap"><table>')
      .replace(/<\/table>/g, '</table></div>'),
    rootRel,
  )

  const generated = new Date().toISOString().replace(/\.\d{3}Z$/, 'Z')
  const out = join(outdir, rel)
  mkdirSync(dirname(out), { recursive: true })
  writeFileSync(out, page({ title, body, source: input, generated, rootRel }), 'utf8')
  return rel
}

const page = ({ title, body, source, generated, rootRel }) => `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>${escapeHtml(title)}</title>
<style>
:root {
  --bg: #ffffff; --fg: #1f2328; --muted: #59636e; --border: #d1d9e0;
  --accent: #0969da; --code-bg: #f6f8fa; --mark: #fff8c5;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #0d1117; --fg: #e6edf3; --muted: #9198a1; --border: #3d444d;
    --accent: #4493f8; --code-bg: #151b23; --mark: #3f2e00;
  }
}
* { box-sizing: border-box; }
html { -webkit-text-size-adjust: 100%; }
body {
  margin: 0; padding: 1.25rem 1rem 6rem;
  background: var(--bg); color: var(--fg);
  font: 16px/1.65 -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
  overflow-wrap: break-word;
}
main { max-width: 46rem; margin: 0 auto; }
h1, h2, h3, h4, h5, h6 { line-height: 1.25; margin: 2rem 0 1rem; font-weight: 600; scroll-margin-top: 1rem; }
h1 { font-size: 1.9rem; padding-bottom: .3em; border-bottom: 1px solid var(--border); margin-top: 0; }
h2 { font-size: 1.45rem; padding-bottom: .3em; border-bottom: 1px solid var(--border); }
h3 { font-size: 1.2rem; }
h4, h5, h6 { font-size: 1rem; }
h6 { color: var(--muted); }
a { color: var(--accent); text-decoration: none; }
a:hover { text-decoration: underline; }
.anchor {
  float: left; margin-left: -1em; padding-right: .3em;
  color: var(--muted); opacity: 0; font-weight: 400;
}
h1:hover .anchor, h2:hover .anchor, h3:hover .anchor,
h4:hover .anchor, h5:hover .anchor, h6:hover .anchor { opacity: .5; }
p, ul, ol, blockquote, pre, .table-wrap { margin: 0 0 1rem; }
ul, ol { padding-left: 1.6rem; }
li + li { margin-top: .25rem; }
blockquote { padding: 0 1rem; color: var(--muted); border-left: .25rem solid var(--border); }
blockquote > :last-child { margin-bottom: 0; }
code, kbd, samp {
  font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Consolas, monospace;
  font-size: .875em;
}
:not(pre) > code { padding: .2em .4em; border-radius: 6px; background: var(--code-bg); }
pre {
  padding: 1rem; border-radius: 6px; background: var(--code-bg);
  overflow-x: auto; -webkit-overflow-scrolling: touch;
}
pre code { padding: 0; background: none; }
hr { height: 1px; margin: 2rem 0; border: 0; background: var(--border); }
mark { background: var(--mark); color: inherit; }
/* Wide spec tables scroll sideways rather than squeezing the page. */
.table-wrap { overflow-x: auto; -webkit-overflow-scrolling: touch; }
table { border-collapse: collapse; width: 100%; font-size: .9rem; }
th, td { padding: .4rem .75rem; border: 1px solid var(--border); text-align: left; vertical-align: top; }
th { background: var(--code-bg); font-weight: 600; white-space: nowrap; }
img { max-width: 100%; }
.meta { margin-bottom: 2rem; color: var(--muted); font-size: .8rem; }
.meta a { color: inherit; text-decoration: underline; }
#top {
  position: fixed; right: 1rem; bottom: 1rem;
  display: none; align-items: center; justify-content: center;
  width: 2.75rem; height: 2.75rem;
  border: 1px solid var(--border); border-radius: 50%;
  background: var(--bg); color: var(--fg); font-size: 1.1rem;
  box-shadow: 0 1px 6px rgb(0 0 0 / .15);
}
#top.show { display: flex; }
</style>
</head>
<body>
<main>
${body}
<p class="meta">Rendered from ${escapeHtml(source)} &middot; ${generated}${
  rootRel ? ` &middot; <a href="${rootRel}index.html">Documentation index</a>` : ''
}</p>
</main>
<a id="top" href="#" aria-label="Back to top">&uarr;</a>
<script>
const btn = document.getElementById('top')
addEventListener('scroll', () => btn.classList.toggle('show', scrollY > 800), { passive: true })
</script>
</body>
</html>
`

for (const input of inputs) render(input)
