import Random from 'random-seed'
import Patch from '../src/patch'
import {INFINITY_POINT, traverse, traversalDistance, format as formatPoint, isZero as isZeroPoint} from '../src/point-helpers'
import TestDocument from './helpers/test-document'
import './helpers/add-to-html-methods'

describe('Patch', function () {
  it('correctly serializes and deserializes changes', function () {
    let patch = new Patch
    patch.spliceWithText({row: 0, column: 3}, {row: 0, column: 4}, 'hello')
    patch.spliceWithText({row: 1, column: 1}, {row: 0, column: 0}, 'hey')
    patch.splice({row: 0, column: 15}, {row: 0, column: 10}, {row: 0, column: 0})
    patch.splice({row: 0, column: 0}, {row: 0, column: 0}, {row: 3, column: 0})
    patch.spliceWithText({row: 4, column: 2}, {row: 0, column: 2}, 'ho')

    assert.deepEqual(patch.getChanges(), Patch.deserialize(patch.serialize()).getChanges())
  })

  it('correctly composes changes from multiple patches', function () {
    let patches = [new Patch(), new Patch(), new Patch()]
    patches[0].spliceWithText({row: 0, column: 3}, {row: 0, column: 4}, 'hello')
    patches[0].spliceWithText({row: 1, column: 1}, {row: 0, column: 0}, 'hey')
    patches[1].splice({row: 0, column: 15}, {row: 0, column: 10}, {row: 0, column: 0})
    patches[1].splice({row: 0, column: 0}, {row: 0, column: 0}, {row: 3, column: 0})
    patches[2].spliceWithText({row: 4, column: 2}, {row: 0, column: 2}, 'ho')

    assert.deepEqual(Patch.compose(patches), [
      {oldStart: {row: 0, column: 0}, newStart: {row: 0, column: 0}, oldExtent: {row: 0, column: 0}, newExtent: {row: 3, column: 0}},
      {oldStart: {row: 0, column: 3}, newStart: {row: 3, column: 3}, oldExtent: {row: 0, column: 4}, newExtent: {row: 0, column: 5}, newText: 'hello'},
      {oldStart: {row: 0, column: 14}, newStart: {row: 3, column: 15}, oldExtent: {row: 0, column: 10}, newExtent: {row: 0, column: 0}},
      {oldStart: {row: 1, column: 1}, newStart: {row: 4, column: 1}, oldExtent: {row: 0, column: 0}, newExtent: {row: 0, column: 3}, newText: 'hho'}
    ])
  })

  it('correctly records basic non-overlapping splices', function () {
    let patch = new Patch()
    patch.spliceWithText({row: 0, column: 3}, {row: 0, column: 4}, 'hello')
    patch.spliceWithText({row: 0, column: 10}, {row: 0, column: 5}, 'world')
    assert.deepEqual(patch.getChanges(), [
      {oldStart: {row: 0, column: 3}, newStart: {row: 0, column: 3}, oldExtent: {row: 0, column: 4}, newExtent: {row: 0, column: 5}, newText: 'hello'},
      {oldStart: {row: 0, column: 9}, newStart: {row: 0, column: 10}, oldExtent: {row: 0, column: 5}, newExtent: {row: 0, column: 5}, newText: 'world'}
    ])
  })

  it('correctly records basic overlapping splices', function () {
    let patch = new Patch()
    patch.spliceWithText({row: 0, column: 3}, {row: 0, column: 4}, 'hello world')
    patch.spliceWithText({row: 0, column: 9}, {row: 0, column: 7}, 'sun')
    assert.deepEqual(patch.getChanges(), [
      {oldStart: {row: 0, column: 3}, newStart: {row: 0, column: 3}, oldExtent: {row: 0, column: 6}, newExtent: {row: 0, column: 9}, newText: 'hello sun'},
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

        verifyOutputChanges(patch, input, output, seedMessage)
        verifyInputChanges(patch, input, output, seedMessage)
      }
    }

    function verifyOutputChanges (patch, input, output, seedMessage) {
      let synthesizedOutput = ''
      patch.iterator.moveToBeginning()
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
      for (let {newStart, oldExtent, newText} of patch.getChanges()) {
        input.splice(newStart, oldExtent, newText)
      }
      assert.equal(input.getText(), output.getText(), seedMessage)
    }

    function verifyInputChanges (patch, input, output, seedMessage) {
      patch.iterator.moveToEnd()
      let synthesizedOutput = input.getTextInRange(patch.iterator.getInputEnd(), INFINITY_POINT)
      do {
        if (patch.iterator.inChange()) {
          assert(!(isZeroPoint(patch.iterator.getInputExtent()) && isZeroPoint(patch.iterator.getOutputExtent())), "Empty region found. " + seedMessage);
          synthesizedOutput = patch.iterator.getNewText() + synthesizedOutput
        } else {
          synthesizedOutput = input.getTextInRange(patch.iterator.getInputStart(), patch.iterator.getInputEnd()) + synthesizedOutput
        }
      } while (patch.iterator.moveToPredecessor())

      assert.equal(synthesizedOutput, output.getText(), seedMessage)

      input = input.clone()
      for (let {oldStart, oldExtent, newText} of patch.getChanges().slice().reverse()) {
        input.splice(oldStart, oldExtent, newText)
      }
      assert.equal(input.getText(), output.getText(), seedMessage)
    }
  })
})
