const Random = require('random-seed')
const {assert} = require('chai')
const TestDocument = require('./helpers/test-document')
const textHelpers = require('./helpers/text-helpers')
const {
  traverse, traversalDistance, compare, format: formatPoint
} = require('./helpers/point-helpers')

const {Patch} = require('../..')

describe('Patch', function () {
  it('honors the mergeAdjacentChanges option set to false', function () {
    const patch = new Patch({mergeAdjacentChanges: false})

    patch.splice({row: 0, column: 10}, {row: 0, column: 0}, {row: 1, column: 5})
    patch.splice({row: 1, column: 5}, {row: 0, column: 2}, {row: 0, column: 8})

    assert.deepEqual(JSON.parse(JSON.stringify(patch.getChanges())), [
      {
        oldStart: {row: 0, column: 10},
        oldEnd: {row: 0, column: 10},
        newStart: {row: 0, column: 10},
        newEnd: {row: 1, column: 5}
      },
      {
        oldStart: {row: 0, column: 10},
        oldEnd: {row: 0, column: 12},
        newStart: {row: 1, column: 5},
        newEnd: {row: 1, column: 13}
      }
    ])

    patch.delete();
  })

  it('honors the mergeAdjacentChanges option set to true', function () {
    const patch = new Patch({mergeAdjacentChanges: true})

    patch.splice({row: 0, column: 5}, {row: 0, column: 1}, {row: 0, column: 2})
    patch.splice({row: 0, column: 10}, {row: 0, column: 3}, {row: 0, column: 4})
    assert.deepEqual(JSON.parse(JSON.stringify(patch.getChanges())), [
      {
        oldStart: {row: 0, column: 5}, oldEnd: {row: 0, column: 6},
        newStart: {row: 0, column: 5}, newEnd: {row: 0, column: 7}
      },
      {
        oldStart: {row: 0, column: 9}, oldEnd: {row: 0, column: 12},
        newStart: {row: 0, column: 10}, newEnd: {row: 0, column: 14}
      }
    ])

    patch.spliceOld({row: 0, column: 6}, {row: 0, column: 3}, {row: 0, column: 0})
    assert.deepEqual(JSON.parse(JSON.stringify(patch.getChanges())), [
      {
        oldStart: {row: 0, column: 5}, oldEnd: {row: 0, column: 9},
        newStart: {row: 0, column: 5}, newEnd: {row: 0, column: 11}
      }
    ])

    patch.delete();
  })

  describe('.compose', () => {
    it('combines the given patches into one', () => {
      const patches = [new Patch(), new Patch(), new Patch()]
      patches[0].splice({row: 0, column: 3}, {row: 0, column: 4}, {row: 0, column: 5}, 'ciao', 'hello')
      patches[0].splice({row: 1, column: 1}, {row: 0, column: 0}, {row: 0, column: 3}, '', 'hey')
      patches[1].splice({row: 0, column: 15}, {row: 0, column: 10}, {row: 0, column: 0}, '0123456789', '')
      patches[1].splice({row: 0, column: 0}, {row: 0, column: 0}, {row: 3, column: 0}, '', '\n\n\n')
      patches[2].splice({row: 4, column: 2}, {row: 0, column: 2}, {row: 0, column: 2}, 'so', 'ho')

      const composedPatch = Patch.compose(patches)
      assert.deepEqual(JSON.parse(JSON.stringify(composedPatch.getChanges())), [
        {
          oldStart: {row: 0, column: 0}, oldEnd: {row: 0, column: 0},
          newStart: {row: 0, column: 0}, newEnd: {row: 3, column: 0},
          oldText: '', newText: '\n\n\n'
        },
        {
          oldStart: {row: 0, column: 3}, oldEnd: {row: 0, column: 7},
          newStart: {row: 3, column: 3}, newEnd: {row: 3, column: 8},
          oldText: 'ciao', newText: 'hello'
        },
        {
          oldStart: {row: 0, column: 14}, oldEnd: {row: 0, column: 24},
          newStart: {row: 3, column: 15}, newEnd: {row: 3, column: 15},
          oldText: '0123456789', newText: ''
        },
        {
          oldStart: {row: 1, column: 1}, oldEnd: {row: 1, column: 1},
          newStart: {row: 4, column: 1}, newEnd: {row: 4, column: 4},
          oldText: '', newText: 'hho'
        }
      ])

      assert.throws(() => Patch.compose([{}, {}]))
      assert.throws(() => Patch.compose([1, 'a']))

      for (let patch of patches)
        patch.delete();

      composedPatch.delete();
    })

    it('throws an Error if the patches do not apply', () => {
      {
        const patches = [new Patch(), new Patch()]
        patches[0].splice({row: 0, column: 3}, {row: 0, column: 3}, {row: 1, column: 2}, 'abc', 'de\nfg')
        patches[1].splice({row: 0, column: 4}, {row: 0, column: 5}, {row: 0, column: 1}, 'hijkl', 'm')
        assert.throws(() => Patch.compose(patches), 'Patch does not apply')
      }

      {
        const patches = [new Patch(), new Patch()]
        patches[0].splice({row: 0, column: 3}, {row: 0, column: 3}, {row: 0, column: 5}, 'abc', 'defgh')
        patches[1].splice({row: 0, column: 2}, {row: 1, column: 1}, {row: 0, column: 1}, 'h\ni', 'j')
        assert.throws(() => Patch.compose(patches), 'Patch does not apply')
      }
    })

    it('removes noop changes when the given patches cancel each other out', () => {
      const patches = [new Patch(), new Patch()]
      patches[0].splice({row: 0, column: 3}, {row: 0, column: 4}, {row: 0, column: 5}, 'ciao', 'hello')
      patches[0].splice({row: 1, column: 1}, {row: 0, column: 0}, {row: 0, column: 3}, '', 'hey')
      patches[1].splice({row: 0, column: 3}, {row: 0, column: 5}, {row: 0, column: 4}, 'hello', 'ciao')

      const composedPatch = Patch.compose(patches)
      assert.deepEqual(JSON.parse(JSON.stringify(composedPatch.getChanges())), [
        {
          oldStart: {row: 1, column: 1}, oldEnd: {row: 1, column: 1},
          newStart: {row: 1, column: 1}, newEnd: {row: 1, column: 4},
          oldText: '', newText: 'hey'
        },
      ])
    })
  })

  it('can invert patches', function () {
    const patch = new Patch()
    patch.splice({row: 0, column: 3}, {row: 0, column: 4}, {row: 0, column: 5}, 'ciao', 'hello')
    patch.splice({row: 0, column: 10}, {row: 0, column: 5}, {row: 0, column: 5}, 'quick', 'world')

    const invertedPatch = patch.invert()
    assert.deepEqual(JSON.parse(JSON.stringify(invertedPatch.getChanges())), [
      {
        oldStart: {row: 0, column: 3}, oldEnd: {row: 0, column: 8},
        newStart: {row: 0, column: 3}, newEnd: {row: 0, column: 7},
        oldText: 'hello',
        newText: 'ciao'
      },
      {
        oldStart: {row: 0, column: 10}, oldEnd: {row: 0, column: 15},
        newStart: {row: 0, column: 9}, newEnd: {row: 0, column: 14},
        oldText: 'world',
        newText: 'quick'
      }
    ])

    const patch2 = new Patch()
    patch2.splice({row: 0, column: 3}, {row: 0, column: 4}, {row: 0, column: 5})
    patch2.splice({row: 0, column: 10}, {row: 0, column: 5}, {row: 0, column: 5})
    assert.deepEqual(JSON.parse(JSON.stringify(patch2.invert().getChanges())), [
      {
        oldStart: {row: 0, column: 3}, oldEnd: {row: 0, column: 8},
        newStart: {row: 0, column: 3}, newEnd: {row: 0, column: 7}
      },
      {
        oldStart: {row: 0, column: 10}, oldEnd: {row: 0, column: 15},
        newStart: {row: 0, column: 9}, newEnd: {row: 0, column: 14}
      }
    ])

    patch.delete();
    invertedPatch.delete();
    patch2.delete();
  })

  it('can copy patches', function () {
    const patch = new Patch()
    patch.splice({row: 0, column: 3}, {row: 0, column: 4}, {row: 0, column: 5}, 'ciao', 'hello')
    patch.splice({row: 0, column: 10}, {row: 0, column: 5}, {row: 0, column: 5}, 'quick', 'world')
    assert.deepEqual(patch.copy().getChanges(), patch.getChanges())

    const patch2 = new Patch()
    patch2.splice({row: 0, column: 3}, {row: 0, column: 4}, {row: 0, column: 5})
    patch2.splice({row: 0, column: 10}, {row: 0, column: 5}, {row: 0, column: 5})

    assert.deepEqual(patch2.copy().getChanges(), patch2.getChanges())
    patch.delete();
    patch2.delete();
  })

  it('can serialize/deserialize patches', () => {
    const emptyPatch = Patch.deserialize(new Patch().serialize())
    assert.equal(emptyPatch.getChangeCount(), 0)
    assert.deepEqual(emptyPatch.getChanges(), [])

    const patch1 = new Patch()
    patch1.splice({row: 0, column: 3}, {row: 0, column: 5}, {row: 0, column: 5}, 'hello', 'world')

    const patch2 = Patch.deserialize(patch1.serialize())
    assert.deepEqual(JSON.parse(JSON.stringify(patch2.getChanges())), [{
      oldStart: {row: 0, column: 3},
      newStart: {row: 0, column: 3},
      oldEnd: {row: 0, column: 8},
      newEnd: {row: 0, column: 8},
      oldText: 'hello',
      newText: 'world'
    }])

    patch1.delete();
    patch2.delete();
  })

  it('removes a change when it becomes empty', () => {
    const patch = new Patch()
    patch.splice({row: 1, column: 0}, {row: 0, column: 0}, {row: 0, column: 5})
    patch.splice({row: 2, column: 0}, {row: 0, column: 0}, {row: 0, column: 5})
    patch.splice({row: 1, column: 0}, {row: 0, column: 5}, {row: 0, column: 0})

    assert.deepEqual(JSON.parse(JSON.stringify(patch.getChanges())), [{
      oldStart: {row: 2, column: 0},
      newStart: {row: 2, column: 0},
      oldEnd: {row: 2, column: 0},
      newEnd: {row: 2, column: 5}
    }])
  })

  it('correctly records random splices', function () {
    this.timeout(Infinity)

    for (let i = 0; i < 100; i++) {
      let seed = Date.now()
      const seedMessage = `Random seed: ${seed}`

      const random = new Random(seed)
      const originalDocument = new TestDocument(seed)
      const mutatedDocument = originalDocument.clone()
      const mergeAdjacentChanges = random(2)
      const patch = new Patch({mergeAdjacentChanges})

      for (let j = 0; j < 20; j++) {
        if (random(10) < 1) {
          patch.rebalance()
        } else if (random(10) < 4) {
          const originalSplice = originalDocument.performRandomSplice(false)
          const mutatedSplice = translateSpliceFromOriginalDocument(
            originalDocument,
            patch,
            originalSplice
          )

          mutatedDocument.splice(
            mutatedSplice.start,
            mutatedSplice.deletionExtent,
            mutatedSplice.insertedText
          )

          // process.stderr.write(`graph message {
          //   label="spliceOld(${formatPoint(originalSplice.start)}, ${formatPoint(originalSplice.deletedExtent)}, ${formatPoint(originalSplice.insertedExtent)}, ${originalSplice.deletedText}, ${originalSplice.insertedText})"
          // }\n`)
          //
          // process.stderr.write(`graph message {
          //   label="document: ${mutatedDocument.getText()}"
          // }\n`)

          patch.spliceOld(
            originalSplice.start,
            originalSplice.deletedExtent,
            originalSplice.insertedExtent
          )
        } else {
          const splice = mutatedDocument.performRandomSplice(true)

          // process.stderr.write(`graph message {
          //   label="splice(${formatPoint(splice.start)}, ${formatPoint(splice.deletedExtent)}, ${formatPoint(splice.insertedExtent)}, '${splice.deletedText}', '${splice.insertedText}')"
          // }\n`)
          //
          // process.stderr.write(`graph message {
          //   label="document: ${mutatedDocument.getText()}"
          // }\n`)

          patch.splice(
            splice.start,
            splice.deletedExtent,
            splice.insertedExtent,
            splice.deletedText,
            splice.insertedText
          )
        }

        // process.stderr.write(patch.getDotGraph())

        const originalDocumentCopy = originalDocument.clone()
        const changes = patch.getChanges()
        assert.equal(patch.getChangeCount(), changes.length, seedMessage)

        let previousChange
        for (let change of patch.getChanges()) {
          const oldExtent = traversalDistance(change.oldEnd, change.oldStart)
          assert.equal(change.newText, mutatedDocument.getTextInRange(change.newStart, change.newEnd), seedMessage)
          assert.equal(change.oldText, originalDocument.getTextInRange(change.oldStart, change.oldEnd), seedMessage)
          originalDocumentCopy.splice(change.newStart, oldExtent, change.newText)
          if (previousChange && mergeAdjacentChanges) {
            assert.notDeepEqual(previousChange.oldEnd, change.oldStart, seedMessage)
            assert.notDeepEqual(previousChange.newEnd, change.newStart, seedMessage)
          }
          previousChange = change
        }

        assert.deepEqual(originalDocumentCopy.getLines(), mutatedDocument.getLines(), seedMessage)

        if (changes.length > 0) {
          assert.deepEqual(JSON.parse(JSON.stringify(patch.getBounds())), JSON.parse(JSON.stringify(({
            oldStart: changes[0].oldStart,
            newStart: changes[0].newStart,
            oldEnd: last(changes).oldEnd,
            newEnd: last(changes).newEnd
          }))))
        } else {
          assert.equal(patch.getBounds(), undefined)
        }

        for (let k = 0; k < 5; k++) {
          let oldRange = originalDocument.buildRandomRange()
          assert.deepEqual(
            patch.getChangesInOldRange(oldRange.start, oldRange.end),
            changes.filter(change =>
              compare(change.oldEnd, oldRange.start) > 0 &&
              compare(change.oldStart, oldRange.end) < 0
            ),
            `old range: ${formatPoint(oldRange.start)} - ${formatPoint(oldRange.end)}, seed: ${seed}`
          )

          let newRange = mutatedDocument.buildRandomRange()
          assert.deepEqual(
            patch.getChangesInNewRange(newRange.start, newRange.end),
            changes.filter(change =>
              compare(change.newEnd, newRange.start) > 0 &&
              compare(change.newStart, newRange.end) < 0
            ),
            `new range: ${formatPoint(newRange.start)} - ${formatPoint(newRange.end)}, seed: ${seed}`
          )

          let oldPoint = originalDocument.buildRandomPoint()
          assert.deepEqual(
            patch.changeForOldPosition(oldPoint),
            last(changes.filter(change => compare(change.oldStart, oldPoint) <= 0)),
            seedMessage
          )

          let newPoint = mutatedDocument.buildRandomPoint()
          assert.deepEqual(
            patch.changeForNewPosition(newPoint),
            last(changes.filter(change => compare(change.newStart, newPoint) <= 0)),
            seedMessage
          )
        }

        let oldPoint = originalDocument.buildRandomPoint()

        let blob = patch.serialize()
        const patchCopy1 = Patch.deserialize(blob)
        assert.deepEqual(patchCopy1.getChanges(), patch.getChanges(), seedMessage)
        assert.deepEqual(patchCopy1.changeForOldPosition(oldPoint), patch.changeForOldPosition(oldPoint), seedMessage)

        const patchCopy2 = patch.copy()
        assert.deepEqual(patchCopy2.getChanges(), patch.getChanges(), seedMessage)
        assert.deepEqual(patchCopy2.changeForOldPosition(oldPoint), patch.changeForOldPosition(oldPoint), seedMessage)
      }

      patch.delete();
    }
  })

  it('does not crash when inconsistent splices are applied', () => {
    this.timeout(Infinity)

    for (let i = 0; i < 100; i++) {
      let seed = Date.now()
      const seedMessage = `Random seed: ${seed}`
      const random = new Random(seed)
      const document = new TestDocument(seed, 10)

      const patch = new Patch()

      for (let j = 0; j < 10; j++) {
        const start = document.buildRandomPoint()
        const insertedText = document.buildRandomLines(1, 4, true).join('\n')
        const deletedText = document.buildRandomLines(1, 4).join('\n')
        const deletedExtent = textHelpers.getExtent(deletedText)
        const insertedExtent = textHelpers.getExtent(insertedText)
        const currentChanges = patch.getChanges()

        try {
          patch.splice(
            start,
            deletedExtent,
            insertedExtent,
            deletedText,
            insertedText
          )
        } catch (error) {
          assert.equal(error.message, 'Patch does not apply')
          assert.deepEqual(patch.getChanges(), currentChanges, seedMessage)
        }
      }
    }
  })
})

function last (array) {
  return array[array.length - 1]
}

function translateSpliceFromOriginalDocument (originalDocument, patch, originalSplice) {
  const originalDeletionEnd = traverse(originalSplice.start, originalSplice.deletedExtent)
  const originalInsertionEnd = traverse(originalSplice.start, originalSplice.insertedExtent)

  let oldStart, newStart
  const startChange = patch.changeForOldPosition(originalSplice.start)
  if (startChange) {
    if (compare(originalSplice.start, startChange.oldEnd) < 0) {
      oldStart = startChange.oldStart
      newStart = startChange.newStart
    } else {
      oldStart = originalSplice.start
      newStart = traverse(startChange.newEnd, traversalDistance(originalSplice.start, startChange.oldEnd))
    }
  } else {
    oldStart = originalSplice.start
    newStart = originalSplice.start
  }

  let endChange
  for (const change of patch.getChanges()) {
    const comparison = compare(change.oldStart, originalDeletionEnd)
    if (comparison <= 0) endChange = change
    if (comparison >= 0 && compare(change.oldStart, originalSplice.start) > 0) break
  }

  let oldInsertionEnd, newDeletionEnd
  if (endChange) {
    if (compare(originalDeletionEnd, endChange.oldStart) === 0 &&
        compare(originalSplice.start, endChange.oldStart) < 0) {
      oldInsertionEnd = originalInsertionEnd
      newDeletionEnd = endChange.newStart
    } else if (compare(originalDeletionEnd, endChange.oldEnd) < 0) {
      oldInsertionEnd = traverse(originalInsertionEnd, traversalDistance(endChange.oldEnd, originalDeletionEnd))
      newDeletionEnd = endChange.newEnd
    } else {
      oldInsertionEnd = originalInsertionEnd
      newDeletionEnd = traverse(endChange.newEnd, traversalDistance(originalDeletionEnd, endChange.oldEnd))
    }
  } else {
    oldInsertionEnd = originalInsertionEnd
    newDeletionEnd = originalDeletionEnd
  }

  return {
    start: newStart,
    deletionExtent: traversalDistance(newDeletionEnd, newStart),
    insertedText: originalDocument.getTextInRange(oldStart, oldInsertionEnd)
  }
}
