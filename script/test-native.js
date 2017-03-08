#!/usr/bin/env node

const fs = require('fs')
const path = require('path')
const {spawnSync} = require('child_process')

const testsPath = path.resolve(__dirname, '..', 'build', 'Debug', 'tests')

if (fs.existsSync(testsPath)) {
  run('node-gyp', ['build'])
} else {
  run('node-gyp', ['rebuild', '--debug', '--tests'])
}

const args = process.argv.slice(2)

if (args[0] == '-d') {
  args.shift()
  run('lldb', [testsPath, ...args])
} else {
  run(testsPath, args)
}

function run(command, args = []) {
  const {status} = spawnSync(command, args, {stdio: 'inherit'})
  if (status !== 0) process.exit(status)
}