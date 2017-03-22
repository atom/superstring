const {assert} = require('chai')
const temp = require('temp').track()
const {TextBuffer} = require('../..')
const fs = require('fs')

describe('TextBuffer', () => {
  describe('load()', () => {
    it('loads the contents of the given filePath in the background', () => {
      const buffer = new TextBuffer()

      const {path: filePath} = temp.openSync()
      const content = 'a\nb\nc\n'.repeat(10 * 1024)
      fs.writeFileSync(filePath, content)

      const progressValues = []

      return buffer.load(
        filePath,
        (i) => progressValues.push(i)
      ).then(() => {
        assert.equal(buffer.getText(), content)

        assert.deepEqual(progressValues, progressValues.map(Number).sort())
        assert(progressValues[0] >= 0)
        assert(progressValues[progressValues.length - 1] <= content.length)
      })
    })
  })
})
