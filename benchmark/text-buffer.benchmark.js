const assert = require('assert')
const {TextBuffer} = require('..')

const text = 'abc def ghi jkl\n'.repeat(1024 * 1024)
const lines = text.split('\n')
const buffer = new TextBuffer(text)
const trialCount = 10

function benchmarkSearch(description, pattern, expectedPosition) {
  let name = `Search for ${description} - TextBuffer`
  console.time(name)
  for (let i = 0; i < trialCount; i++) {
    assert.deepEqual(buffer.searchSync(pattern), expectedPosition)
  }
  console.timeEnd(name)

  name = `Search for ${description} - lines array`
  console.time(name)
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
  console.timeEnd(name)
  console.log()
}

benchmarkSearch('simple non-existent pattern', '\t', null)
benchmarkSearch('complex non-existent pattern', '123|456|789', null)
benchmarkSearch('simple existing pattern', 'jkl', {start: {row: 0, column: 12}, end: {row: 0, column: 15}})
benchmarkSearch('complex existing pattern', 'j\\w+', {start: {row: 0, column: 12}, end: {row: 0, column: 15}})