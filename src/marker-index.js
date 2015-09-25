import Random from 'random-seed'

import Iterator from './iterator'
import {addToSet} from './helpers'

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

  insert (markerId, startOffset, endOffset) {
    let startNode = this.iterator.insertMarkerStart(markerId, startOffset, endOffset)
    let endNode = this.iterator.insertMarkerEnd(markerId, startOffset, endOffset)

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
    if (!this.root || oldExtent === 0 && newExtent === 0) return

    let isInsertion = oldExtent === 0
    let startNode = this.iterator.insertSpliceBoundary(start, false)
    let endNode = this.iterator.insertSpliceBoundary(start + oldExtent, isInsertion)

    startNode.priority = -2
    this.bubbleNodeUp(startNode)
    endNode.priority = -1
    this.bubbleNodeUp(endNode)

    if (isInsertion) {
      for (let markerId of startNode.startMarkerIds) {
        if (this.exclusiveMarkers.has(markerId)) {
          startNode.startMarkerIds.delete(markerId)
          endNode.startMarkerIds.add(markerId)
          endNode.leftMarkerIds.delete(markerId)
          this.startNodesById[markerId] = endNode
        }
      }
      for (let markerId of startNode.endMarkerIds) {
        if (!this.exclusiveMarkers.has(markerId) || endNode.startMarkerIds.has(markerId)) {
          startNode.endMarkerIds.delete(markerId)
          endNode.endMarkerIds.add(markerId)
          if (!endNode.startMarkerIds.has(markerId)) {
            endNode.leftMarkerIds.add(markerId)
          }
          this.endNodesById[markerId] = endNode
        }
      }
    } else if (endNode.left) {
      let startMarkerIds = new Set()
      let endMarkerIds = new Set()
      this.getStartingAndEndingsMarkerWithinSubtree(endNode.left, startMarkerIds, endMarkerIds)

      for (let markerId of startMarkerIds) {
        endNode.startMarkerIds.add(markerId)
        this.startNodesById[markerId] = endNode
      }

      for (let markerId of endMarkerIds) {
        endNode.endMarkerIds.add(markerId)
        if (!startMarkerIds.has(markerId)) {
          endNode.leftMarkerIds.add(markerId)
        }
        this.endNodesById[markerId] = endNode
      }

      endNode.left = null
    }

    endNode.leftExtent += (newExtent - oldExtent)

    if (endNode.leftExtent === 0) {
      for (let markerId of endNode.startMarkerIds) {
        startNode.startMarkerIds.add(markerId)
        endNode.leftMarkerIds.add(markerId)
        this.startNodesById[markerId] = startNode
      }
      for (let markerId of endNode.endMarkerIds) {
        startNode.endMarkerIds.add(markerId)
        endNode.leftMarkerIds.delete(markerId)
        this.endNodesById[markerId] = startNode
      }
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
  }

  findIntersecting (start, end = start) {
    let intersecting = new Set()
    this.iterator.findIntersecting(start, end, intersecting)
    return intersecting
  }

  findContaining (start, end = start) {
    let containing = new Set()
    this.iterator.findContaining(start, containing)
    if (end !== start) {
      let containingEnd = new Set()
      this.iterator.findContaining(end, containingEnd)
      for (let markerId of containing) {
        if (!containingEnd.has(markerId)) containing.delete(markerId)
      }
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
        offset += node.parent.leftExtent
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

    pivot.leftExtent = root.leftExtent + pivot.leftExtent

    addToSet(pivot.rightMarkerIds, root.rightMarkerIds)

    for (let markerId of pivot.leftMarkerIds) {
      if (root.leftMarkerIds.has(markerId)) {
        root.leftMarkerIds.delete(markerId)
      } else {
        pivot.leftMarkerIds.delete(markerId)
        root.rightMarkerIds.add(markerId)
      }
    }
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

    root.leftExtent = root.leftExtent - pivot.leftExtent

    for (let markerId of root.leftMarkerIds) {
      if (!pivot.startMarkerIds.has(markerId)) { // don't do this when pivot is at offset 0
        pivot.leftMarkerIds.add(markerId)
      }
    }

    for (let markerId of pivot.rightMarkerIds) {
      if (root.rightMarkerIds.has(markerId)) {
        root.rightMarkerIds.delete(markerId)
      } else {
        pivot.rightMarkerIds.delete(markerId)
        root.leftMarkerIds.add(markerId)
      }
    }
  }

  getStartingAndEndingsMarkerWithinSubtree (node, startMarkerIds, endMarkerIds) {
    addToSet(startMarkerIds, node.startMarkerIds)
    addToSet(endMarkerIds, node.endMarkerIds)
    if (node.left) {
      this.getStartingAndEndingsMarkerWithinSubtree(node.left, startMarkerIds, endMarkerIds)
    }
    if (node.right) {
      this.getStartingAndEndingsMarkerWithinSubtree(node.right, startMarkerIds, endMarkerIds)
    }
  }
}
