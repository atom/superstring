 require('segfault-handler').registerHandler()

import Random from 'random-seed'
import Patch from '..'
import TestDocument from './helpers/test-document'
import {
  ZERO_POINT, traverse, traversalDistance, compare as comparePoints,
  format as formatPoint, min as minPoint, isZero
} from '../src/point-helpers'

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

  it('correctly records random splices', function () {
    this.timeout(Infinity)

    for (let i = 0; i < 1000; i++) {
      let seed = Date.now()
      const seedMessage = `Random seed: ${seed}`
      // console.log(seedMessage);

      const random = new Random(seed)
      const originalDocument = new TestDocument(seed)
      const mutatedDocument = originalDocument.clone()
      const mergeAdjacentHunks = random(2)
      const patch = new Patch({mergeAdjacentHunks: mergeAdjacentHunks})

      for (let j = 0; j < 10; j++) {
        if (random(10) < 3) {
          const originalSplice = originalDocument.performRandomSplice()
          const mutatedSplice = translateSpliceFromOriginalDocument(originalDocument, patch, originalSplice)

          mutatedDocument.splice(
            mutatedSplice.start,
            mutatedSplice.deletionExtent,
            mutatedSplice.insertedText
          )

          // process.stderr.write(`graph message {
          //   label="spliceOld(${formatPoint(originalSplice.start)}, ${formatPoint(originalSplice.deletedExtent)}, ${formatPoint(originalSplice.insertedExtent)})"
          // }\n`)

          patch.spliceOld(originalSplice.start, originalSplice.deletedExtent, originalSplice.insertedExtent)
        } else {
          const {start, deletedExtent, insertedExtent, insertedText} = mutatedDocument.performRandomSplice()

          // process.stderr.write(`graph message {
          //   label="splice(${formatPoint(start)}, ${formatPoint(deletedExtent)}, ${formatPoint(insertedExtent)})"
          // }\n`)

          patch.splice(start, deletedExtent, insertedExtent, insertedText)
        }

        // patch.printDotGraph()

        const originalDocumentCopy = originalDocument.clone()
        const hunks = patch.getHunks()
        let previousHunk
        for (let hunk of patch.getHunks()) {
          const oldExtent = traversalDistance(hunk.oldEnd, hunk.oldStart)
          assert.equal(hunk.newText, mutatedDocument.getTextInRange(hunk.newStart, hunk.newEnd), seedMessage)
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
              comparePoints(hunk.oldEnd, oldRange.start) > 0 &&
              comparePoints(hunk.oldStart, oldRange.end) < 0
            ),
            `old range: ${formatPoint(oldRange.start)} - ${formatPoint(oldRange.end)}, seed: ${seed}`
          )

          let newRange = mutatedDocument.buildRandomRange()
          assert.deepEqual(
            patch.getHunksInNewRange(newRange.start, newRange.end),
            hunks.filter(hunk =>
              comparePoints(hunk.newEnd, newRange.start) > 0 &&
              comparePoints(hunk.newStart, newRange.end) < 0
            ),
            `new range: ${formatPoint(newRange.start)} - ${formatPoint(newRange.end)}, seed: ${seed}`
          )

          let oldPoint = originalDocument.buildRandomPoint()
          assert.deepEqual(
            patch.hunkForOldPosition(oldPoint),
            last(hunks.filter(hunk => comparePoints(hunk.oldStart, oldPoint) <= 0))
          )

          let newPoint = mutatedDocument.buildRandomPoint()
          assert.deepEqual(
            patch.hunkForNewPosition(newPoint),
            last(hunks.filter(hunk => comparePoints(hunk.newStart, newPoint) <= 0))
          )
        }

        let blob = patch.serialize()
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
    if (comparePoints(oldPosition, hunk.oldEnd) >= 0) {
      return traverse(hunk.newEnd, traversalDistance(oldPosition, hunk.oldEnd))
    } else {
      return minPoint(
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
    if (comparePoints(originalSplice.start, startHunk.oldEnd) < 0) {
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
    if (comparePoints(originalDeletionEnd, endHunk.oldStart) === 0 &&
        comparePoints(originalSplice.start, endHunk.oldStart) < 0) {
      oldInsertionEnd = originalInsertionEnd
      newDeletionEnd = endHunk.newStart
    } else if (comparePoints(originalDeletionEnd, endHunk.oldEnd) < 0) {
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
