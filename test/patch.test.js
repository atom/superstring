import Random from 'random-seed'
import Patch from '../src/patch'
import {traverse, traversalDistance, format as formatPoint, isZero as isZeroPoint} from '../src/point-helpers'
import TestDocument from './helpers/test-document'
import './helpers/add-to-html-methods'

describe('Patch', function () {
  describe('.prototype.getChangesInReverse()', function () {
    it('returns changes that can be spliced into a patch in reverse', function () {
      let patch = new Patch()
      patch.splice({row: 0, column: 3}, {row: 0, column: 2}, {row: 3, column: 4})
      patch.splice({row: 5, column: 10}, {row: 0, column: 0}, {row: 1, column: 1})
      patch.splice({row: 8, column: 20}, {row: 0, column: 0}, {row: 0, column: 3})
      patch.splice({row: 8, column: 28}, {row: 0, column: 0}, {row: 0, column: 1})

      let reversePatch = new Patch()
      patch.getChangesInReverse().reverse().forEach(change => reversePatch.splice(change.start, change.oldExtent, change.newExtent))

      assert.deepEqual(patch.getChanges(), reversePatch.getChanges())
    })
  })

  it('correctly records basic non-overlapping splices', function () {
    let patch = new Patch()
    patch.spliceWithText({row: 0, column: 3}, {row: 0, column: 4}, 'hello')
    patch.spliceWithText({row: 0, column: 10}, {row: 0, column: 5}, 'world')
    assert.deepEqual(patch.getChanges(), [
      {start: {row: 0, column: 3}, oldExtent: {row: 0, column: 4}, newExtent: {row: 0, column: 5}, newText: 'hello'},
      {start: {row: 0, column: 10}, oldExtent: {row: 0, column: 5}, newExtent: {row: 0, column: 5}, newText: 'world'}
    ])
  })

  it('correctly records basic overlapping splices', function () {
    let patch = new Patch()
    patch.spliceWithText({row: 0, column: 3}, {row: 0, column: 4}, 'hello world')
    patch.spliceWithText({row: 0, column: 9}, {row: 0, column: 7}, 'sun')
    assert.deepEqual(patch.getChanges(), [
      {start: {row: 0, column: 3}, oldExtent: {row: 0, column: 6}, newExtent: {row: 0, column: 9}, newText: 'hello sun'},
    ])
  })

  it('correctly records random splices', function () {
    this.timeout(Infinity)

    for (let i = 0; i < 1000; i++) {
      let seed = Date.now()
      let seedMessage = `Random seed: ${seed}`
      let random = new Random(seed)
      let input = new TestDocument(seed)
      let output = input.clone()
      let patch = new Patch()

      for (let j = 0; j < 10; j++) {
        let {start, oldExtent, newExtent, newText} = output.performRandomSplice()
        patch.spliceWithText(start, oldExtent, newText)
        // document.write(`<div>after splice(${formatPoint(start)}, ${formatPoint(oldExtent)}, ${formatPoint(newExtent)}, ${newText})</div>`)
        // document.write(patch.toHTML())
        // document.write('<hr>')

        let shouldRebalance = Boolean(random(2))
        if (shouldRebalance) patch.rebalance()

        verifySynthesizedOutput(patch, input, output, seedMessage)
      }
    }

    function verifySynthesizedOutput (patch, input, output, seedMessage) {
      let synthesizedOutput = ''
      patch.iterator.rewind()
      do {
        if (patch.iterator.inChange()) {
          assert(!(isZeroPoint(patch.iterator.getInputExtent()) && isZeroPoint(patch.iterator.getOutputExtent())), "Empty region found. " + seedMessage);
          synthesizedOutput += patch.iterator.getNewText()
        } else {
          synthesizedOutput += input.getTextInRange(patch.iterator.getInputStart(), patch.iterator.getInputEnd())
        }
      } while (patch.iterator.moveToSuccessor())

      assert.equal(synthesizedOutput, output.getText(), seedMessage)

      input = input.clone()
      for (let {start, oldExtent, newText} of patch.getChanges()) {
        input.splice(start, oldExtent, newText)
      }
      assert.equal(input.getText(), output.getText(), seedMessage)
    }
  })
})
