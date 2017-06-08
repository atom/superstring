const fs = require('fs')
const path = require('path')
const temp = require('temp').track()
const {assert} = require('chai')
const {TextBuffer} = require('../..')
const Random = require('random-seed')
const TestDocument = require('./helpers/test-document')
const {traverse} = require('./helpers/point-helpers')
const {getExtent} = require('./helpers/text-helpers')
const MAX_INT32 = 4294967296

const isWindows = process.platform === 'win32'

const encodings = [
  'big5hkscs',
  'cp437',
  'cp850',
  'cp866',
  'cp932',
  'cp950',
  'eucjp',
  'euckr',
  'gb18030',
  'gbk',
  'ISO-8859-4',
  'iso88591',
  'iso885913',
  'iso885915',
  'iso88592',
  'iso88593',
  'iso88594',
  'iso88595',
  'iso88596',
  'iso88597',
  'iso88598',
  'iso88599',
  'koi8r',
  'koi8u',
  'shift_jis',
  'shiftjis',
  'UTF-16-LE',
  'UTF-8',
  'utf16be',
  'UTF16BE',
  'utf16le',
  'UTF16LE',
  'utf8',
  'UTF8',
  'WINDOWS_1252',
  'windows1250',
  'windows1251',
  'windows1252',
  'windows1253',
  'windows1254',
  'windows1255',
  'windows1256',
  'windows1257',
  'windows1258'
]

if (process.platform === 'darwin') {
  encodings.push(
    'macroman',
    'MAC-ROMAN'
  )
}

if (process.platform !== 'win32') {
  encodings.push(
    'iso885910',
    'iso885914',
    'iso885916'
  )
}

describe('TextBuffer', () => {
  describe('.load', () => {
    if (!TextBuffer.prototype.load) return;

    it('can load from a file at a given path', () => {
      const buffer = new TextBuffer()

      const {path: filePath} = temp.openSync()
      const content = 'a\nb\nc\n'.repeat(10 * 1024)
      fs.writeFileSync(filePath, content)

      const percentages = []
      return buffer.load(filePath, (percentDone) => percentages.push(percentDone))
        .then(() => {
          assert.equal(buffer.getText(), content)
          assert.deepEqual(percentages, percentages.map(Number).sort((a, b) => a - b))
          assert(percentages[0] >= 0)
          assert(percentages[percentages.length - 1] == 100)
        })
    })

    it('can load from a given stream', () => {
      const buffer = new TextBuffer()

      const {path: filePath} = temp.openSync()
      const fileContent = 'abc def ghi jkl\n'.repeat(10 * 1024)
      fs.writeFileSync(filePath, fileContent)

      const percentages = []
      const stream = fs.createReadStream(filePath)
      return buffer.load(stream, (percentage) => percentages.push(percentage))
        .then(() => {
          assert.equal(buffer.getText(), fileContent)
          assert.deepEqual(percentages, percentages.map(Number).sort((a, b) => a - b))
          assert(percentages[percentages.length - 1] == 100)
        })
    })

    it('can load from a given stream with an encoding already set', () => {
      const buffer = new TextBuffer()

      const {path: filePath} = temp.openSync()
      const fileContent = 'abc def ghi jkl\n'.repeat(1024)
      fs.writeFileSync(filePath, fileContent)

      const stream = fs.createReadStream(filePath, 'utf8')
      return buffer.load(stream).then(() => {
        assert.equal(buffer.getText(), fileContent)
      })
    })

    it('resolves with a Patch representing the difference between the old and new text', () => {
      const buffer = new TextBuffer('cat\ndog\nelephant\nfox')
      const {path: filePath} = temp.openSync()
      fs.writeFileSync(filePath, 'bug\ncat\ndog\nelephant\nfox\ngoat')

      let progressCallbackPatch
      return buffer.load(filePath, (percentDone, patch) => {
        progressCallbackPatch = patch
      }).then(patch => {

        // The patch is also passed to the progress callback on the final call.
        assert.equal(progressCallbackPatch, patch)

        assert.deepEqual(toPlainObject(patch.getChanges()), [
          {
            oldStart: {row: 0, column: 0}, oldEnd: {row: 0, column: 0},
            newStart: {row: 0, column: 0}, newEnd: {row: 1, column: 0},
            oldText: '',
            newText: 'bug\n'
          },
          {
            oldStart: {row: 3, column: 3}, oldEnd: {row: 3, column: 3},
            newStart: {row: 4, column: 3}, newEnd: {row: 5, column: 4},
            oldText: '',
            newText: '\ngoat'
          }
        ])
      })
    })

    it('resolves with an empty patch when the contents of the file have not changed', () => {
      const buffer = new TextBuffer('cat\ndog\nelephant\nfox')
      const {path: filePath} = temp.openSync()
      fs.writeFileSync(filePath, 'cat\ndog\nelephant\nfox')

      return buffer.load(filePath).then(patch => {
        assert.deepEqual(patch.getChanges(), [])
        assert.equal(buffer.getText(), 'cat\ndog\nelephant\nfox')
      })
    })

    it('can load a file in a non-UTF8 encoding', () => {
      const buffer = new TextBuffer()

      const {path: filePath} = temp.openSync()
      const content = 'a\nb\nc\n'.repeat(10)
      fs.writeFileSync(filePath, content, 'utf16le')

      return buffer.load(filePath, 'UTF-16LE').then(() =>
        assert.equal(buffer.getText(), content)
      )
    })

    it('rejects its promise if an invalid encoding is given', () => {
      const buffer = new TextBuffer()

      const {path: filePath} = temp.openSync()
      const content = 'a\nb\nc\n'.repeat(10)
      fs.writeFileSync(filePath, content, 'utf16le')

      let rejection = null
      return buffer.load(filePath, 'GARBAGE16')
        .catch(error => rejection = error)
        .then(() => assert.equal(rejection.message, 'Invalid encoding name: GARBAGE16'))
    })

    it('aborts if the buffer is modified before the load', () => {
      const buffer = new TextBuffer('abc')
      const {path: filePath} = temp.openSync()
      fs.writeFileSync(filePath, 'def')

      buffer.setText('ghi')

      return buffer.load(filePath).then(result => {
        assert.equal(result, null)
        assert.equal(buffer.getText(), 'ghi')
        assert.ok(buffer.isModified())
      })
    })

    it('aborts if the buffer is modified during the load', () => {
      const buffer = new TextBuffer('abc')
      const {path: filePath} = temp.openSync()
      fs.writeFileSync(filePath, 'def')

      const loadPromise = buffer.load(filePath).then(result => {
        assert.equal(result, null)
        assert.equal(buffer.getText(), 'ghi')
        assert.ok(buffer.isModified())
      })

      buffer.setText('ghi')
      return loadPromise
    })

    it('aborts if the progress callback returns false during the load', () => {
      const buffer = new TextBuffer('abc')
      const filePath = temp.openSync().path
      fs.writeFileSync(filePath, '123456789\n'.repeat(100))

      let progressCount = 0
      function progressCallback() {
        progressCount++
        return false
      }

      return buffer.load(filePath, progressCallback).then((patch) => {
        assert.notOk(patch)
        assert.equal(progressCount, 1)
        assert.equal(buffer.getText(), 'abc')
        assert.notOk(buffer.isModified())
      })
    })

    it('aborts if the final progress callback returns false', () => {
      const buffer = new TextBuffer('abc')
      const filePath = temp.openSync().path

      function progressCallback(percentDone, patch) {
        if (patch) return false
      }

      return buffer.load(filePath, progressCallback).then((patch) => {
        assert.notOk(patch)
        assert.equal(buffer.getText(), 'abc')
        assert.notOk(buffer.isModified())
      })
    })

    it('can handle a variety of encodings', () => {
      const filePath = temp.openSync().path
      fs.writeFileSync(filePath, 'abc', 'ascii')

      return Promise.all(encodings.map((encoding) =>
        new TextBuffer().load(filePath, encoding)
      ))
    })

    describe('error handling', () => {
      it('rejects with an error if the path points to a directory', (done) => {
        const buffer = new TextBuffer()
        const filePath = temp.mkdirSync()

        return buffer.load(filePath)
          .then(() => {
            done(new Error('Expected an error'))
          })
          .catch((error) => {
            if (!isWindows) assert.equal(error.code, 'EISDIR')
            assert.equal(error.path, filePath)
            done()
          })
      })

      it('rejects with an error if the path is a circular symlink', (done) => {
        const tempDir = temp.mkdirSync()
        const filePath = path.join(tempDir, 'one')
        const otherPath = path.join(tempDir, 'two')
        fs.symlinkSync(filePath, otherPath)
        fs.symlinkSync(otherPath, filePath)

        const buffer = new TextBuffer()
        return buffer.load(filePath)
          .then(() => {
            done(new Error('Expected an error'))
          })
          .catch((error) => {
            if (!isWindows) assert.equal(error.code, 'ELOOP')
            assert.equal(error.path, filePath)
            done()
          })
      })
    })
  })

  describe('.reload', () => {
    if (!TextBuffer.prototype.reload) return;

    it('discards any modifications and incorporates that change into the resolved patch', () => {
      const buffer = new TextBuffer('abcdef')
      const {path: filePath} = temp.openSync()
      fs.writeFileSync(filePath, '  abcdef')

      buffer.setTextInRange(Range(Point(0, 3), Point(0, 3)), ' ')
      assert.equal(buffer.getText(), 'abc def')
      assert.ok(buffer.isModified())

      const loadPromise = buffer.reload(filePath).then(patch => {
        assert.equal(buffer.getText(), '  abcdef')
        assert.notOk(buffer.isModified())
        assert.deepEqual(toPlainObject(patch.getChanges()), [
          {
            oldStart: {row: 0, column: 0}, oldEnd: {row: 0, column: 0},
            newStart: {row: 0, column: 0}, newEnd: {row: 0, column: 2},
            oldText: '',
            newText: '  '
          },
          {
            oldStart: {row: 0, column: 3}, oldEnd: {row: 0, column: 4},
            newStart: {row: 0, column: 5}, newEnd: {row: 0, column: 5},
            oldText: ' ',
            newText: ''
          },
          {
            oldStart: {row: 0, column: 7}, oldEnd: {row: 0, column: 8},
            newStart: {row: 0, column: 8}, newEnd: {row: 0, column: 8},
            oldText: ' ',
            newText: ''
          }
        ])
      })

      buffer.setTextInRange(Range(Point(0, 7), Point(0, 7)), ' ')
      assert.equal(buffer.getText(), 'abc def ')
      assert.ok(buffer.isModified())

      return loadPromise
    })

    it('marks the buffer as unmodified even if the reload does not change the text', () => {
      const buffer = new TextBuffer('abcdef')

      const {path: filePath} = temp.openSync()
      const fileContent = '  abcdef'
      fs.writeFileSync(filePath, fileContent)

      buffer.setTextInRange(Range(Point(0, 0), Point(0, 0)), '  ')
      assert.ok(buffer.isModified())
      assert.equal(buffer.getText(), fileContent)

      return buffer.reload(filePath).then(patch => {
        assert.equal(patch.getChanges(), 0)
        assert.equal(buffer.getText(), '  abcdef')
        assert.notOk(buffer.isModified())
      })
    })
  })

  describe('.loadSync', () => {
    if (!TextBuffer.prototype.loadSync) return;

    it('returns a Patch representing the difference between the old and new text', () => {
      const buffer = new TextBuffer('cat\ndog\nelephant\nfox')
      const {path: filePath} = temp.openSync()
      fs.writeFileSync(filePath, 'bug\ncat\ndog\nelephant\nfox\ngoat')

      const patch = buffer.loadSync(filePath, 'UTF-8')
      assert.deepEqual(toPlainObject(patch.getChanges()), [
        {
          oldStart: {row: 0, column: 0}, oldEnd: {row: 0, column: 0},
          newStart: {row: 0, column: 0}, newEnd: {row: 1, column: 0},
          oldText: '',
          newText: 'bug\n'
        },
        {
          oldStart: {row: 3, column: 3}, oldEnd: {row: 3, column: 3},
          newStart: {row: 4, column: 3}, newEnd: {row: 5, column: 4},
          oldText: '',
          newText: '\ngoat'
        }
      ])
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

    it('can write the buffer\'s content to a given stream', () => {
      const buffer = new TextBuffer('abcdefghijklmnopqrstuvwxyz')

      // Perform some edits just to exercise the saving logic
      buffer.setTextInRange(Range(Point(0, 3), Point(0, 3)), '123')
      buffer.setTextInRange(Range(Point(0, 7), Point(0, 7)), '456')
      assert.equal(buffer.getText(), 'abc123d456efghijklmnopqrstuvwxyz')

      const {path: filePath} = temp.openSync()
      const savePromise = buffer.save(fs.createWriteStream(filePath))

      buffer.setTextInRange(Range(Point(0, 11), Point(0, 11)), '789')
      assert.equal(buffer.getText(), 'abc123d456e789fghijklmnopqrstuvwxyz')

      return savePromise.then(() => {
        assert.equal(fs.readFileSync(filePath, 'utf8'), 'abc123d456efghijklmnopqrstuvwxyz')
        assert.equal(buffer.getText(), 'abc123d456e789fghijklmnopqrstuvwxyz')

        return buffer.save(fs.createWriteStream(filePath)).then(() => {
          assert.equal(fs.readFileSync(filePath, 'utf8'), 'abc123d456e789fghijklmnopqrstuvwxyz')
          assert.notOk(buffer.isModified())
        })
      })
    })

    it('can write the buffer\'s long content to a given stream', () => {
      const buffer = new TextBuffer('abc def ghi jkl\n'.repeat(10 * 1024))

      const {path: filePath} = temp.openSync()
      const stream = fs.createWriteStream(filePath)
      const savePromise = buffer.save(stream)

      return savePromise.then(() => {
        assert.equal(fs.readFileSync(filePath, 'utf8'), buffer.getText())
      })
    })

    it('handles concurrent saves', () => {
      const {path: filePath1} = temp.openSync()
      const {path: filePath2} = temp.openSync()
      const {path: filePath3} = temp.openSync()
      const {path: filePath4} = temp.openSync()

      const buffer = new TextBuffer('abc def ghi jkl\n'.repeat(10 * 1024))

      buffer.setTextInRange(Range(Point(10, 15), Point(10, 15)), ' mno')
      buffer.setTextInRange(Range(Point(20, 15), Point(20, 15)), ' mno')
      const text1 = buffer.getText()
      const savePromise1 = buffer.save(filePath1)

      buffer.setTextInRange(Range(Point(30, 15), Point(30, 15)), ' mno')
      buffer.setTextInRange(Range(Point(40, 15), Point(40, 15)), ' mno')
      const text2 = buffer.getText()
      const savePromise2 = buffer.save(filePath2)

      buffer.setTextInRange(Range(Point(50, 15), Point(50, 15)), ' mno')
      buffer.setTextInRange(Range(Point(60, 15), Point(60, 15)), ' mno')
      const text3 = buffer.getText()
      const savePromise3 = buffer.save(filePath3)

      buffer.setTextInRange(Range(Point(70, 15), Point(70, 15)), ' mno')
      buffer.setTextInRange(Range(Point(80, 15), Point(80, 15)), ' mno')
      const text4 = buffer.getText()
      const savePromise4 = buffer.save(filePath4)

      return Promise.all([savePromise1, savePromise2, savePromise3, savePromise4]).then(() => {
        assert.equal(fs.readFileSync(filePath1, 'utf8'), text1)
        assert.equal(fs.readFileSync(filePath2, 'utf8'), text2)
        assert.equal(fs.readFileSync(filePath3, 'utf8'), text3)
        assert.equal(fs.readFileSync(filePath4, 'utf8'), text4)

        assert.equal(buffer.getText(), text4)
        assert(!buffer.isModified())
      })
    })

    it('can handle a variety of encodings', () => {
      const {path: filePath} = temp.openSync()

      return Promise.all(encodings.map((encoding) =>
        new TextBuffer('abc').save(filePath, encoding)
      ))
    })

    describe('error handling', () => {
      it('rejects with an error if the path points to a directory', (done) => {
        const buffer = new TextBuffer('hello')
        const filePath = temp.mkdirSync()

        return buffer.save(filePath)
          .then(() => {
            done(new Error('Expected an error'))
          })
          .catch((error) => {
            assert.equal(error.code, isWindows ? 'EACCES' : 'EISDIR')
            assert.equal(error.path, filePath)
            done()
          })
      })

      it('rejects with an error if the path is a circular symlink', (done) => {
        const tempDir = temp.mkdirSync()
        const filePath = path.join(tempDir, 'one')
        const otherPath = path.join(tempDir, 'two')
        fs.symlinkSync(filePath, otherPath)
        fs.symlinkSync(otherPath, filePath)

        const buffer = new TextBuffer('hello')
        return buffer.save(filePath)
          .then(() => {
            done(new Error('Expected an error'))
          })
          .catch((error) => {
            assert.equal(error.code, isWindows ? 'EINVAL' : 'ELOOP')
            assert.equal(error.path, filePath)
            done()
          })
      })
    })
  })

  describe('.isModified', () => {
    it('indicates whether the buffer changed since its construction', () => {
      const buffer = new TextBuffer('abc')
      assert.notOk(buffer.isModified())

      buffer.setTextInRange(Range(Point(0, 2), Point(0, 2)), ' ')
      assert.ok(buffer.isModified())

      buffer.setText('abc')
      assert.notOk(buffer.isModified())
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

    it('causes .isModified to return false when the original text is restored', () => {
      const buffer = new TextBuffer('abc\ndef')
      assert.equal(buffer.isModified(), false)

      buffer.setTextInRange(Range(Point(0, 2), Point(1, 1)), '!')
      assert.equal(buffer.getText(), 'ab!ef')
      assert.equal(buffer.isModified(), true)

      buffer.setTextInRange(Range(Point(0, 2), Point(0, 3)), 'c\nd')
      assert.equal(buffer.getText(), 'abc\ndef')
      assert.equal(buffer.isModified(), false)

      buffer.setText('ok')
      assert.equal(buffer.getText(), 'ok')
      assert.equal(buffer.isModified(), true)

      buffer.setText('abc\ndef')
      assert.equal(buffer.getText(), 'abc\ndef')
      assert.equal(buffer.isModified(), false)
    })
  })

  describe('.getTextInRange', () => {
    it('reads substrings from the buffer', () => {
      const buffer = new TextBuffer()
      buffer.setText('abc\ndef\nghi')
      assert.equal(buffer.getTextInRange(Range(Point(0, 1), Point(0, 2))), 'b')
      assert.equal(buffer.getTextInRange(Range(Point(0, 1), Point(1, 2))), 'bc\nde')
      assert.equal(buffer.getTextInRange(Range(Point(0, 1), Point(2, 8))), 'bc\ndef\nghi')

      assert.equal(buffer.getTextInRange(Range(Point(-Infinity, -Infinity), Point(0, Infinity))), 'abc')
      assert.equal(buffer.getTextInRange(Range(Point(1, -Infinity), Point(1, Infinity))), 'def')
    })
  })

  describe('.lineForRow, .lineLengthForRow, and .lineEndingForRow', () => {
    it('returns the properties of the given line of text', () => {
      const buffer = new TextBuffer('abc\r\ndefg\n\r\nhijkl\n\n')
      buffer.setTextInRange(Range(Point(1, 1), Point(1, 2)), 'EEE')

      assert.equal(buffer.lineForRow(0), 'abc')
      assert.equal(buffer.lineForRow(1), 'dEEEfg')
      assert.equal(buffer.lineForRow(2), '')
      assert.equal(buffer.lineForRow(3), 'hijkl')
      assert.equal(buffer.lineForRow(4), '')
      assert.equal(buffer.lineForRow(5), '')
      assert.equal(buffer.lineForRow(6), undefined)

      assert.equal(buffer.lineLengthForRow(0), 3)
      assert.equal(buffer.lineLengthForRow(1), 6)
      assert.equal(buffer.lineLengthForRow(2), 0)
      assert.equal(buffer.lineLengthForRow(3), 5)
      assert.equal(buffer.lineLengthForRow(4), 0)
      assert.equal(buffer.lineLengthForRow(5), 0)
      assert.equal(buffer.lineLengthForRow(6), undefined)

      assert.equal(buffer.lineEndingForRow(0), '\r\n')
      assert.equal(buffer.lineEndingForRow(1), '\n')
      assert.equal(buffer.lineEndingForRow(2), '\r\n')
      assert.equal(buffer.lineEndingForRow(3), '\n')
      assert.equal(buffer.lineEndingForRow(4), '\n')
      assert.equal(buffer.lineEndingForRow(5), '')
      assert.equal(buffer.lineEndingForRow(6), undefined)
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
    if (!TextBuffer.prototype.baseTextDigest) return

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
    if (!TextBuffer.prototype.serializeChanges) return

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

  describe('.findSync', () => {
    it('returns the range of the first match with the given pattern', () => {
      const buffer = new TextBuffer('abc\ndef')
      buffer.setTextInRange(Range(Point(0, 0), Point(0, 0)), '1')
      buffer.setTextInRange(Range(Point(0, 3), Point(0, 4)), '2')
      assert.equal(buffer.getText(), '1ab2\ndef')

      assert.deepEqual(buffer.findSync('b2'), Range(Point(0, 2), Point(0, 4)))
      assert.deepEqual(buffer.findSync('bc'), null)
      assert.deepEqual(buffer.findSync('^d'), Range(Point(1, 0), Point(1, 1)))
    })

    it('throws an exception if an invalid pattern is passed', () => {
      const buffer = new TextBuffer('abc\ndef')
      assert.throws(() => buffer.findSync('['), /missing terminating ] for character class/)
    })
  })

  describe('.findAllSync', () => {
    it('returns the ranges of all matches of the given pattern', () => {
      const buffer = new TextBuffer('abcd')
      buffer.setTextInRange(Range(Point(0, 1), Point(0, 1)), '1')
      buffer.setTextInRange(Range(Point(0, 3), Point(0, 3)), '2')
      assert.equal(buffer.getText(), 'a1b2cd')

      assert.deepEqual(buffer.findAllSync(/\d[a-z]/), [
        Range(Point(0, 1), Point(0, 3)),
        Range(Point(0, 3), Point(0, 5)),
      ])
    })

    it('returns the same results as a reference implementation', () => {
      for (let i = 0; i < 3; i++) {
        const generateSeed = Random.create()
        let seed = generateSeed(MAX_INT32)
        const random = new Random(seed)
        console.log('Seed: ', seed);

        const testDocument = new TestDocument(seed)
        const buffer = new TextBuffer(testDocument.getText())

        for (let j = 0; j < 5; j++) {
          const {start, deletedExtent, insertedText} = testDocument.performRandomSplice()
          buffer.setTextInRange(
            {start, end: traverse(start, deletedExtent)},
            insertedText
          )
        }

        assert.equal(buffer.getText(), testDocument.getText())

        const regexes = [
          /^\w+/mg,
          /[ \t]+$/mg,
          /\w{3}\n\w/mg,
          /[g-z]+\n\s*[a-f]+/mg,
        ]

        for (let j = 0; j < 10; j++) {
          let regex
          if (random(2)) {
            regex = regexes[random(regexes.length)]
          } else {
            const {start, end} = testDocument.buildRandomRange()
            let substring = testDocument.getTextInRange(start, end)
            if (substring === '') substring += '.'
            regex = new RegExp(substring, 'g')
          }

          const expectedRanges = testDocument.searchAll(regex)
          const actualRanges = buffer.findAllSync(regex)
          assert.deepEqual(actualRanges, expectedRanges, `Regex: ${regex}, text: ${testDocument.getText()}`)
        }
      }
    })
  })

  describe('.find', () => {
    it('resolves with the range of the first match with the given pattern', () => {
      const buffer = new TextBuffer('abc\ndef')
      buffer.setTextInRange(Range(Point(0, 0), Point(0, 0)), '1')
      buffer.setTextInRange(Range(Point(0, 3), Point(0, 4)), '2')
      assert.equal(buffer.getText(), '1ab2\ndef')

      return Promise.all([
        buffer.find('b2').then(value => assert.deepEqual(value, Range(Point(0, 2), Point(0, 4)))),
        buffer.find('bc').then(value => assert.deepEqual(value, null)),
        buffer.find('^d').then(value => assert.deepEqual(value, Range(Point(1, 0), Point(1, 1))))
      ])
    })

    it('rejects the promise if an invalid pattern is given', () => {
      const buffer = new TextBuffer('abc\ndef')
      return buffer.find('[')
        .then(() => assert(false))
        .catch((error) => assert.match(error.message, /missing terminating ] for character class/))
    })

    it('can find for a RegExp or a string', () => {
      const buffer = new TextBuffer('abc\ndef')
      const regex = /b/
      return buffer.find(regex)
        .then((result) => assert.deepEqual(result, Range(Point(0, 1), Point(0, 2))))
        .then(() => buffer.setTextInRange(Range(Point(0, 0), Point(0, 0)), ' '))
        .then(() => buffer.find(regex))
        .then((result) => assert.deepEqual(result, Range(Point(0, 2), Point(0, 3))))
        .then(() => buffer.setTextInRange(Range(Point(0, 0), Point(0, 0)), ' '))
        .then(() => buffer.find(regex))
        .then((result) => assert.deepEqual(result, Range(Point(0, 3), Point(0, 4))))
    })
  })

  describe('.reset', () => {
    it('sets the buffer\'s text and does not consider it modified', () => {
      const buffer = new TextBuffer('abc')
      buffer.setTextInRange(Range(Point(0, 3), Point(0, 3)), 'def')
      assert.equal(buffer.getText(), 'abcdef')
      assert.ok(buffer.isModified())

      buffer.reset('g')
      assert.equal(buffer.getText(), 'g')
      assert.notOk(buffer.isModified())
    })
  })

  describe('concurrent IO', function () {
    if (!TextBuffer.prototype.load) return;

    this.timeout(60 * 1000);

    it('handles multiple calls to .load at a time', () => {
      const buffer = new TextBuffer('abc')

      const emptyFilePath = temp.openSync('atom').path
      const smallFilePath = temp.openSync('atom').path
      const largeFilePath = temp.openSync('atom').path
      const smallContent = '123456789\n'.repeat(1024)
      const largeContent = smallContent.repeat(10)
      fs.writeFileSync(smallFilePath, smallContent)
      fs.writeFileSync(largeFilePath, largeContent)

      const load1 = buffer.load(emptyFilePath).then(() => {
        assert.equal(buffer.getLength(), 0)
      })

      const load2 = buffer.load(smallFilePath).then(() => {
        assert.equal(buffer.getLength(), smallContent.length)
      })

      const load3 = buffer.load(largeFilePath).then(() => {
        assert.equal(buffer.getLength(), largeContent.length)
      })

      return Promise.all([load1, load2, load3])
    })

    it('handles random concurrent IO calls', () => {
      const generateSeed = Random.create()
      let seed = generateSeed(MAX_INT32)
      const random = new Random(seed)
      const testDocument = new TestDocument(seed)
      console.log('Seed: ', seed);

      const promises = []
      const buffer = new TextBuffer(testDocument.getText())
      let currentText = buffer.getText()

      for (let i = 0; i < 20; i++) {
        switch (random(4)) {
          case 0: {
            testDocument.performRandomSplice()
            const text = testDocument.getText()
            const filePath = temp.openSync().path
            const previousText = buffer.getText()
            const wasModified = buffer.isModified()
            fs.writeFileSync(filePath, text, 'utf8')
            promises.push(buffer.load(filePath).then(() => {
              if (!wasModified && !buffer.isModified()) {
                assert.equal(buffer.getText(), text)
                assert.notOk(buffer.isModified())
                currentText = text
              } else {
                assert.equal(buffer.getText(), currentText)
              }
            }))
            break;
          }

          case 1: {
            const text = testDocument.getText()
            const filePath = temp.openSync().path
            fs.writeFileSync(filePath, text, 'utf8')
            promises.push(buffer.reload(filePath).then((patch) => {
              assert.equal(buffer.getText(), text)
              assert.equal(applyPatch(currentText, patch), text)
              currentText = text
              assert.notOk(buffer.isModified())
            }))
            break;
          }

          case 2: {
            const {start, deletedExtent, insertedText} = testDocument.performRandomSplice()
            buffer.setTextInRange(Range(start, traverse(start, deletedExtent)), insertedText)
            const text = buffer.getText()
            const filePath = temp.openSync().path
            currentText = text
            promises.push(buffer.save(filePath).then(() => {
              assert.equal(fs.readFileSync(filePath, 'utf8'), text)
            }))
            break;
          }

          case 3: {
            const subtext = buffer.getTextInRange(testDocument.buildRandomRange())
            const regex = new RegExp(subtext)
            const expectedRange = referenceSearch(buffer.getText(), regex)
            promises.push(buffer.find(regex).then((result) => {
              assert.deepEqual(result, expectedRange)
            }))
            break;
          }
        }
      }

      return Promise.all(promises)
    })
  })
})

function referenceSearch(text, regex) {
  const match = regex.exec(text)
  if (match) {
    const start = getExtent(text.slice(0, match.index))
    const end = traverse(start, getExtent(match[0]))
    return {start, end}
  }
  return null
}

function applyPatch(text, patch) {
  const buffer = new TextBuffer(text)
  const changes = patch.getChanges()
  for (let i = changes.length - 1; i >= 0; i--) {
    const change = changes[i]
    buffer.setTextInRange(
      Range(change.oldStart, change.oldEnd),
      change.newText
    )
  }
  return buffer.getText()
}

function toPlainObject(value) {
  return JSON.parse(JSON.stringify(value))
}

function Range(start, end) {
  return {start, end}
}

function Point(row, column) {
  return {row, column}
}