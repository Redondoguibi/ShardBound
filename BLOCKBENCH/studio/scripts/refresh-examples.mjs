import { readFile, writeFile, mkdir, readdir } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const root = fileURLToPath(new URL('../', import.meta.url)), run = path.resolve(process.argv[2] || '');
if (!process.argv[2] || !run.startsWith(path.join(root, '.local/e2e') + path.sep)) throw new Error('Informe uma execução de testes em .local/e2e/.');
const report = JSON.parse(await readFile(path.join(run, 'test-report.json')));
if (report.status !== 'passed' || report.glb.some(result => result.errors)) throw new Error('A execução não passou em todas as verificações.');
const examples = path.join(root, 'examples'); await mkdir(examples, { recursive: true });
for (const [source, destination] of [['player.sbstudio', 'player.sbstudio'], ['studio-demo.sbstudio', 'authoring-lab.sbstudio'], ['crystal-sword.sbstudio', 'crystal-sword.sbstudio']]) {
  const bytes = await readFile(path.join(run, source)), project = JSON.parse(bytes);
  if (project.meta?.model_format !== 'shardbound_studio' || !project.shardbound_studio.id.startsWith(source === 'player.sbstudio' ? 'test:' : 'studio_demo:')) throw new Error('A origem não é um fixture Studio.');
  await writeFile(path.join(examples, destination), bytes);
}
async function copyPackage(source, target) {
  await mkdir(target, { recursive: true });
  for (const entry of await readdir(source, { withFileTypes: true })) {
    const src = path.join(source, entry.name), dest = path.join(target, entry.name);
    if (entry.isDirectory()) await copyPackage(src, dest);
    else if (entry.isFile() && !entry.name.endsWith('.validation.json') && /\.(json|png|glb|wav|ogg|mp3)$/.test(entry.name)) await writeFile(dest, await readFile(src));
  }
}
await copyPackage(path.join(run, 'sword-package'), path.join(examples, 'crystal-sword-package'));
console.log('Refreshed original examples from verified run: ' + path.basename(run));
