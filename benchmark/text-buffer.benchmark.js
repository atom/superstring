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
    assert.equal(buffer.search(pattern), expectedPosition)
  }
  console.timeEnd(name)

  name = `Search for ${description} - string`
  console.time(name)
  for (let i = 0; i < trialCount; i++) {
    assert.equal(text.search(pattern), expectedPosition)
  }
  console.timeEnd(name)

  name = `Search for ${description} - lines array`
  console.time(name)
  for (let i = 0; i < trialCount; i++) {
    let position = 0
    for (let row = 0, rowCount = lines.length; row < rowCount; row++) {
      let linePosition = lines[row].search(pattern)
      if (linePosition !== -1) {
        position += linePosition
        assert.equal(position, expectedPosition)
        break
      }
      position += lines[row].length
    }
  }
  console.timeEnd(name)
  console.log()
}

benchmarkSearch('simple non-existent pattern', /\t/, -1)
benchmarkSearch('complex non-existent pattern', /123|456|789/, -1)
benchmarkSearch('simple existing pattern', /jkl/, 12)
benchmarkSearch('complex existing pattern', /j\w+/, 12)