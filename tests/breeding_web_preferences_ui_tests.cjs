// Execute the actual page script with a small DOM/API harness.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const html = fs.readFileSync(path.join(__dirname, '../tools/breeding_web.html'), 'utf8');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
let preferences = {assist: true, food_assist: true};
const latest = {planned_pairs: 4, target_pairs: 4, selected_pairs: [[1,2],[3,4],[5,6],[7,8]],
  all_seven_population: 8, population: 20, replacement_pairs: 2, window_ratio: 1,
  window_days: 1, window_births: 2, food_remaining: 30, assisted_births: 2, removed_dead: []};

async function openPage() {
  const elements = new Map();
  const element = id => {
    if (!elements.has(id)) elements.set(id, {checked: true, disabled: true, textContent: '',
      replaceChildren() {}, append() {}, setAttribute() {}});
    return elements.get(id);
  };
  const context = vm.createContext({
    document: {getElementById: element, createElement: () => ({append() {}})},
    setInterval() {},
    async fetch(url, options) {
      if (url === '/api/preferences') preferences = JSON.parse(options.body);
      return {ok: true, async json() {
        return url === '/api/state' ? {token: 'test', preferences, saves: [], game_running: false,
          job: {status: 'running', config: {breeding_pairs: 4}, latest, history: [latest]}} : {ok: true};
      }};
    },
  });
  vm.runInContext(script, context);
  await new Promise(setImmediate);
  assert.equal(element('assist').disabled, false);
  return {context, element};
}

(async () => {
  let page = await openPage();
  assert.equal(page.element('assist').checked, true);
  assert.equal(page.element('food-assist').checked, true);
  page.element('assist').checked = false;
  page.element('assist').onchange();
  page.element('food-assist').checked = false;
  page.element('food-assist').onchange();
  await vm.runInContext('preferenceWrite', page.context);
  assert.deepEqual(preferences, {assist: false, food_assist: false});
  page = await openPage();
  assert.equal(page.element('assist').checked, false);
  assert.equal(page.element('food-assist').checked, false);
  page.element('food-assist').checked = true;
  page.element('food-assist').onchange();
  await vm.runInContext('preferenceWrite', page.context);
  page = await openPage();
  assert.equal(page.element('assist').checked, false);
  assert.equal(page.element('food-assist').checked, true);
  await vm.runInContext('poll()', page.context);
  assert.equal(page.element('planned').textContent, '4 / 4');
  assert.equal(page.element('replacement').textContent, '≥2');
  console.log('PASS actual page script: defaults, changes, reopen restore, 4 / 4 display');
})().catch(error => {console.error(error); process.exitCode = 1;});
