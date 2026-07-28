import { readdir, readFile } from 'node:fs/promises';
import { join, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const distUrl = new URL('../dist/', import.meta.url);
const root = fileURLToPath(distUrl);
const expectedOrigin = 'https://docs.siecs.dev';
const failures = [];

async function collectHtml(directory) {
  const entries = await readdir(directory, { withFileTypes: true });
  const nested = await Promise.all(entries.map((entry) => {
    const path = join(directory, entry.name);
    return entry.isDirectory()
      ? collectHtml(path)
      : entry.name.endsWith('.html') ? [path] : [];
  }));
  return nested.flat();
}

function decode(value) {
  return value
    .replaceAll('&amp;', '&')
    .replaceAll('&quot;', '"')
    .replaceAll('&#39;', "'")
    .replaceAll('&lt;', '<')
    .replaceAll('&gt;', '>');
}

function attributes(tag) {
  const result = new Map();
  for (const match of tag.matchAll(/([:\w-]+)=["']([^"']*)["']/g)) {
    result.set(match[1], decode(match[2]));
  }
  return result;
}

function metadata(html) {
  const meta = [...html.matchAll(/<meta\b[^>]*>/gi)].map((match) => attributes(match[0]));
  const links = [...html.matchAll(/<link\b[^>]*>/gi)].map((match) => attributes(match[0]));
  const byMeta = (key, value) => meta.find((attrs) => attrs.get(key) === value)?.get('content');
  const byLink = (rel) => links.find((attrs) => attrs.get('rel') === rel)?.get('href');

  return {
    title: decode(html.match(/<title>([\s\S]*?)<\/title>/i)?.[1]?.trim() ?? ''),
    description: byMeta('name', 'description'),
    canonical: byLink('canonical'),
    ogTitle: byMeta('property', 'og:title'),
    ogDescription: byMeta('property', 'og:description'),
    ogImage: byMeta('property', 'og:image'),
    twitterImage: byMeta('name', 'twitter:image'),
    h1Count: [...html.matchAll(/<h1\b/gi)].length,
    hasJsonLd: /<script\b[^>]*type=["']application\/ld\+json["'][^>]*>/i.test(html),
    noindex: meta.some((attrs) =>
      attrs.get('name') === 'robots' && attrs.get('content')?.includes('noindex')),
  };
}

const files = (await collectHtml(root))
  .filter((path) => !path.endsWith(`${sep}404.html`));
const titles = new Map();
const descriptions = new Map();
const pagePaths = new Set(files.map((file) => {
  const path = `/${relative(root, file).replaceAll(sep, '/')}`
    .replace(/index\.html$/, '');
  return path === '/' ? path : path.replace(/\/$/, '');
}));

for (const file of files) {
  const page = `/${relative(root, file).replaceAll(sep, '/')}`;
  const html = await readFile(file, 'utf8');
  const data = metadata(html);

  if (!data.title) failures.push(`${page}: missing <title>`);
  if (!data.description || data.description.length < 50) {
    failures.push(`${page}: missing or too-short meta description`);
  }
  if (!data.canonical?.startsWith(expectedOrigin)) {
    failures.push(`${page}: missing canonical URL on ${expectedOrigin}`);
  }
  if (!data.ogTitle || !data.ogDescription || !data.ogImage) {
    failures.push(`${page}: incomplete Open Graph metadata`);
  }
  if (!data.twitterImage) failures.push(`${page}: missing Twitter image`);
  if (data.h1Count !== 1) failures.push(`${page}: expected one H1, found ${data.h1Count}`);
  if (!data.hasJsonLd) failures.push(`${page}: missing JSON-LD structured data`);
  if (data.noindex) failures.push(`${page}: unexpectedly marked noindex`);

  if (data.title) {
    const duplicate = titles.get(data.title);
    if (duplicate) failures.push(`${page}: duplicate title also used by ${duplicate}`);
    titles.set(data.title, page);
  }
  if (data.description) {
    const duplicate = descriptions.get(data.description);
    if (duplicate) failures.push(`${page}: duplicate description also used by ${duplicate}`);
    descriptions.set(data.description, page);
  }

  for (const match of html.matchAll(/<a\b[^>]*href=["']([^"']+)["'][^>]*>/gi)) {
    const href = decode(match[1]);
    if (href.startsWith('#') || href.startsWith('mailto:')) continue;

    const target = new URL(href, data.canonical);
    if (target.origin !== expectedOrigin || /\.[a-z0-9]+$/i.test(target.pathname)) continue;

    const path = target.pathname === '/'
      ? '/'
      : target.pathname.replace(/\/$/, '');
    if (!pagePaths.has(path)) failures.push(`${page}: broken internal link to ${href}`);
  }
}

const robots = await readFile(new URL('./robots.txt', distUrl), 'utf8');
if (!robots.includes('Sitemap: https://docs.siecs.dev/sitemap-index.xml')) {
  failures.push('/robots.txt: missing production sitemap URL');
}

await readFile(new URL('./sitemap-index.xml', distUrl), 'utf8');
await readFile(new URL('./favicon.svg', distUrl), 'utf8');
await readFile(new URL('./og-image.png', distUrl));

if (failures.length > 0) {
  console.error(`SEO audit failed with ${failures.length} issue(s):`);
  for (const failure of failures) console.error(`- ${failure}`);
  process.exit(1);
}

console.log(`SEO audit passed for ${files.length} documentation pages.`);
