#!/usr/bin/env node

const fs = require('fs');
const path = require('path')
const temp = require("temp");
const spawn = require("child_process").spawn;
const rootPath = path.join(__dirname, '..')

function profileCommand (command, containingFunctionName, callback) {
  const dtraceOutputFile = temp.openSync({prefix: 'dtrace.out'});
  const flamegraphFile = temp.openSync({prefix: 'flamegraph', suffix: '.html'});
  fs.chmodSync(flamegraphFile.path, '755');

  const dtraceProcess = spawn("dtrace", [
    "-x", "ustackframes=100",
    "-n", "profile-2000 /pid == $target/ { @num[ustack()] = count(); }",
    "-c", command
  ], {
    stdio: ['ignore', dtraceOutputFile.fd, process.stderr]
  });

  dtraceProcess.on('close', function(code) {
    if (code !== 0) {
      return callback(code);
    }

    const stackvisProcess = spawn(path.join(rootPath, 'node_modules', '.bin', 'stackvis'), [], {
      stdio: ['pipe', flamegraphFile.fd, process.stderr]
    });

    let bufferedDtraceContent = ''
    fs.closeSync(dtraceOutputFile.fd);
    console.log("Wrote stacks to", dtraceOutputFile.path);

    const dtraceOutput = fs.createReadStream(dtraceOutputFile.path, 'utf8')
    dtraceOutput.on('data', (cchange) => {
      bufferedDtraceContent += cchange.toString('utf8')

      while (true) {
        const stackEndIndex = bufferedDtraceContent.indexOf('\n\n')
        if (stackEndIndex == -1) break

        const stack = bufferedDtraceContent.slice(0, stackEndIndex)
        const containingFunctionIndex = stack.indexOf(containingFunctionName)
        if (containingFunctionIndex != -1) {
          const truncatedEndIndex = stack.indexOf('\n', containingFunctionIndex)
          const truncatedStack = stack.slice(0, truncatedEndIndex) + stack.slice(stack.lastIndexOf('\n'))
          stackvisProcess.stdin.write(truncatedStack + '\n')
        }

        bufferedDtraceContent = bufferedDtraceContent.slice(stackEndIndex + 1)
      }
    })

    dtraceOutput.on('end', () => {
      stackvisProcess.stdin.end();

      stackvisProcess.on('close', function(code) {
        if (code !== 0) {
          return callback(code);
        }

        console.log("Opening", flamegraphFile.path);
        spawn('open', [flamegraphFile.path]);
        callback(0);
      });
    })
  });
}

profileCommand(process.argv[2], process.argv[3], process.exit)
