const fs = require('fs')
const path = require('path')
const temp = require('temp').track()
const {Writable} = require('stream')
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

      return buffer.load(filePath, {encoding: 'UTF-16LE'}).then(() =>
        assert.equal(buffer.getText(), content)
      )
    })

    it('rejects its promise if an invalid encoding is given', () => {
      const buffer = new TextBuffer()

      const {path: filePath} = temp.openSync()
      const content = 'a\nb\nc\n'.repeat(10)
      fs.writeFileSync(filePath, content, 'utf16le')

      let rejection = null
      return buffer.load(filePath, {encoding: 'GARBAGE16'})
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
        new TextBuffer().load(filePath, {encoding})
      ))
    })

    it('handles paths containing non-ascii characters', () => {
      const directory = temp.mkdirSync()
      const filePath = path.join(directory, 'русский.txt')
      fs.writeFileSync(filePath, 'Hello')
      const buffer = new TextBuffer()
      return buffer.load(filePath).then(() => {
        assert.equal(buffer.getText(), 'Hello')

        buffer.setTextInRange(Range(Point(0, 5), Point(0, 5)), '!')
        return buffer.save(filePath).then(() => {
          assert.equal(fs.readFileSync(filePath), 'Hello!')
        })
      })
    })

    describe('when the `force` option is set to true', () => {
      it('discards any modifications and incorporates that change into the resolved patch', () => {
        const buffer = new TextBuffer('abcdef')
        const {path: filePath} = temp.openSync()
        fs.writeFileSync(filePath, '  abcdef')

        buffer.setTextInRange(Range(Point(0, 3), Point(0, 3)), ' ')
        assert.equal(buffer.getText(), 'abc def')
        assert.ok(buffer.isModified())

        const loadPromise = buffer.load(filePath, {force: true}).then(patch => {
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

        return buffer.load(filePath, {force: true}).then(patch => {
          assert.equal(patch.getChanges(), 0)
          assert.equal(buffer.getText(), '  abcdef')
          assert.notOk(buffer.isModified())
        })
      })
    })

    describe('when the `patch` option is set to false', () => {
      it('does not compute a Patch representing the changes', () => {
        const buffer = new TextBuffer()

        const filePath = temp.openSync().path
        fs.writeFileSync(filePath, 'abc')

        return buffer.load(filePath, {patch: false}).then((patch) => {
          assert.equal(patch, null)
          assert.equal(buffer.getText(), 'abc')

          buffer.setTextInRange(Range(Point(0, 0), Point(0, 0)), '  ')
          return buffer.load(filePath, {patch: false, force: true}).then((patch) => {
            assert.equal(patch, null)
            assert.equal(buffer.getText(), 'abc')
          })
        })
      })
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
            if (!isWindows) {
              assert.include(error.message, ' read ')
              assert.equal(error.code, 'EISDIR')
            }
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
            if (!isWindows) {
              assert.include(error.message, ' open ')
              assert.equal(error.code, 'ELOOP')
            }
            assert.equal(error.path, filePath)
            done()
          })
      })
    })
  })

  describe('.baseTextMatchesFile', () => {
    if (!TextBuffer.prototype.baseTextMatchesFile) return;

    it('indicates whether the base text matches the contents of the given file path', () => {
      const content = 'abc'

      const buffer = new TextBuffer(content)

      const {path: filePath} = temp.openSync()
      fs.writeFileSync(filePath, content)

      return buffer.baseTextMatchesFile(filePath).then((result) => {
        assert.ok(result)
      }).then(() => {
        fs.writeFileSync(filePath, content + '!')
        return buffer.baseTextMatchesFile(filePath)
      }).then((result) => {
        assert.notOk(result)
      })
    })

    it('indicates whether the base text matches the contents of the given stream', () => {
      const content = 'abc'

      const buffer = new TextBuffer(content)

      const {path: filePath} = temp.openSync()
      fs.writeFileSync(filePath, content)

      return buffer.baseTextMatchesFile(fs.createReadStream(filePath)).then((result) => {
        assert.ok(result)
      }).then(() => {
        fs.writeFileSync(filePath, content + '!')
        return buffer.baseTextMatchesFile(fs.createReadStream(filePath))
      }).then((result) => {
        assert.notOk(result)
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

      return buffer.save(stream).then(() => {
        assert.notOk(buffer.isModified())
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

    it('can save to a stream when the buffer has been cleared (regression)', () => {
      const buffer = new TextBuffer('abc')
      buffer.setText('')

      const {path: filePath} = temp.openSync()
      const stream = fs.createWriteStream(filePath)

      return buffer.save(stream).then(() => {
        assert.equal(fs.readFileSync(filePath, 'utf8'), buffer.getText())
      })
    })

    it('can save to a stream when the buffer has deletions (regression)', () => {
      const buffer = new TextBuffer('abc')
      buffer.setTextInRange(Range(Point(0, 1), Point(0, 2)), '')
      assert.equal(buffer.getText(), 'ac')

      const {path: filePath} = temp.openSync()
      const stream = fs.createWriteStream(filePath)

      return buffer.save(stream).then(() => {
        assert.equal(fs.readFileSync(filePath, 'utf8'), buffer.getText())
      })
    })

    describe('error handling', () => {
      it('rejects with an error if the path points to a directory', (done) => {
        const buffer = new TextBuffer()
        const filePath = temp.mkdirSync()

        buffer.setText('hello')
        assert.ok(buffer.isModified())

        return buffer.save(filePath)
          .then(() => {
            done(new Error('Expected an error'))
          })
          .catch((error) => {
            assert.include(error.message, ' open ')
            assert.equal(error.code, isWindows ? 'EACCES' : 'EISDIR')
            assert.equal(error.path, filePath)
            assert.ok(buffer.isModified())
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

        buffer.setText('hello')
        assert.ok(buffer.isModified())

        return buffer.save(filePath)
          .then(() => {
            done(new Error('Expected an error'))
          })
          .catch((error) => {
            assert.include(error.message, ' open ')
            assert.equal(error.code, isWindows ? 'EINVAL' : 'ELOOP')
            assert.equal(error.path, filePath)
            assert.ok(buffer.isModified())
            done()
          })
      })

      it('rejects with an error if writing to a stream fails', (done) => {
        const tempDir = temp.mkdirSync()
        const filePath = path.join(tempDir, 'one')
        const otherPath = path.join(tempDir, 'two')
        fs.symlinkSync(filePath, otherPath)
        fs.symlinkSync(otherPath, filePath)

        const buffer = new TextBuffer('abcd')
        buffer.setText('efg')
        assert.ok(buffer.isModified())

        const stream = new Writable({
          write(chunk, encoding, callback) {
            process.nextTick(() => callback(new Error('Could not write to stream')))
          }
        })

        buffer.save(stream)
          .then(() => {
            done(new Error('Expected a rejection'))
          })
          .catch((error) => {
            assert.equal(error.message, 'Could not write to stream')
            assert.ok(buffer.isModified())
            done()
          })
      })

      it('rejects with an error if closing the stream fails', (done) => {
        const tempDir = temp.mkdirSync()
        const filePath = path.join(tempDir, 'one')
        const otherPath = path.join(tempDir, 'two')
        fs.symlinkSync(filePath, otherPath)
        fs.symlinkSync(otherPath, filePath)

        const buffer = new TextBuffer('abcd')
        buffer.setText('efg')
        assert.ok(buffer.isModified())

        const stream = new Writable({
          write(chunk, encoding, callback) {
            callback()
          }
        })

        stream.end = function () {
          process.nextTick(() => {
            this.emit('error', new Error('Could not close stream'))
            this.emit('finish')
          })
        }

        buffer.save(stream)
          .then(() => {
            done(new Error('Expected a rejection'))
          })
          .catch((error) => {
            assert.equal(error.message, 'Could not close stream')
            assert.ok(buffer.isModified())
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

  describe('.getLength, .getExtent, and .getLineCount', () => {
    it('returns the total length and total extent of the text', () => {
      const buffer = new TextBuffer()
      assert.equal(buffer.getLength(), 0)
      assert.deepEqual(buffer.getExtent(), Point(0, 0))
      assert.equal(buffer.getLineCount(), 1)

      buffer.setText('abc\r\ndefg\n\r\nhijkl')
      assert.equal(buffer.getLength(), buffer.getText().length)
      assert.deepEqual(buffer.getExtent(), Point(3, 5))
      assert.equal(buffer.getLineCount(), 4)
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

  describe('.find (sync and async)', () => {
    it('returns the range of the first match with the given pattern', async () => {
      const buffer = new TextBuffer('abc\ndef')
      buffer.setTextInRange(Range(Point(0, 0), Point(0, 0)), '1')
      buffer.setTextInRange(Range(Point(0, 3), Point(0, 4)), '2')
      assert.equal(buffer.getText(), '1ab2\ndef')

      assert.deepEqual(buffer.findSync('b2'), Range(Point(0, 2), Point(0, 4)))
      assert.deepEqual(buffer.findSync('bc'), null)
      assert.deepEqual(buffer.findSync('^d'), Range(Point(1, 0), Point(1, 1)))

      assert.deepEqual(await buffer.find('b2'), Range(Point(0, 2), Point(0, 4)))
      assert.deepEqual(await buffer.find('bc'), null)
      assert.deepEqual(await buffer.find('^d'), Range(Point(1, 0), Point(1, 1)))
    })

    it('throws an exception if an invalid pattern is passed', async () => {
      const buffer = new TextBuffer('abc\ndef')

      try {
        buffer.findSync('[')
        assert(false, 'Expected an exception')
      } catch (error) {
        assert.match(error.message, /missing terminating ] for character class/)
      }

      try {
        await buffer.find('[')
        assert(false, 'Expected an exception')
      } catch (error) {
        assert.match(error.message, /missing terminating ] for character class/)
      }
    })

    it('can be called repeatedly with the same RegExp to avoid recompiling the RegExp', async () => {
      const buffer = new TextBuffer('abc\ndef')

      const regex = /b/
      assert.deepEqual(buffer.findSync(regex), Range(Point(0, 1), Point(0, 2)))
      assert.deepEqual(await buffer.find(regex), Range(Point(0, 1), Point(0, 2)))

      buffer.setTextInRange(Range(Point(0, 0), Point(0, 0)), ' ')
      assert.deepEqual(buffer.findSync(regex), Range(Point(0, 2), Point(0, 3)))
      assert.deepEqual(await buffer.find(regex), Range(Point(0, 2), Point(0, 3)))

      buffer.setTextInRange(Range(Point(0, 0), Point(0, 0)), ' ')
      assert.deepEqual(buffer.findSync(regex), Range(Point(0, 3), Point(0, 4)))
      assert.deepEqual(await buffer.find(regex), Range(Point(0, 3), Point(0, 4)))
    })

    it('supports regexes with unicode escape sequences', () => {
      const buffer = new TextBuffer('åå é')
      assert.equal('\u00e5', 'å')
      assert.deepEqual(buffer.findSync('\u00e5+'), Range(Point(0, 0), Point(0, 2)))
      assert.deepEqual(buffer.findSync(/\u00e5+/), Range(Point(0, 0), Point(0, 2)))

      // Like with JS regexes, invalid unicode escape sequences just match their literal content.
      assert.equal(buffer.findSync(/\uxxxx/), null)
      buffer.setText('a \\uxxxx b')
      assert.deepEqual(buffer.findSync(/\uxxxx/), Range(Point(0, 2), Point(0, 8)))

      assert.deepEqual(buffer.findSync(/\uxxx/), Range(Point(0, 2), Point(0, 7)))
      assert.deepEqual(buffer.findSync(/\uxx/), Range(Point(0, 2), Point(0, 6)))
    })
  })

  describe('.findInRange (sync and async)', () => {
    it('returns the range of the first match in the given range', async () => {
      const buffer = new TextBuffer('abc def\nghi jkl\n')
      buffer.setTextInRange(Range(Point(0, 1), Point(0, 2)), 'B')
      buffer.setTextInRange(Range(Point(1, 2), Point(1, 3)), 'F')

      assert.deepEqual(
        buffer.findInRangeSync(/\w+/, Range(Point(0, 1), Point(1, 2))),
        Range(Point(0, 1), Point(0, 3)))
      assert.deepEqual(
        await buffer.findInRange(/\w+/, Range(Point(0, 1), Point(1, 2))),
        Range(Point(0, 1), Point(0, 3)))
      assert.deepEqual(
        buffer.findInRangeSync(/j\w*/, Range(Point(0, 0), Point(1, 6))),
        Range(Point(1, 4), Point(1, 6)))
      assert.deepEqual(
        await buffer.findInRange(/j\w*/, Range(Point(0, 0), Point(1, 6))),
        Range(Point(1, 4), Point(1, 6)))
    })
  })

  describe('.findAll (sync and async)', () => {
    it('returns the ranges of all matches of the given pattern', async () => {
      const buffer = new TextBuffer('abcd')
      buffer.setTextInRange(Range(Point(0, 1), Point(0, 1)), '1')
      buffer.setTextInRange(Range(Point(0, 3), Point(0, 3)), '2')
      assert.equal(buffer.getText(), 'a1b2cd')

      assert.deepEqual(buffer.findAllSync(/\d[a-z]/), [
        Range(Point(0, 1), Point(0, 3)),
        Range(Point(0, 3), Point(0, 5)),
      ])
      assert.deepEqual(await buffer.findAll(/\d[a-z]/), [
        Range(Point(0, 1), Point(0, 3)),
        Range(Point(0, 3), Point(0, 5)),
      ])
    })

    it('handles empty matches before CRLF line endings (regression)', () => {
      const buffer = new TextBuffer('abc def\r\n\r\nghi jkl\r\n\r\n')

      assert.deepEqual(buffer.findAllSync(/^[ \t\r]*$/), [
        Range(Point(1, 0), Point(1, 0)),
        Range(Point(3, 0), Point(3, 0))
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

        for (let j = 0; j < 10; j++) {
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

          if (random(2)) {
            const expectedRanges = testDocument.searchAll(regex)
            const actualRanges = buffer.findAllSync(regex)
            assert.deepEqual(actualRanges, expectedRanges, `Regex: ${regex}, text: ${testDocument.getText()}`)
          } else {
            const rowCount = testDocument.getExtent().row
            const [startRow, endRow] = [random(rowCount), random(rowCount)].sort((a, b) => a - b)
            const searchRange = {start: {row: startRow, column: 0}, end: {row: endRow, column: Infinity}}
            const expectedRanges = testDocument.searchAllInRange(searchRange, regex)
            const actualRanges = buffer.findAllInRangeSync(regex, searchRange)
            assert.deepEqual(actualRanges, expectedRanges, `Regex: ${regex}, range: ${JSON.stringify(searchRange)}, text: ${testDocument.getText()}`)
          }
        }
      }
    })
  })

  describe('.findAllInRange (sync and async)', () => {
    it('returns the ranges of all matches of the given pattern within the given range', async () => {
      const buffer = new TextBuffer('abc def\nghi jkl\n')

      assert.deepEqual(buffer.findAllInRangeSync(/\w+/, Range(Point(0, 1), Point(1, 2))), [
        Range(Point(0, 1), Point(0, 3)),
        Range(Point(0, 4), Point(0, 7)),
        Range(Point(1, 0), Point(1, 2))
      ])
      assert.deepEqual(await buffer.findAllInRange(/\w+/, Range(Point(0, 1), Point(1, 2))), [
        Range(Point(0, 1), Point(0, 3)),
        Range(Point(0, 4), Point(0, 7)),
        Range(Point(1, 0), Point(1, 2))
      ])
    })

    it('handles the ^ and $ anchors properly', () => {
      const buffer = new TextBuffer('abc\ndefg\nhijkl')

      assert.deepEqual(buffer.findAllInRangeSync(/^\w/, Range(Point(0, 1), Point(1, 2))), [
        Range(Point(1, 0), Point(1, 1))
      ])
      assert.deepEqual(buffer.findAllInRangeSync(/\w$/, Range(Point(0, 1), Point(1, 2))), [
        Range(Point(0, 2), Point(0, 3))
      ])
      assert.deepEqual(buffer.findAllInRangeSync(/\w$/, Range(Point(2, 1), Point(2, 5))), [
        Range(Point(2, 4), Point(2, 5))
      ])
      assert.deepEqual(buffer.findAllInRangeSync(/\w$/, Range(Point(2, 1), Point(2, Infinity))), [
        Range(Point(2, 4), Point(2, 5))
      ])
      assert.deepEqual(buffer.findAllInRangeSync(/\w*$/, Range(Point(1, 0), Point(1, Infinity))), [
        Range(Point(1, 0), Point(1, 4))
      ])
    })

    it('handles the ^ and $ anchors properly (CRLF line endings)', () => {
      const buffer = new TextBuffer('abc\r\ndefg\r\nhijkl')

      assert.deepEqual(buffer.findAllInRangeSync(/^\w/, Range(Point(0, 1), Point(1, 2))), [
        Range(Point(1, 0), Point(1, 1))
      ])
      assert.deepEqual(buffer.findAllInRangeSync(/\w\r?$/, Range(Point(0, 1), Point(1, 2))), [
        Range(Point(0, 2), Point(0, 3))
      ])
      assert.deepEqual(buffer.findAllInRangeSync(/\w\r?$/, Range(Point(2, 1), Point(2, 5))), [
        Range(Point(2, 4), Point(2, 5))
      ])
      assert.deepEqual(buffer.findAllInRangeSync(/\w\r?$/, Range(Point(2, 1), Point(2, Infinity))), [
        Range(Point(2, 4), Point(2, 5))
      ])
    })
  })

  describe('.findWordsWithSubsequence and .findWordsWithSubsequenceInRange', () => {
    it('doesn\'t crash intermittently', () => {
      let buffer;
      let promises = []
      for (let k = 0; k < 100; k++) {
        buffer = new TextBuffer('abc')
        promises.push(
          buffer.findWordsWithSubsequence('a', '', 10)
        )
      }
      return Promise.all(promises)
    })

    it('resolves with all words matching the given query', () => {
      const buffer = new TextBuffer('banana bandana ban_ana bandaid band bNa\nbanana')
      return buffer.findWordsWithSubsequence('bna', '_', 4).then((result) => {
        assert.deepEqual(result, [
          {
            score: 29,
            matchIndices: [0, 1, 2 ],
            positions: [{row: 0, column: 36}],
            word: "bNa"
          },
          {
            score: 16,
            matchIndices: [0, 2, 4],
            positions: [{row: 0, column: 15}],
            word: "ban_ana"
          },
          {
            score: 12,
            matchIndices: [0, 2, 3],
            positions: [{row: 0, column: 0}, {row: 1, column: 0}],
            word: "banana"
          },
          {
            score: 7,
            matchIndices: [0, 5, 6],
            positions: [{row: 0, column: 7}],
            word: "bandana"
          }
        ])
      })
    })

    it('resolves with all words matching the given query and range', () => {
      const buffer = new TextBuffer('banana bandana ban_ana bandaid band bNa\nbanana')
      const range = {start: {column: 0, row: 0}, end: {column: 22, row: 0}}
      return buffer.findWordsWithSubsequenceInRange('bna', '_', 4, range).then((result) => {
        assert.deepEqual(result, [
          {
            score: 16,
            matchIndices: [0, 2, 4],
            positions: [{row: 0, column: 15}],
            word: "ban_ana"
          },
          {
            score: 12,
            matchIndices: [0, 2, 3],
            positions: [{row: 0, column: 0}],
            word: "banana"
          },
          {
            score: 7,
            matchIndices: [0, 5, 6],
            positions: [{row: 0, column: 7}],
            word: "bandana"
          }
        ])
      })
    })

    it('does not compute matches for words longer than 80 characters', () => {
      const buffer = new TextBuffer('eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbIi4uLy4uL2xpYi9jb252ZXJ0LmpzIl0sIm5hbWVzIjpbImxzi')
      const buffer2 = new TextBuffer('eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbIi4uLy4uL2xpYi9jb252ZXJ0LmpzIl0sIm5hbWVzIjpbImxz')
      return buffer.findWordsWithSubsequence('eyJ', '', 1).then(results => {
        assert.equal(results.length, 0)
        return buffer2.findWordsWithSubsequence('eyJ', '', 1).then(results => {
          assert.equal(results.length, 1)
        })
      })
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
            promises.push(buffer.load(filePath, {force: true}).then((patch) => {
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
