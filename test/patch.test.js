 require('segfault-handler').registerHandler()

import Random from 'random-seed'
import Patch from '..'
import TestDocument from './helpers/test-document'
import {
  traverse, traversalDistance, compare, isZero, format as formatPoint
} from './helpers/point-helpers'

describe('Native Patch', function () {
  it('honors the mergeAdjacentHunks option set to false', function () {
    const patch = new Patch({mergeAdjacentHunks: false})

    patch.splice({row: 0, column: 10}, {row: 0, column: 0}, {row: 1, column: 5})
    patch.splice({row: 1, column: 5}, {row: 0, column: 2}, {row: 0, column: 8})

    assert.deepEqual(JSON.parse(JSON.stringify(patch.getHunks())), [
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
  })

  it('honors the mergeAdjacentHunks option set to true', function () {
    const patch = new Patch({mergeAdjacentHunks: true})

    patch.splice({row: 0, column: 5}, {row: 0, column: 1}, {row: 0, column: 2})
    patch.splice({row: 0, column: 10}, {row: 0, column: 3}, {row: 0, column: 4})
    assert.deepEqual(JSON.parse(JSON.stringify(patch.getHunks())), [
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
    assert.deepEqual(JSON.parse(JSON.stringify(patch.getHunks())), [
      {
        oldStart: {row: 0, column: 5}, oldEnd: {row: 0, column: 9},
        newStart: {row: 0, column: 5}, newEnd: {row: 0, column: 11}
      }
    ])
  })

  it('can compose multiple patches together', function () {
    const patches = [new Patch(), new Patch(), new Patch()]
    patches[0].splice({row: 0, column: 3}, {row: 0, column: 4}, {row: 0, column: 5}, 'ciao', 'hello')
    patches[0].splice({row: 1, column: 1}, {row: 0, column: 0}, {row: 0, column: 3}, '', 'hey')
    patches[1].splice({row: 0, column: 15}, {row: 0, column: 10}, {row: 0, column: 0}, '0123456789', '')
    patches[1].splice({row: 0, column: 0}, {row: 0, column: 0}, {row: 3, column: 0}, '', '\n\n\n')
    patches[2].splice({row: 4, column: 2}, {row: 0, column: 2}, {row: 0, column: 2}, 'so', 'ho')

    const composedPatch = Patch.compose(patches)
    assert.deepEqual(JSON.parse(JSON.stringify(composedPatch.getHunks())), [
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
  })

  it('can invert patches', function () {
    const patch = new Patch()
    patch.splice({row: 0, column: 3}, {row: 0, column: 4}, {row: 0, column: 5}, 'ciao', 'hello')
    patch.splice({row: 0, column: 10}, {row: 0, column: 5}, {row: 0, column: 5}, 'quick', 'world')

    const invertedPatch = patch.invert()
    assert.deepEqual(JSON.parse(JSON.stringify(invertedPatch.getHunks())), [
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
  })

  it('can serialize/deserialize patches', () => {
    const patch1 = new Patch()
    patch1.splice({row: 0, column: 3}, {row: 0, column: 5}, {row: 0, column: 5}, 'hello', 'world')

    const patch2 = Patch.deserialize(Buffer.from(patch1.serialize().toString('base64'), 'base64'))
    assert.deepEqual(JSON.parse(JSON.stringify(patch2.getHunks())), [{
      oldStart: {row: 0, column: 3},
      newStart: {row: 0, column: 3},
      oldEnd: {row: 0, column: 8},
      newEnd: {row: 0, column: 8},
      oldText: 'hello',
      newText: 'world'
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
      const mergeAdjacentHunks = random(2)
      const patch = new Patch({mergeAdjacentHunks: mergeAdjacentHunks})

      for (let j = 0; j < 20; j++) {
        if (random(10) < 1) {
          patch.rebalance()
        } else if (random(10) < 4) {
          const originalSplice = originalDocument.performRandomSplice()
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
          //   label="spliceOld(${formatPoint(originalSplice.start)}, ${formatPoint(originalSplice.deletedExtent)}, ${formatPoint(originalSplice.insertedExtent)})"
          // }\n`)

          patch.spliceOld(
            originalSplice.start,
            originalSplice.deletedExtent,
            originalSplice.insertedExtent
          )
        } else {
          const splice = mutatedDocument.performRandomSplice()

          // process.stderr.write(`graph message {
          //   label="splice(${formatPoint(splice.start)}, ${formatPoint(splice.deletedExtent)}, ${formatPoint(splice.insertedExtent)})"
          // }\n`)

          patch.splice(
            splice.start,
            splice.deletedExtent,
            splice.insertedExtent,
            splice.deletedText,
            splice.insertedText
          )
        }

        // patch.printDotGraph()

        const originalDocumentCopy = originalDocument.clone()
        const hunks = patch.getHunks()
        assert.equal(patch.getHunkCount(), hunks.length)

        let previousHunk
        for (let hunk of patch.getHunks()) {
          const oldExtent = traversalDistance(hunk.oldEnd, hunk.oldStart)
          assert.equal(hunk.newText, mutatedDocument.getTextInRange(hunk.newStart, hunk.newEnd), seedMessage)
          assert.equal(hunk.oldText, originalDocument.getTextInRange(hunk.oldStart, hunk.oldEnd), seedMessage)
          originalDocumentCopy.splice(hunk.newStart, oldExtent, hunk.newText)
          if (previousHunk && mergeAdjacentHunks) {
            assert.notDeepEqual(previousHunk.oldEnd, hunk.oldStart)
            assert.notDeepEqual(previousHunk.newEnd, hunk.newStart)
          }
          previousHunk = hunk
        }

        assert.deepEqual(originalDocumentCopy.getLines(), mutatedDocument.getLines(), seedMessage)

        for (let k = 0; k < 5; k++) {
          let oldRange = originalDocument.buildRandomRange()
          assert.deepEqual(
            patch.getHunksInOldRange(oldRange.start, oldRange.end),
            hunks.filter(hunk =>
              compare(hunk.oldEnd, oldRange.start) > 0 &&
              compare(hunk.oldStart, oldRange.end) < 0
            ),
            `old range: ${formatPoint(oldRange.start)} - ${formatPoint(oldRange.end)}, seed: ${seed}`
          )

          let newRange = mutatedDocument.buildRandomRange()
          assert.deepEqual(
            patch.getHunksInNewRange(newRange.start, newRange.end),
            hunks.filter(hunk =>
              compare(hunk.newEnd, newRange.start) > 0 &&
              compare(hunk.newStart, newRange.end) < 0
            ),
            `new range: ${formatPoint(newRange.start)} - ${formatPoint(newRange.end)}, seed: ${seed}`
          )

          let oldPoint = originalDocument.buildRandomPoint()
          assert.deepEqual(
            patch.hunkForOldPosition(oldPoint),
            last(hunks.filter(hunk => compare(hunk.oldStart, oldPoint) <= 0))
          )

          let newPoint = mutatedDocument.buildRandomPoint()
          assert.deepEqual(
            patch.hunkForNewPosition(newPoint),
            last(hunks.filter(hunk => compare(hunk.newStart, newPoint) <= 0))
          )
        }

        let blob = Buffer.from(patch.serialize().toString('base64'), 'base64')
        const patchCopy = Patch.deserialize(blob)
        assert.deepEqual(patchCopy.getHunks(), patch.getHunks())
      }
    }
  })
})

function last (array) {
  return array[array.length - 1]
}

function translateOldPosition (patch, oldPosition) {
  const hunk = patch.hunkForOldPosition(oldPosition)
  if (hunk) {
    if (compare(oldPosition, hunk.oldEnd) >= 0) {
      return traverse(hunk.newEnd, traversalDistance(oldPosition, hunk.oldEnd))
    } else {
      return min(
        hunk.newEnd,
        traverse(hunk.newStart, traversalDistance(oldPosition, hunk.oldStart))
      )
    }
  } else {
    return oldPosition
  }
}

function translateSpliceFromOriginalDocument(originalDocument, patch, originalSplice) {
  const originalDeletionEnd = traverse(originalSplice.start, originalSplice.deletedExtent)
  const originalInsertionEnd = traverse(originalSplice.start, originalSplice.insertedExtent)

  let oldStart, newStart
  const startHunk = patch.hunkForOldPosition(originalSplice.start)
  if (startHunk) {
    if (compare(originalSplice.start, startHunk.oldEnd) < 0) {
      oldStart = startHunk.oldStart
      newStart = startHunk.newStart
    } else {
      oldStart = originalSplice.start
      newStart = traverse(startHunk.newEnd, traversalDistance(originalSplice.start, startHunk.oldEnd))
    }
  } else {
    oldStart = originalSplice.start
    newStart = originalSplice.start
  }

  let oldInsertionEnd, newDeletionEnd
  const endHunk = patch.hunkForOldPosition(originalDeletionEnd)
  if (endHunk) {
    if (compare(originalDeletionEnd, endHunk.oldStart) === 0 &&
        compare(originalSplice.start, endHunk.oldStart) < 0) {
      oldInsertionEnd = originalInsertionEnd
      newDeletionEnd = endHunk.newStart
    } else if (compare(originalDeletionEnd, endHunk.oldEnd) < 0) {
      oldInsertionEnd = traverse(originalInsertionEnd, traversalDistance(endHunk.oldEnd, originalDeletionEnd))
      newDeletionEnd = endHunk.newEnd
    } else {
      oldInsertionEnd = originalInsertionEnd
      newDeletionEnd = traverse(endHunk.newEnd, traversalDistance(originalDeletionEnd, endHunk.oldEnd))
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
