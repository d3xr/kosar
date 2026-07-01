const bars = document.getElementById('bars');
const terminal = document.getElementById('terminal');
const speedEl = document.getElementById('speed');
const steerEl = document.getElementById('steer');
const batteryEl = document.getElementById('battery');
const heapEl = document.getElementById('heap');

const labels = ['ROLL','PITCH','THR','YAW','ARM','MODE','AUX3','RTH'];
labels.forEach((label, index) => {
  const row = document.createElement('div');
  row.className = 'bar';
  row.innerHTML = `<span>${label}</span><i></i><b id="ch${index}">1500</b>`;
  bars.appendChild(row);
});

const logLines = [
  ['boot', 'Kosar dashboard mock replay online'],
  ['wifi', 'public site наблюдает, управление остается на ESP32'],
  ['hover', 'uart telemetry contract собирается'],
  ['stack', 'моторный контур локальный, драйв локальный'],
];

let tick = 0;
function paint() {
  tick += 1;
  const speed = Math.round(Math.sin(tick / 14) * 420);
  const steer = Math.round(Math.cos(tick / 19) * 260);
  speedEl.textContent = speed;
  steerEl.textContent = steer;
  batteryEl.textContent = (38.4 + Math.sin(tick / 50) * .2).toFixed(1);
  heapEl.textContent = Math.round(181 + Math.cos(tick / 20) * 4);

  labels.forEach((_, index) => {
    const us = index < 4
      ? 1500 + Math.round(Math.sin(tick / 12 + index) * 380)
      : (index === 4 ? 1000 + ((tick >> 5) % 2) * 1000 : 1500);
    const pct = Math.max(0, Math.min(100, (us - 988) / (2012 - 988) * 100));
    const row = bars.children[index];
    row.querySelector('i').style.setProperty('--w', `${pct}%`);
    row.querySelector('b').textContent = us;
  });

  if (tick % 10 === 1) {
    const [type, msg] = logLines[(tick / 10 | 0) % logLines.length];
    const line = document.createElement('div');
    line.innerHTML = `<b>${type}</b> ${msg}`;
    terminal.appendChild(line);
    while (terminal.children.length > 8) terminal.removeChild(terminal.firstChild);
    terminal.scrollTop = terminal.scrollHeight;
  }
}

setInterval(paint, 240);
paint();
