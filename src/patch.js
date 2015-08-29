import SegmentTree from './segment-tree'
import {traversalDistance} from './point-helpers'

export default class Patch {
  constructor (seed) {
    this.segmentTree = new SegmentTree(seed)
  }

  splice (start, replacedExtent, replacementText) {
    this.segmentTree.splice(start, replacedExtent, replacementText)
  }

  getChanges () {
    let changes = []
    let iterator = this.segmentTree.buildIteratorAtStart()

    while (!iterator.next().done) {
      if (iterator.inChange()) {
        changes.push({
          start: iterator.getOutputStart(),
          replacedExtent: traversalDistance(iterator.getInputEnd(), iterator.getInputStart()),
          replacementText: iterator.getChangeText()
        })
      }
    }

    return changes
  }

  toHTML () {
    return this.segmentTree.toHTML()
  }
}
