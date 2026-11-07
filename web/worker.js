// SPDX-License-Identifier: MIT
// Builds openlf1.gba in the browser: the game ZIP is converted by the same Python code the Makefile runs
// (tools/convert_assets.py in Pyodide) and the result is written into the prebuilt ROM.
//
// Messages in:  {type: "start", file?: File}
// Messages out: {type: "status", text, progress?}, {type: "need-file", reason}, {type: "done", rom}, {type: "error", text}

const post = (msg, transfer) => self.postMessage(msg, transfer || []);
const status = (text, progress) => post({ type: "status", text, progress });

let config, manifest;

async function getJson(url) {
  const r = await fetch(url);
  if (!r.ok) throw new Error(`${url}: HTTP ${r.status}`);
  return r.json();
}

async function getBytes(url, label, expected) {
  const r = await fetch(url);
  if (!r.ok) throw new Error(`${label}: HTTP ${r.status}`);
  const total = Number(r.headers.get("content-length")) || expected || 0;
  const chunks = [];
  let got = 0;
  const reader = r.body.getReader();
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    chunks.push(value);
    got += value.length;
    if (total && got <= total) status(`${label}: ${(got / 1048576).toFixed(1)} of ${(total / 1048576).toFixed(1)} MiB`, got / total);
  }
  const out = new Uint8Array(got);
  let at = 0;
  for (const c of chunks) { out.set(c, at); at += c.length; }
  return out;
}

async function sha256(bytes) {
  const d = new Uint8Array(await crypto.subtle.digest("SHA-256", bytes));
  return Array.from(d, (b) => b.toString(16).padStart(2, "0")).join("");
}

async function gunzip(bytes) {
  const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream("gzip"));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

async function loadGame(file) {
  if (file) return new Uint8Array(await file.arrayBuffer());
  let last = "";
  for (const url of config.zipUrls) {
    try {
      return await getBytes(url, "Downloading the game", config.zipSize);
    } catch (e) {
      last = `${url}: ${e.message}`;
    }
  }
  throw Object.assign(new Error(last), { needFile: true });
}

async function build(file) {
  config = await getJson("config.json");
  manifest = await getJson("manifest.json");

  const gamePromise = loadGame(file);
  const templatePromise = getBytes("template.bin.gz", "Loading the ROM template", 0).then(gunzip);
  const game = await gamePromise;
  status("Checking the game files…");
  if (game.length !== config.zipSize || (await sha256(game)) !== config.zipSha256) {
    throw Object.assign(new Error("This is not the expected Little_Fighter_1995.zip."), { needFile: !!file });
  }
  const rom = await templatePromise;

  // The boot logo is not part of the template; it is read from devkitPro's gbafix.c.
  const source = await (await fetch(config.logoUrl)).text();
  const body = source.match(/\/\/ logo\s*\n\s*\{([^}]*)\}/)[1];
  const logo = Uint8Array.from(body.match(/0x[0-9A-Fa-f]{2}/g), (v) => parseInt(v, 16));
  if (logo.length !== 156 || (await sha256(logo)) !== manifest.logo.sha256) throw new Error("Could not get the boot logo.");
  rom.set(logo, manifest.logo.offset);

  status("Loading Python…", 0);
  const { loadPyodide } = await import(config.pyodide + "pyodide.mjs");
  const py = await loadPyodide({ indexURL: config.pyodide });
  py.FS.mkdir("/tools");
  for (const name of manifest.python) {
    py.FS.writeFile("/tools/" + name, new Uint8Array(await (await fetch("py/" + name)).arrayBuffer()));
  }
  py.FS.writeFile("/game.zip", game);

  status("Converting the game data…");
  await new Promise((r) => setTimeout(r, 0));
  py.runPython(`
import sys
sys.path.insert(0, "/tools")
import convert_assets
convert_assets.convert("/game.zip", "/out")
`);

  status("Assembling the ROM…");
  for (const blob of manifest.blobs) {
    const data = py.FS.readFile("/out/" + blob.name);
    if (data.length !== blob.size || (await sha256(data)) !== blob.sha256) {
      throw new Error(`${blob.name} does not match the expected build output.`);
    }
    rom.set(data, blob.offset);
  }
  for (const hdr of manifest.headers) {
    if ((await sha256(py.FS.readFile("/out/" + hdr.name))) !== hdr.sha256) {
      throw new Error(`${hdr.name} does not match the prebuilt code.`);
    }
  }
  return rom;
}

self.onmessage = async (e) => {
  if (e.data.type !== "start") return;
  try {
    const rom = await build(e.data.file);
    post({ type: "done", rom }, [rom.buffer]);
  } catch (err) {
    if (err.needFile) post({ type: "need-file", reason: err.message });
    else post({ type: "error", text: String((err && err.message) || err) });
  }
};
