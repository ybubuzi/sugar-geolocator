import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const addon = require('../build/Release/addon.node');
const promise = addon.GetPosition()
console.log(promise)