 // require('segfault-handler').registerHandler()

import Random from 'random-seed'
import Patch from '..'
import TestDocument from './helpers/test-document'
import {
  ZERO_POINT, traverse, traversalDistance, compare as comparePoints,
  format as formatPoint, min as minPoint
} from '../src/point-helpers'

describe('Native Patch', function () {
  it('honors the mergeAdjacentHunks option', function () {
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

  it('correctly records random splices', function () {
    this.timeout(Infinity)

    for (let i = 0; i < 10000; i++) {
      let seed = Date.now()
      seed = 1478825425386
      const seedMessage = `Random seed: ${seed}`
      console.log(seedMessage);
      const random = new Random(seed)
      const originalDocument = new TestDocument(seed)
      const mutatedDocument = originalDocument.clone()
      const patch = new Patch({mergeAdjacentHunks: false})

      for (let j = 0; j < 2; j++) {
        if (random(10) < 3) {
          const {start, deletedExtent, insertedExtent, insertedText, deletedText} = originalDocument.performRandomSplice()

          const newStart = translateOldPosition(patch, start)
          const newDeletionEnd = translateOldPosition(patch, traverse(start, deletedExtent))
          mutatedDocument.splice(newStart, traversalDistance(newDeletionEnd, newStart), insertedText)

          process.stderr.write(`graph message {
            label="spliceOld(${formatPoint(start)}, ${formatPoint(deletedExtent)}, ${formatPoint(insertedExtent)})"
          }\n`)

          patch.spliceOld(start, deletedExtent, insertedExtent)
        } else {
          const {start, deletedExtent, insertedExtent, insertedText} = mutatedDocument.performRandomSplice()

          process.stderr.write(`graph message {
            label="splice(${formatPoint(start)}, ${formatPoint(deletedExtent)}, ${formatPoint(insertedExtent)})"
          }\n`)

          patch.splice(start, deletedExtent, insertedExtent, insertedText)
        }

        patch.printDotGraph()

        const originalDocumentCopy = originalDocument.clone()
        const hunks = patch.getHunks()
        for (let hunk of patch.getHunks()) {
          const oldExtent = traversalDistance(hunk.oldEnd, hunk.oldStart)
          assert.equal(hunk.newText, mutatedDocument.getTextInRange(hunk.newStart, hunk.newEnd), seedMessage)
          originalDocumentCopy.splice(hunk.newStart, oldExtent, hunk.newText)
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
