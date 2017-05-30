#!/usr/bin/env node

const fs = require('fs')
const path = require('path')
const {spawnSync} = require('child_process')

const testsPath = path.resolve(__dirname, '..', 'build', 'Debug', 'tests')
const dotPath = path.resolve(__dirname, '..', 'build', 'debug.dot')
const htmlPath = path.join(__dirname, '..', 'build', 'debug.html')

if (fs.existsSync(testsPath)) {
  run('node-gyp', ['build'])
} else {
  run('node-gyp', ['rebuild', '--debug', '--tests'])
}

const args = process.argv.slice(2)

switch (args[0]) {
  case '-d':
  case '--debug':
    args.shift()
    run('lldb', [testsPath, '--', ...args])
    break

  case '-v':
  case '--valgrind':
    args.shift()
    run('valgrind', ['--leak-check=full', testsPath, args[0]])
    break

  case '-s':
  case '--svg':
    args.shift()

    let dotFile = fs.openSync(dotPath, 'w')
    const {status} = spawnSync(testsPath, args, {stdio: ['ignore', 1, dotFile]})
    fs.closeSync(dotFile)

    dotFile = fs.openSync(dotPath, 'r')
    let htmlFile = fs.openSync(htmlPath, 'w')
    fs.writeSync(htmlFile, '<!doctype HTML>\n<style>svg {width: 100%;}</style>\n')
    spawnSync('dot', ['-Tsvg'], {stdio: [dotFile, htmlFile, 2]})
    spawnSync('open', [htmlPath])

    process.exit(status)
    break

  default:
    run(testsPath, args)
    break
}

function run(command, args = [], options = {stdio: 'inherit'}) {
  const {status} = spawnSync(command, args, options)
  if (status !== 0) process.exit(status)
}