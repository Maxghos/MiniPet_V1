// Replays the generated HTML and compares its decoded pixels with native C++ frames.
const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const html = fs.readFileSync('docs/caras.html', 'utf8');
const source = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const captures = [fs.readFileSync('build/faces.raw'), fs.readFileSync('build/styles.raw'), fs.readFileSync('build/weather.raw')];
const counts = captures.map(data => data.length / 512);
assert.deepStrictEqual(counts, [11, 19, 8]);
let tick, last;
const ctx = {
  createImageData: () => ({data: new Uint8ClampedArray(128 * 32 * 4)}),
  putImageData: image => { last = Uint8Array.from(image.data); }
};
const elements = {oled: {getContext: () => ctx}, mood: {value: '0'}, pause: {}};
vm.runInNewContext(source, {
  Uint8Array, atob,
  document: {getElementById: id => elements[id]},
  setInterval: fn => { tick = fn; }
});
let index = 0;
for (let group = 0; group < counts.length; group++) {
  for (let item = 0; item < counts[group]; item++, index++) {
    elements.mood.value = String(index);
    elements.mood.onchange();
    for (let frame = 0; frame <= 30; frame++) tick(); // t=1980 ms at the preview's 66 ms interval
    const actual = Buffer.alloc(512);
    for (let y = 0; y < 32; y++) for (let x = 0; x < 128; x++) {
      const pixel = (y * 128 + x) * 4;
      assert.strictEqual(last[pixel + 3], 255);
      if (last[pixel]) actual[x + (y >> 3) * 128] |= 1 << (y & 7);
    }
    assert.deepStrictEqual(actual, captures[group].subarray(item * 512, (item + 1) * 512), `sequence ${index}`);
  }
}
const button = {textContent: ''};
elements.pause.onclick({target: button}); tick();
const paused = last;
tick(); assert.deepStrictEqual(last, paused);
assert.strictEqual(button.textContent, 'Reproducir');
console.log(`Preview: ${index} sequences match native C++ pixels; pause works`);
