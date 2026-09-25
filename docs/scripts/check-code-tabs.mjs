import { readdir, readFile } from 'node:fs/promises';
import { join, relative } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../src/content/docs/', import.meta.url));
const failures = [];
let selectorCount = 0;
let pairedCodeCount = 0;
const sharedCppFunctions = new Set([
  'ecs_entity_id', 'ecs_entity_generation',
  'ecs_observer_enable', 'ecs_observer_disable',
  'ecs_save_memory', 'ecs_scene_free', 'ecs_scene_validate',
  'ecs_load_memory', 'ecs_save', 'ecs_load',
]);

async function collect(directory) {
  const entries = await readdir(directory, { withFileTypes: true });
  const nested = await Promise.all(entries.map((entry) => {
    const path = join(directory, entry.name);
    return entry.isDirectory()
      ? collect(path)
      : /\.mdx?$/.test(entry.name) ? [path] : [];
  }));
  return nested.flat();
}

function fail(file, line, message) {
  failures.push(`${relative(root, file)}:${line}: ${message}`);
}

for (const file of await collect(root)) {
  const source = await readFile(file, 'utf8');
  const lines = source.split('\n');
  const tabs = [];
  let tab = null;
  let fence = null;

  for (let index = 0; index < lines.length; index++) {
    const line = lines[index];
    const number = index + 1;

    if (line.startsWith('<Tabs')) {
      tabs.push({
        line: number,
        languages: new Map(),
        labels: [],
        languageSelector: line.includes('syncKey="language"'),
      });
      continue;
    }
    if (line.startsWith('</Tabs>')) {
      const group = tabs.pop();
      if (!group) {
        fail(file, number, 'closing </Tabs> has no opening <Tabs>');
      } else if (group.languages.size > 0 &&
                 (!group.languages.has('C') || !group.languages.has('C++') ||
                  group.languages.size !== 2)) {
        fail(file, group.line, 'a language selector must contain exactly one C and one C++ example');
      } else if (group.languages.size === 2) {
        const c = group.languages.get('C');
        const cpp = group.languages.get('C++');
        if (c.body.join('\n').trim() === cpp.body.join('\n').trim()) {
          fail(file, group.line, 'C and C++ tabs contain identical code');
        }
      }
      if (group?.languageSelector &&
          (group.labels.length !== 2 || group.labels[0] !== 'C' || group.labels[1] !== 'C++')) {
        fail(file, group.line, 'language selector must contain one C tab followed by one C++ tab');
      }
      if (group?.languageSelector) selectorCount++;
      if (group?.languages.size === 2) pairedCodeCount++;
      continue;
    }

    const tabMatch = line.match(/^<TabItem label="(C\+\+|C)">$/);
    if (tabMatch) {
      tab = { label: tabMatch[1], line: number, group: tabs.at(-1) };
      if (!tab.group) fail(file, number, 'language tab must be inside <Tabs>');
      else tab.group.labels.push(tab.label);
      continue;
    }
    if (line === '</TabItem>') {
      tab = null;
      continue;
    }

    const openingFence = line.match(/^```(c|cpp)\s*$/);
    if (openingFence) {
      fence = { language: openingFence[1], line: number, tab, body: [] };
      if (!tab) fail(file, number, `${fence.language} example is not inside a C/C++ selector`);
      else if ((fence.language === 'c') !== (tab.label === 'C')) {
        fail(file, number, `${fence.language} fence is under the ${tab.label} tab`);
      } else {
        const previous = tab.group.languages.get(tab.label);
        if (previous) fail(file, number, `selector already has a ${tab.label} example at line ${previous.line}`);
        else tab.group.languages.set(tab.label, fence);
      }
      continue;
    }
    if (line === '```' && fence) {
      fence = null;
      continue;
    }
    if (fence?.language === 'cpp' && /(^|[^:]):{2}ecs_/.test(line)) {
      fail(file, number, 'C++ example calls the C ABI directly; use the typed ecs:: API');
    }
    // The standalone quickstart uses the distribution header, which embeds the C++ facade.
    if (fence?.language === 'cpp' && line.includes('#include <siecs.h>') &&
        !file.endsWith('quickstart.mdx')) {
      fail(file, number, 'C++ example must include <siecs/cpp.hpp>');
    }
    if (fence?.language === 'cpp') {
      const call = line.match(/\b(ecs_[a-zA-Z0-9_]+)\s*\(/)?.[1];
      if (call && !sharedCppFunctions.has(call)) {
        fail(file, number, `C++ example uses ${call} even though a typed facade should be preferred`);
      }
    }
    if (fence) fence.body.push(line);
  }

  if (tabs.length) fail(file, lines.length, 'unclosed <Tabs> selector');
  if (source.includes('<Tabs') && file.endsWith('.md')) {
    fail(file, 1, 'files using Tabs must use the .mdx extension');
  }
  if (source.includes('<Tabs') && !source.includes("import { Tabs, TabItem }")) {
    fail(file, 1, 'file uses Tabs without importing Tabs and TabItem');
  }
}

if (failures.length) {
  console.error(`Code-tab audit failed with ${failures.length} issue(s):`);
  for (const failure of failures) console.error(`- ${failure}`);
  process.exit(1);
}

console.log(
  `Code-tab audit passed: ${selectorCount} language selectors, ` +
  `${pairedCodeCount} paired C/C++ examples.`
);
