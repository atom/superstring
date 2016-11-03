require('segfault-handler').registerHandler()

import Random from 'random-seed'
import Patch from '../build/Debug/atom_patch'
import TestDocument from './helpers/test-document'
import {
  ZERO_POINT, traverse, traversalDistance, compare as comparePoints,
  format as formatPoint, min as minPoint
} from '../src/point-helpers'

describe('Native Patch', function () {
  it('correctly records random splices', function () {
    this.timeout(Infinity)

    for (let i = 0; i < 1000; i++) {
      const seed = Date.now()
      const seedMessage = `Random seed: ${seed}`
      const random = new Random(seed)
      const originalDocument = new TestDocument(seed)
      const mutatedDocument = originalDocument.clone()
      const patch = new Patch()

      for (let j = 0; j < 10; j++) {
        const {start, oldText, oldExtent, newExtent, newText} = mutatedDocument.performRandomSplice()
        patch.splice(start, oldExtent, newExtent, oldText, newText)

        // process.stderr.write(`graph message {
        //   label="splice(${formatPoint(start)}, ${formatPoint(oldExtent)}, ${formatPoint(newExtent)})"
        // }\n`)
        // patch.printDotGraph()

        const originalDocumentCopy = originalDocument.clone()
        const hunks = patch.getHunks()
        for (let hunk of patch.getHunks()) {
          const oldExtent = traversalDistance(hunk.oldEnd, hunk.oldStart)
          const newText = mutatedDocument.getTextInRange(hunk.newStart, hunk.newEnd)
          originalDocumentCopy.splice(hunk.newStart, oldExtent, newText)
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
            patch.translateOldPosition(oldPoint),
            translateOldPositionSlowly(hunks, oldPoint)
          )

          let newPoint = mutatedDocument.buildRandomPoint()
          assert.deepEqual(
            patch.translateNewPosition(newPoint),
            translateNewPositionSlowly(hunks, newPoint)
          )
        }

        let blob = patch.serialize()
        const patchCopy = Patch.deserialize(blob)
        assert.deepEqual(patchCopy.getHunks(), patch.getHunks())
      }
    }
  })
})

function translateOldPositionSlowly (hunks, target) {
  let lastOldPosition = ZERO_POINT
  let lastNewPosition = ZERO_POINT

  for (let hunk of hunks) {
    if (comparePoints(hunk.oldEnd, target) <= 0) {
      lastOldPosition = hunk.oldEnd
      lastNewPosition = hunk.newEnd
    } else if (comparePoints(hunk.oldStart, target) <= 0) {
      return minPoint(
        hunk.newEnd,
        traverse(hunk.newStart, traversalDistance(target, hunk.oldStart))
      )
    } else {
      break
    }
  }

  return traverse(lastNewPosition, traversalDistance(target, lastOldPosition))
}

function translateNewPositionSlowly (hunks, target) {
  let lastOldPosition = ZERO_POINT
  let lastNewPosition = ZERO_POINT

  for (let hunk of hunks) {
    if (comparePoints(hunk.newEnd, target) <= 0) {
      lastOldPosition = hunk.oldEnd
      lastNewPosition = hunk.newEnd
    } else if (comparePoints(hunk.newStart, target) <= 0) {
      return minPoint(
        hunk.oldEnd,
        traverse(hunk.oldStart, traversalDistance(target, hunk.newStart))
      )
    } else {
      break
    }
  }

  return traverse(lastOldPosition, traversalDistance(target, lastNewPosition))
}
