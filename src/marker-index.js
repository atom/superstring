import Random from 'random-seed'

import Iterator from './iterator'
import {addToSet} from './helpers'
import {compare, isZero, traversal, traverse} from './point-helpers'

const MAX_PRIORITY = 2147483647 // max 32 bit signed int (unboxed in v8)

export default class MarkerIndex {
  constructor (seed) {
    this.random = new Random(seed)
    this.root = null
    this.startNodesById = {}
    this.endNodesById = {}
    this.iterator = new Iterator(this)
    this.exclusiveMarkers = new Set()
  }

  dump (filterSet) {
    return this.iterator.dump(filterSet)
  }

  getRange (markerId) {
    return [this.getStart(markerId), this.getEnd(markerId)]
  }

  getStart (markerId) {
    return this.getNodeOffset(this.startNodesById[markerId])
  }

  getEnd (markerId) {
    return this.getNodeOffset(this.endNodesById[markerId])
  }

  insert (markerId, start, end) {
    let startNode = this.iterator.insertMarkerStart(markerId, start, end)
    let endNode = this.iterator.insertMarkerEnd(markerId, start, end)

    startNode.startMarkerIds.add(markerId)
    endNode.endMarkerIds.add(markerId)

    startNode.priority = this.random(MAX_PRIORITY)
    this.bubbleNodeUp(startNode)

    endNode.priority = this.random(MAX_PRIORITY)
    this.bubbleNodeUp(endNode)

    this.startNodesById[markerId] = startNode
    this.endNodesById[markerId] = endNode
  }

  setExclusive (markerId, exclusive) {
    if (exclusive) {
      this.exclusiveMarkers.add(markerId)
    } else {
      this.exclusiveMarkers.delete(markerId)
    }
  }

  isExclusive (markerId) {
    return this.exclusiveMarkers.has(markerId)
  }

  delete (markerId) {
    let startNode = this.startNodesById[markerId]
    let endNode = this.endNodesById[markerId]

    let node = startNode
    while (node) {
      node.rightMarkerIds.delete(markerId)
      node = node.parent
    }

    node = endNode
    while (node) {
      node.leftMarkerIds.delete(markerId)
      node = node.parent
    }

    startNode.startMarkerIds.delete(markerId)
    endNode.endMarkerIds.delete(markerId)

    if (!startNode.isMarkerEndpoint()) {
      this.deleteNode(startNode)
    }

    if (endNode !== startNode && !endNode.isMarkerEndpoint()) {
      this.deleteNode(endNode)
    }

    delete this.startNodesById[markerId]
    delete this.endNodesById[markerId]
  }

  splice (start, oldExtent, newExtent) {
    let invalidated = {
      touch: new Set,
      inside: new Set,
      overlap: new Set,
      surround: new Set
    }

    if (!this.root || isZero(oldExtent) && isZero(newExtent)) return invalidated

    let isInsertion = isZero(oldExtent)
    let startNode = this.iterator.insertSpliceBoundary(start, false)
    let endNode = this.iterator.insertSpliceBoundary(traverse(start, oldExtent), isInsertion)

    startNode.priority = -1
    this.bubbleNodeUp(startNode)
    endNode.priority = -2
    this.bubbleNodeUp(endNode)

    if (isInsertion) {
      startNode.startMarkerIds.forEach(markerId => {
        if (this.exclusiveMarkers.has(markerId)) {
          startNode.startMarkerIds.delete(markerId)
          startNode.rightMarkerIds.delete(markerId)
          endNode.startMarkerIds.add(markerId)
          this.startNodesById[markerId] = endNode
        }
      })
      startNode.endMarkerIds.forEach(markerId => {
        if (!this.exclusiveMarkers.has(markerId) || endNode.startMarkerIds.has(markerId)) {
          startNode.endMarkerIds.delete(markerId)
          if (!endNode.startMarkerIds.has(markerId)) {
            startNode.rightMarkerIds.add(markerId)
          }
          endNode.endMarkerIds.add(markerId)
          this.endNodesById[markerId] = endNode
        }
      })
    }

    let startingInsideSplice, endingInsideSplice
    if (startNode.right) {
      startingInsideSplice = new Set
      endingInsideSplice = new Set
      this.getStartingAndEndingMarkersWithinSubtree(startNode.right, startingInsideSplice, endingInsideSplice)
    }

    this.populateSpliceInvalidationSets(invalidated, startNode, endNode, startingInsideSplice, endingInsideSplice, isInsertion)

    if (startNode.right) {
      startingInsideSplice.forEach(markerId => {
        endNode.startMarkerIds.add(markerId)
        this.startNodesById[markerId] = endNode
      })

      endingInsideSplice.forEach(markerId => {
        endNode.endMarkerIds.add(markerId)
        if (!startingInsideSplice.has(markerId)) {
          startNode.rightMarkerIds.add(markerId)
        }
        this.endNodesById[markerId] = endNode
      })

      startNode.right = null
    }

    endNode.leftExtent = traverse(start, newExtent)

    if (compare(startNode.leftExtent, endNode.leftExtent) === 0) {
      endNode.startMarkerIds.forEach(markerId => {
        startNode.startMarkerIds.add(markerId)
        startNode.rightMarkerIds.add(markerId)
        this.startNodesById[markerId] = startNode
      })
      endNode.endMarkerIds.forEach(markerId => {
        startNode.endMarkerIds.add(markerId)
        if (endNode.leftMarkerIds.has(markerId)) {
          startNode.leftMarkerIds.add(markerId)
          endNode.leftMarkerIds.delete(markerId)
        }
        this.endNodesById[markerId] = startNode
      })
      this.deleteNode(endNode)
    } else if (endNode.isMarkerEndpoint()) {
      endNode.priority = this.random(MAX_PRIORITY)
      this.bubbleNodeDown(endNode)
    } else {
      this.deleteNode(endNode)
    }

    if (startNode.isMarkerEndpoint()) {
      startNode.priority = this.random(MAX_PRIORITY)
      this.bubbleNodeDown(startNode)
    } else {
      this.deleteNode(startNode)
    }

    return invalidated
  }

  findIntersecting (start, end = start) {
    let intersecting = new Set()
    this.iterator.findIntersecting(start, end, intersecting)
    return intersecting
  }

  findContaining (start, end = start) {
    let containing = new Set()
    this.iterator.findContaining(start, containing)
    if (compare(end, start) !== 0) {
      let containingEnd = new Set()
      this.iterator.findContaining(end, containingEnd)
      containing.forEach(function (markerId) {
        if (!containingEnd.has(markerId)) containing.delete(markerId)
      })
    }
    return containing
  }

  findContainedIn (start, end) {
    let containedIn = new Set()
    this.iterator.findContainedIn(start, end, containedIn)
    return containedIn
  }

  findStartingIn (start, end) {
    let startingIn = new Set()
    this.iterator.findStartingIn(start, end, startingIn)
    return startingIn
  }

  findEndingIn (start, end) {
    let endingIn = new Set()
    this.iterator.findEndingIn(start, end, endingIn)
    return endingIn
  }

  findStartingAt (offset) {
    return this.findStartingIn(offset, offset)
  }

  findEndingAt (offset) {
    return this.findEndingIn(offset, offset)
  }

  getNodeOffset (node) {
    let offset = node.leftExtent
    while (node.parent) {
      if (node.parent.right === node) {
        offset = traverse(node.parent.leftExtent, offset)
      }
      node = node.parent
    }
    return offset
  }

  deleteNode (node) {
    node.priority = Infinity
    this.bubbleNodeDown(node)
    if (node.parent) {
      if (node.parent.left === node) {
        node.parent.left = null
      } else {
        node.parent.right = null
      }
    } else {
      this.root = null
    }
  }

  bubbleNodeUp (node) {
    while (node.parent && node.priority < node.parent.priority) {
      if (node === node.parent.left) {
        this.rotateNodeRight(node)
      } else {
        this.rotateNodeLeft(node)
      }
    }
  }

  bubbleNodeDown (node) {
    while (true) {
      let leftChildPriority = node.left ? node.left.priority : Infinity
      let rightChildPriority = node.right ? node.right.priority : Infinity

      if (leftChildPriority < rightChildPriority && leftChildPriority < node.priority) {
        this.rotateNodeRight(node.left)
      } else if (rightChildPriority < node.priority) {
        this.rotateNodeLeft(node.right)
      } else {
        break
      }
    }
  }

  rotateNodeLeft (pivot) {
    let root = pivot.parent

    if (root.parent) {
      if (root.parent.left === root) {
        root.parent.left = pivot
      } else {
        root.parent.right = pivot
      }
    } else {
      this.root = pivot
    }
    pivot.parent = root.parent

    root.right = pivot.left
    if (root.right) {
      root.right.parent = root
    }

    pivot.left = root
    pivot.left.parent = pivot

    pivot.leftExtent = traverse(root.leftExtent, pivot.leftExtent)

    addToSet(pivot.rightMarkerIds, root.rightMarkerIds)

    pivot.leftMarkerIds.forEach(function (markerId) {
      if (root.leftMarkerIds.has(markerId)) {
        root.leftMarkerIds.delete(markerId)
      } else {
        pivot.leftMarkerIds.delete(markerId)
        root.rightMarkerIds.add(markerId)
      }
    })
  }

  rotateNodeRight (pivot) {
    let root = pivot.parent

    if (root.parent) {
      if (root.parent.left === root) {
        root.parent.left = pivot
      } else {
        root.parent.right = pivot
      }
    } else {
      this.root = pivot
    }
    pivot.parent = root.parent

    root.left = pivot.right
    if (root.left) {
      root.left.parent = root
    }

    pivot.right = root
    pivot.right.parent = pivot

    root.leftExtent = traversal(root.leftExtent, pivot.leftExtent)

    root.leftMarkerIds.forEach(function (markerId) {
      if (!pivot.startMarkerIds.has(markerId)) { // don't do this when pivot is at offset 0
        pivot.leftMarkerIds.add(markerId)
      }
    })

    pivot.rightMarkerIds.forEach(function (markerId) {
      if (root.rightMarkerIds.has(markerId)) {
        root.rightMarkerIds.delete(markerId)
      } else {
        pivot.rightMarkerIds.delete(markerId)
        root.leftMarkerIds.add(markerId)
      }
    })
  }

  getStartingAndEndingMarkersWithinSubtree (node, startMarkerIds, endMarkerIds) {
    addToSet(startMarkerIds, node.startMarkerIds)
    addToSet(endMarkerIds, node.endMarkerIds)
    if (node.left) {
      this.getStartingAndEndingMarkersWithinSubtree(node.left, startMarkerIds, endMarkerIds)
    }
    if (node.right) {
      this.getStartingAndEndingMarkersWithinSubtree(node.right, startMarkerIds, endMarkerIds)
    }
  }

  populateSpliceInvalidationSets (invalidated, startNode, endNode, startingInsideSplice, endingInsideSplice, isInsertion) {
    addToSet(invalidated.touch, startNode.endMarkerIds)
    addToSet(invalidated.touch, endNode.startMarkerIds)
    startNode.rightMarkerIds.forEach(function (markerId) {
      invalidated.touch.add(markerId)
      if (!(isInsertion && (startNode.startMarkerIds.has(markerId) || endNode.endMarkerIds.has(markerId)))) {
        invalidated.inside.add(markerId)
      }
    })
    endNode.leftMarkerIds.forEach(function (markerId) {
      invalidated.touch.add(markerId)
      invalidated.inside.add(markerId)
    })
    if (startingInsideSplice && endingInsideSplice) {
      startingInsideSplice.forEach(function (markerId) {
        invalidated.touch.add(markerId)
        invalidated.inside.add(markerId)
        invalidated.overlap.add(markerId)
        if (endingInsideSplice.has(markerId)) invalidated.surround.add(markerId)
      })
      endingInsideSplice.forEach(function (markerId) {
        invalidated.touch.add(markerId)
        invalidated.inside.add(markerId)
        invalidated.overlap.add(markerId)
      })
    }
  }
}
