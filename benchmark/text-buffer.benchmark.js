console.log(' running text-buffer tests... \n')

const assert = require('assert')
const {performance} = require("perf_hooks")

const {TextBuffer} = require('..')

const text = 'abc def ghi jkl\n'.repeat(1024 * 1024)
const lines = text.split('\n')
const buffer = new TextBuffer(text)
const trialCount = 10

function benchmarkSearch(description, pattern, expectedPosition) {
  let name = `Search for ${description} - TextBuffer`
  const ti1 = performance.now()
  for (let i = 0; i < trialCount; i++) {
    assert.deepEqual(buffer.findSync(pattern), expectedPosition)
  }
  const tf1 = performance.now()
  console.log(`${name} ${' '.repeat(80-name.length)} ${(tf1-ti1).toFixed(3)} ms`)

  name = `Search for ${description} - lines array`
  const ti2 = performance.now()
  const regex = new RegExp(pattern)
  for (let i = 0; i < trialCount; i++) {
    for (let row = 0, rowCount = lines.length; row < rowCount; row++) {
      let match = regex.exec(lines[row])
      if (match) {
        assert.deepEqual(
          {
            start: {row, column: match.index},
            end: {row, column: match.index + match[0].length}
          },
          expectedPosition
        )
        break
      }
    }
  }
  const tf2 = performance.now()
  console.log(`${name} ${' '.repeat(80-name.length)} ${(tf2-ti2).toFixed(3)} ms`)
}

benchmarkSearch('simple non-existent pattern', '\t', null)
benchmarkSearch('complex non-existent pattern', '123|456|789', null)
benchmarkSearch('simple existing pattern', 'jkl', {start: {row: 0, column: 12}, end: {row: 0, column: 15}})
benchmarkSearch('complex existing pattern', 'j\\w+', {start: {row: 0, column: 12}, end: {row: 0, column: 15}})


console.log('\n text-buffer finished \n')