import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import Ajv from 'ajv';
import { defaultEffect, projectState } from '../src/core.mjs';
const schema = JSON.parse(await readFile(new URL('../schemas/studio-v1.schema.json', import.meta.url)));
const validate = new Ajv({ allErrors: true }).compile(schema);
test('export contract validates effects and rejects unsupported versions or malformed authored resources', () => {
  const document = { schema: 'shardbound.studio.effects', version: 1, generator: 'test', project: 'test:project',
    renderContract: projectState().renderContract, definitions: [defaultEffect()] };
  assert.equal(validate(document), true, JSON.stringify(validate.errors));
  assert.equal(validate({ ...document, version: 999 }), false);
  assert.equal(validate({ ...document, definitions: [{ ...defaultEffect(), id: '..:unsafe' }] }), false);
  assert.equal(validate({ ...document, definitions: [{ ...defaultEffect(), lifetime: 0 }] }), false);
  assert.equal(validate({ ...document, definitions: [{ ...defaultEffect(), curves: { color: [{ age: 0, value: 'blue' }, { age: 1, value: 'red' }] } }] }), false);
});
