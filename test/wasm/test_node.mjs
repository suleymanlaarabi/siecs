import { createRequire } from "node:module";
import { resolve } from "node:path";
import { performance } from "node:perf_hooks";

const require = createRequire(import.meta.url);
const roots = process.argv.slice(2);
if (roots.length === 0) {
    console.error("usage: node test/wasm/test_node.mjs <build-dir> [...]");
    process.exit(2);
}

for (const root of roots) {
    for (const [language, symbol] of [["c", "_siecs_wasm_smoke"], ["cpp", "_siecs_wasm_cpp_smoke"]]) {
        const factory = require(resolve(root, language, "siecs_wasm.js"));
        const module = await factory();
        const start = performance.now();
        const result = module[symbol]();
        const elapsed = performance.now() - start;
        if (result !== 0) {
            throw new Error(`${root}/${language} returned ${result}`);
        }
        if (elapsed > 500) {
            throw new Error(`${root}/${language} blocked for ${elapsed.toFixed(1)} ms`);
        }
        console.log(`${root}/${language}: SIECS_WASM_OK`);
    }
}
