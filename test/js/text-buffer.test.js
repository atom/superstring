const {assert} = require('chai')
const temp = require('temp').track()
const {TextBuffer} = require('../..')
const fs = require('fs')

describe('TextBuffer', () => {
  describe('.load', () => {
    if (!TextBuffer.prototype.load) return;

    it('loads the contents of the given filePath in the background', () => {
      const buffer = new TextBuffer()

      const {path: filePath} = temp.openSync()
      const content = 'a\nb\nc\n'.repeat(10 * 1024)
      fs.writeFileSync(filePath, content)

      const progressValues = []

      return buffer.load(filePath, (i) => progressValues.push(i))
        .then(() => {
          assert.equal(buffer.getText(), content)

          assert.deepEqual(progressValues, progressValues.map(Number).sort())
          assert(progressValues[0] >= 0)
          assert(progressValues[progressValues.length - 1] <= content.length)
        })
    })

    it('can load a file in a non-UTF8 encoding', () => {
      const buffer = new TextBuffer()

      const {path: filePath} = temp.openSync()
      const content = 'a\nb\nc\n'.repeat(10)
      fs.writeFileSync(filePath, content, 'utf16le')

      return buffer.load(filePath, 'UTF-16LE')
        .then(() => assert.equal(buffer.getText(), content))
    })

    it('rejects its promise if an invalid encoding is given', () => {
      const buffer = new TextBuffer()

      const {path: filePath} = temp.openSync()
      const content = 'a\nb\nc\n'.repeat(10)
      fs.writeFileSync(filePath, content, 'utf16le')

      let rejection = null
      return buffer.load(filePath, 'GARBAGE-16')
        .catch(error => rejection = error)
        .then(() => assert.equal(rejection.message, 'Invalid encoding name: GARBAGE-16'))
    })
  })

  describe('.save', () => {
    if (!TextBuffer.prototype.save) return;

    it('writes the buffer\'s content to the given file', () => {
      const buffer = new TextBuffer('abcdefghijklmnopqrstuvwxyz')

      // Perform some edits just to exercise the saving logic
      buffer.setTextInRange(Range(Point(0, 3), Point(0, 3)), '123')
      buffer.setTextInRange(Range(Point(0, 7), Point(0, 7)), '456')
      assert.equal(buffer.getText(), 'abc123d456efghijklmnopqrstuvwxyz')

      const {path: filePath} = temp.openSync()
      const savePromise = buffer.save(filePath)

      buffer.setTextInRange(Range(Point(0, 11), Point(0, 11)), '789')
      assert.equal(buffer.getText(), 'abc123d456e789fghijklmnopqrstuvwxyz')

      return savePromise.then(() => {
        assert.equal(fs.readFileSync(filePath, 'utf8'), 'abc123d456efghijklmnopqrstuvwxyz')
        assert.equal(buffer.getText(), 'abc123d456e789fghijklmnopqrstuvwxyz')
      })
    })
  })

  describe('.isModified', () => {
    it('indicates whether the buffer changed since its construction', () => {
      const buffer = new TextBuffer('abc')
      assert.notOk(buffer.isModified())

      buffer.setTextInRange(Range(Point(0, 2), Point(0, 2)), ' ')
      assert.ok(buffer.isModified())
    })

    it('returns false after the buffer is loaded', () => {
      const buffer = new TextBuffer('abc')
      buffer.setTextInRange(Range(Point(0, 2), Point(0, 2)), ' ')

      const {path: filePath} = temp.openSync()
      fs.writeFileSync(filePath, 'defg')

      return buffer.load(filePath).then(() => {
        assert.equal(buffer.getText(), 'defg')
        assert.notOk(buffer.isModified())
      })
    })
  })

  describe('.setTextInRange', () => {
    it('mutates the buffer', () => {
      const buffer = new TextBuffer()

      buffer.setText('abc\ndef\nghi')
      assert.equal(buffer.getText(), 'abc\ndef\nghi')

      buffer.setTextInRange(Range(Point(0, 2), Point(1, 1)), 'Y')
      assert.equal(buffer.getText(), 'abYef\nghi')

      buffer.setTextInRange(Range(Point(0, 1), Point(5, 5)), 'O')
      assert.equal(buffer.getText(), 'aO')
    })
  })

  describe('.getTextInRange', () => {
    it('mutates the buffer', () => {
      const buffer = new TextBuffer()
      buffer.setText('abc\ndef\nghi')
      assert.equal(buffer.getTextInRange(Range(Point(0, 1), Point(0, 2))), 'b')
      assert.equal(buffer.getTextInRange(Range(Point(0, 1), Point(1, 2))), 'bc\nde')
      assert.equal(buffer.getTextInRange(Range(Point(0, 1), Point(2, 8))), 'bc\ndef\nghi')
    })
  })

  describe('.lineForRow, .lineLengthForRow, and .lineEndingForRow', () => {
    it('returns the properties of the given line of text', () => {
      const buffer = new TextBuffer()
      buffer.setText('abc\r\ndefg\n\r\nhijkl')

      assert.equal(buffer.lineForRow(0), 'abc')
      assert.equal(buffer.lineForRow(1), 'defg')
      assert.equal(buffer.lineForRow(2), '')
      assert.equal(buffer.lineForRow(3), 'hijkl')
      assert.equal(buffer.lineForRow(4), undefined)

      assert.equal(buffer.lineLengthForRow(0), 3)
      assert.equal(buffer.lineLengthForRow(1), 4)
      assert.equal(buffer.lineLengthForRow(2), 0)
      assert.equal(buffer.lineLengthForRow(3), 5)
      assert.equal(buffer.lineLengthForRow(5), undefined)

      assert.equal(buffer.lineEndingForRow(0), '\r\n')
      assert.equal(buffer.lineEndingForRow(1), '\n')
      assert.equal(buffer.lineEndingForRow(2), '\r\n')
      assert.equal(buffer.lineEndingForRow(3), '')
      assert.equal(buffer.lineEndingForRow(5), undefined)
    })
  })

  describe('.getLength and .getExtent', () => {
    it('returns the total length and total extent of the text', () => {
      const buffer = new TextBuffer()
      buffer.setText('abc\r\ndefg\n\r\nhijkl')
      assert.equal(buffer.getLength(), buffer.getText().length)
      assert.deepEqual(buffer.getExtent(), Point(3, 5))
    })
  })

  describe('.characterIndexForPosition and .positionForCharacterIndex', () => {
    it('returns the character index for the given position', () => {
      const buffer = new TextBuffer()
      buffer.setText('abc\r\ndefg\n\r\nhijkl')

      assert.equal(buffer.characterIndexForPosition(Point(0, 2)), 2)
      assert.deepEqual(buffer.positionForCharacterIndex(2), Point(0, 2))
      assert.equal(buffer.characterIndexForPosition(Point(0, 3)), 3)
      assert.deepEqual(buffer.positionForCharacterIndex(3), Point(0, 3))
      assert.equal(buffer.characterIndexForPosition(Point(0, 4)), 3)
      assert.equal(buffer.characterIndexForPosition(Point(0, Infinity)), 3)
      assert.deepEqual(buffer.positionForCharacterIndex(4), Point(0, 3))
      assert.equal(buffer.characterIndexForPosition(Point(1, 0)), 5)
      assert.deepEqual(buffer.positionForCharacterIndex(5), Point(1, 0))
      assert.equal(buffer.characterIndexForPosition(Point(1, 1)), 6)
      assert.deepEqual(buffer.positionForCharacterIndex(6), Point(1, 1))

      assert.equal(buffer.characterIndexForPosition(Point(-1, -1)), 0)
      assert.deepEqual(buffer.positionForCharacterIndex(-1), Point(0, 0))
    })
  })

  describe('.baseTextDigest', () => {
    it('returns a hash of the base text', () => {
      const buffer = new TextBuffer('abc\r\ndefg\n\r\nhijkl')
      const digest1 = buffer.baseTextDigest()

      buffer.setTextInRange(Range(Point(0, 0), Point(0, 1)), 'A')
      assert.equal(buffer.baseTextDigest(), digest1)

      const buffer2 = new TextBuffer('abc\r\ndefg\n\r\nhijkl')
      assert.equal(buffer2.baseTextDigest(), digest1)
    })
  })

  describe('.serializeChanges and .deserializeChanges', () => {
    it('allows the outstanding changes to be serialized and restored', () => {
      const buffer = new TextBuffer('abc')
      buffer.setTextInRange(Range(Point(0, 0), Point(0, 0)), '\n')
      buffer.setTextInRange(Range(Point(1, 3), Point(1, 3)), 'D')
      assert.equal(buffer.getText(), '\nabcD')

      const changes = buffer.serializeChanges()
      const buffer2 = new TextBuffer('123')
      assert.equal(buffer2.getText(), '123')
      buffer2.deserializeChanges(changes)
      assert.equal(buffer2.getText(), '\n123D')
    })
  })
})

function Range(start, end) {
  return {start, end}
}

function Point(row, column) {
  return {row, column}
}