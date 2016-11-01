require('segfault-handler').registerHandler()

import Random from 'random-seed'
import Patch from '../build/Debug/atom_patch'
import {INFINITY_POINT, traverse, traversalDistance, format as formatPoint, isZero as isZeroPoint} from '../src/point-helpers'
import TestDocument from './helpers/test-document'

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
        for (let hunk of patch.getHunks()) {
          const oldExtent = traversalDistance(hunk.oldEnd, hunk.oldStart)
          const newText = mutatedDocument.getTextInRange(hunk.newStart, hunk.newEnd)
          originalDocumentCopy.splice(hunk.newStart, oldExtent, newText)
        }

        assert.deepEqual(originalDocumentCopy.getLines(), mutatedDocument.getLines(), seedMessage)
      }
    }
  })
})
