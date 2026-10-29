import {mkdir, copyFile, readdir} from 'node:fs/promises';
await mkdir('dist', {recursive:true});
for (const file of ['index.html','styles.css','app.js','core.js','skills.js','models.json','llm-worker.js','server-config.json']) await copyFile(file, `dist/${file}`);
console.log('Astheral static build ready.');
