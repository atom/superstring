import Patch from '../src/patch'
import TestDocument from './helpers/test-document'
import {format as formatPoint} from '../src/point-helpers'

describe('Patch', function () {
  it('correctly records basic non-overlapping splices', function () {
    let seed = 123
    let patch = new Patch(seed)
    patch.spliceText({row: 0, column: 3}, {row: 0, column: 4}, 'hello')
    patch.spliceText({row: 0, column: 10}, {row: 0, column: 5}, 'world')
    assert.deepEqual(patch.getChanges(), [
      {start: {row: 0, column: 3}, replacedExtent: {row: 0, column: 4}, replacementText: 'hello'},
      {start: {row: 0, column: 10}, replacedExtent: {row: 0, column: 5}, replacementText: 'world'}
    ])
  })

  it('correctly records basic overlapping splices', function () {
    let seed = 123
    let patch = new Patch(seed)
    patch.spliceText({row: 0, column: 3}, {row: 0, column: 4}, 'hello world')
    patch.spliceText({row: 0, column: 9}, {row: 0, column: 7}, 'sun')
    assert.deepEqual(patch.getChanges(), [
      {start: {row: 0, column: 3}, replacedExtent: {row: 0, column: 6}, replacementText: 'hello sun'},
    ])
  })

  it('correctly records random splices', function () {
    this.timeout(Infinity)

    for (let i = 0; i < 500; i++) {
      let seed = Date.now()
      let input = new TestDocument(seed)
      let output = input.clone()
      let patch = new Patch(seed)

      for (let j = 0; j < 30; j++) {
        let {start, replacedExtent, replacementText} = output.performRandomSplice()
        patch.spliceText(start, replacedExtent, replacementText)
        verifyPatch(patch, input.clone(), output, seed)
      }
    }

    function verifyPatch (patch, input, output, seed) {
      for (let {start, replacedExtent, replacementText} of patch.getChanges()) {
        input.splice(start, replacedExtent, replacementText)
      }
      assert.equal(input.getText(), output.getText(), `Random seed: ${seed}`)
    }
  })
})
