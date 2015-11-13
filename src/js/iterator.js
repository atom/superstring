import Node from './node'
import {addToSet} from './helpers'
import {compare, isZero, traversal, traverse} from './point-helpers'

export default class Iterator {
  constructor (markerIndex) {
    this.markerIndex = markerIndex
  }

  reset () {
    this.node = this.markerIndex.root
    this.nodeOffset = this.node ? this.node.leftExtent : null
    this.leftAncestorOffset = {row: 0, column: 0}
    this.rightAncestorOffset = {row: Infinity, column: Infinity}
    this.leftAncestorOffsetStack = []
    this.rightAncestorOffsetStack = []
  }

  insertMarkerStart (markerId, startOffset, endOffset) {
    this.reset()

    if (!this.node) {
      let node = new Node(null, startOffset)
      this.markerIndex.root = node
      return node
    }

    while (true) {
      let comparison = compare(startOffset, this.nodeOffset)
      if (comparison === 0) {
        this.markRight(markerId, startOffset, endOffset)
        return this.node
      } else if (comparison < 0) {
        this.markRight(markerId, startOffset, endOffset)
        if (this.node.left) {
          this.descendLeft()
        } else {
          this.insertLeftChild(startOffset)
          this.descendLeft()
          this.markRight(markerId, startOffset, endOffset)
          return this.node
        }
      } else { // startOffset > this.nodeOffset
        if (this.node.right) {
          this.descendRight()
        } else {
          this.insertRightChild(startOffset)
          this.descendRight()
          this.markRight(markerId, startOffset, endOffset)
          return this.node
        }
      }
    }
  }

  insertMarkerEnd (markerId, startOffset, endOffset) {
    this.reset()

    if (!this.node) {
      let node = new Node(null, endOffset)
      this.markerIndex.root = node
      return node
    }

    while (true) {
      let comparison = compare(endOffset, this.nodeOffset)
      if (comparison === 0) {
        this.markLeft(markerId, startOffset, endOffset)
        return this.node
      } else if (comparison < 0) {
        if (this.node.left) {
          this.descendLeft()
        } else {
          this.insertLeftChild(endOffset)
          this.descendLeft()
          this.markLeft(markerId, startOffset, endOffset)
          return this.node
        }
      } else { // endOffset > this.nodeOffset
        this.markLeft(markerId, startOffset, endOffset)
        if (this.node.right) {
          this.descendRight()
        } else {
          this.insertRightChild(endOffset)
          this.descendRight()
          this.markLeft(markerId, startOffset, endOffset)
          return this.node
        }
      }
    }
  }

  insertSpliceBoundary (offset, isInsertionEnd) {
    this.reset()

    while (true) {
      let comparison = compare(offset, this.nodeOffset)
      if (comparison === 0 && !isInsertionEnd) {
        return this.node
      } else if (comparison < 0) {
        if (this.node.left) {
          this.descendLeft()
        } else {
          this.insertLeftChild(offset)
          return this.node.left
        }
      } else { // endOffset > this.nodeOffset
        if (this.node.right) {
          this.descendRight()
        } else {
          this.insertRightChild(offset)
          return this.node.right
        }
      }
    }
  }

  findIntersecting (start, end, resultSet) {
    this.reset()
    if (!this.node) return

    while (true) {
      if (compare(start, this.nodeOffset) < 0) {
        if (this.node.left) {
          this.checkIntersection(start, end, resultSet)
          this.descendLeft()
        } else {
          break
        }
      } else {
        if (this.node.right) {
          this.checkIntersection(start, end, resultSet)
          this.descendRight()
        } else {
          break
        }
      }
    }

    do {
      this.checkIntersection(start, end, resultSet)
      this.moveToSuccessor()
    } while (this.node && compare(this.nodeOffset, end) <= 0)
  }

  findContaining (offset, resultSet) {
    this.reset()
    if (!this.node) return

    while (true) {
      this.checkIntersection(offset, offset, resultSet)

      if (compare(offset, this.nodeOffset) < 0) {
        if (this.node.left) {
          this.descendLeft()
        } else {
          break
        }
      } else {
        if (this.node.right) {
          this.descendRight()
        } else {
          break
        }
      }
    }
  }

  findContainedIn (start, end, resultSet) {
    this.reset()
    if (!this.node) return

    this.seekToFirstNodeGreaterThanOrEqualTo(start)

    let started = new Set()
    while (this.node && compare(this.nodeOffset, end) <= 0) {
      addToSet(started, this.node.startMarkerIds)
      this.node.endMarkerIds.forEach(function (markerId) {
        if (started.has(markerId)) {
          resultSet.add(markerId)
        }
      })
      this.moveToSuccessor()
    }
  }

  findStartingIn (start, end, resultSet) {
    this.reset()
    if (!this.node) return

    this.seekToFirstNodeGreaterThanOrEqualTo(start)

    while (this.node && compare(this.nodeOffset, end) <= 0) {
      addToSet(resultSet, this.node.startMarkerIds)
      this.moveToSuccessor()
    }
  }

  findEndingIn (start, end, resultSet) {
    this.reset()
    if (!this.node) return

    this.seekToFirstNodeGreaterThanOrEqualTo(start)

    while (this.node && compare(this.nodeOffset, end) <= 0) {
      addToSet(resultSet, this.node.endMarkerIds)
      this.moveToSuccessor()
    }
  }

  dump (filterSet) {
    this.reset()

    while (this.node && this.node.left) this.descendLeft()

    let snapshot = {}

    while (this.node) {
      this.node.startMarkerIds.forEach(markerId => {
        if (!filterSet || filterSet.has(markerId)) {
          snapshot[markerId] = {start: this.nodeOffset, end: null}
        }
      })

      this.node.endMarkerIds.forEach(markerId => {
        if (!filterSet || filterSet.has(markerId)) {
          snapshot[markerId].end = this.nodeOffset
        }
      })

      this.moveToSuccessor()
    }

    return snapshot
  }

  seekToFirstNodeGreaterThanOrEqualTo (offset) {
    while (true) {
      let comparison = compare(offset, this.nodeOffset)

      if (comparison === 0) {
        break
      } else if (comparison < 0) {
        if (this.node.left) {
          this.descendLeft()
        } else {
          break
        }
      } else {
        if (this.node.right) {
          this.descendRight()
        } else {
          break
        }
      }
    }

    if (compare(this.nodeOffset, offset) < 0) this.moveToSuccessor()
  }

  markLeft (markerId, startOffset, endOffset) {
    if (!isZero(this.nodeOffset) && compare(startOffset, this.leftAncestorOffset) <= 0 && compare(this.nodeOffset, endOffset) <= 0) {
      this.node.leftMarkerIds.add(markerId)
    }
  }

  markRight (markerId, startOffset, endOffset) {
    if (compare(this.leftAncestorOffset, startOffset) < 0 &&
      compare(startOffset, this.nodeOffset) <= 0 &&
      compare(this.rightAncestorOffset, endOffset) <= 0) {
      this.node.rightMarkerIds.add(markerId)
    }
  }

  ascend () {
    if (this.node.parent) {
      if (this.node.parent.left === this.node) {
        this.nodeOffset = this.rightAncestorOffset
      } else {
        this.nodeOffset = this.leftAncestorOffset
      }
      this.leftAncestorOffset = this.leftAncestorOffsetStack.pop()
      this.rightAncestorOffset = this.rightAncestorOffsetStack.pop()
      this.node = this.node.parent
    } else {
      this.node = null
      this.nodeOffset = null
      this.leftAncestorOffset = {row: 0, column: 0}
      this.rightAncestorOffset = {row: Infinity, column: Infinity}
    }
  }

  descendLeft () {
    this.leftAncestorOffsetStack.push(this.leftAncestorOffset)
    this.rightAncestorOffsetStack.push(this.rightAncestorOffset)

    this.rightAncestorOffset = this.nodeOffset
    this.node = this.node.left
    this.nodeOffset = traverse(this.leftAncestorOffset, this.node.leftExtent)
  }

  descendRight () {
    this.leftAncestorOffsetStack.push(this.leftAncestorOffset)
    this.rightAncestorOffsetStack.push(this.rightAncestorOffset)

    this.leftAncestorOffset = this.nodeOffset
    this.node = this.node.right
    this.nodeOffset = traverse(this.leftAncestorOffset, this.node.leftExtent)
  }

  moveToSuccessor () {
    if (!this.node) return

    if (this.node.right) {
      this.descendRight()
      while (this.node.left) {
        this.descendLeft()
      }
    } else {
      while (this.node.parent && this.node.parent.right === this.node) {
        this.ascend()
      }
      this.ascend()
    }
  }

  insertLeftChild (offset) {
    this.node.left = new Node(this.node, traversal(offset, this.leftAncestorOffset))
  }

  insertRightChild (offset) {
    this.node.right = new Node(this.node, traversal(offset, this.nodeOffset))
  }

  checkIntersection (start, end, resultSet) {
    if (compare(this.leftAncestorOffset, end) <= 0 && compare(start, this.nodeOffset) <= 0) {
      addToSet(resultSet, this.node.leftMarkerIds)
    }

    if (compare(start, this.nodeOffset) <= 0 && compare(this.nodeOffset, end) <= 0) {
      addToSet(resultSet, this.node.startMarkerIds)
      addToSet(resultSet, this.node.endMarkerIds)
    }

    if (compare(this.nodeOffset, end) <= 0 && compare(start, this.rightAncestorOffset) <= 0) {
      addToSet(resultSet, this.node.rightMarkerIds)
    }
  }
}
