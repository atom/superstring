import Node from './node'
import {addToSet} from './helpers'
import {compare, isZero, traversal, traverse} from './point-helpers'

export default class Iterator {
  constructor (markerIndex) {
    this.markerIndex = markerIndex
  }

  reset () {
    this.node = this.markerIndex.root
    this.nodePosition = this.node ? this.node.leftExtent : null
    this.leftAncestorPosition = {row: 0, column: 0}
    this.rightAncestorPosition = {row: Infinity, column: Infinity}
    this.leftAncestorPositionStack = []
    this.rightAncestorPositionStack = []
  }

  insertMarkerStart (markerId, startPosition, endPosition) {
    this.reset()

    if (!this.node) {
      let node = new Node(null, startPosition)
      this.markerIndex.root = node
      return node
    }

    while (true) {
      let comparison = compare(startPosition, this.nodePosition)
      if (comparison === 0) {
        this.markRight(markerId, startPosition, endPosition)
        return this.node
      } else if (comparison < 0) {
        this.markRight(markerId, startPosition, endPosition)
        if (this.node.left) {
          this.descendLeft()
        } else {
          this.insertLeftChild(startPosition)
          this.descendLeft()
          this.markRight(markerId, startPosition, endPosition)
          return this.node
        }
      } else { // startPosition > this.nodePosition
        if (this.node.right) {
          this.descendRight()
        } else {
          this.insertRightChild(startPosition)
          this.descendRight()
          this.markRight(markerId, startPosition, endPosition)
          return this.node
        }
      }
    }
  }

  insertMarkerEnd (markerId, startPosition, endPosition) {
    this.reset()

    if (!this.node) {
      let node = new Node(null, endPosition)
      this.markerIndex.root = node
      return node
    }

    while (true) {
      let comparison = compare(endPosition, this.nodePosition)
      if (comparison === 0) {
        this.markLeft(markerId, startPosition, endPosition)
        return this.node
      } else if (comparison < 0) {
        if (this.node.left) {
          this.descendLeft()
        } else {
          this.insertLeftChild(endPosition)
          this.descendLeft()
          this.markLeft(markerId, startPosition, endPosition)
          return this.node
        }
      } else { // endPosition > this.nodePosition
        this.markLeft(markerId, startPosition, endPosition)
        if (this.node.right) {
          this.descendRight()
        } else {
          this.insertRightChild(endPosition)
          this.descendRight()
          this.markLeft(markerId, startPosition, endPosition)
          return this.node
        }
      }
    }
  }

  insertSpliceBoundary (position, isInsertionEnd) {
    this.reset()

    while (true) {
      let comparison = compare(position, this.nodePosition)
      if (comparison === 0 && !isInsertionEnd) {
        return this.node
      } else if (comparison < 0) {
        if (this.node.left) {
          this.descendLeft()
        } else {
          this.insertLeftChild(position)
          return this.node.left
        }
      } else { // position > this.nodePosition
        if (this.node.right) {
          this.descendRight()
        } else {
          this.insertRightChild(position)
          return this.node.right
        }
      }
    }
  }

  findIntersecting (start, end, resultSet) {
    this.reset()
    if (!this.node) return

    while (true) {
      if (compare(start, this.nodePosition) < 0) {
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
    } while (this.node && compare(this.nodePosition, end) <= 0)
  }

  findContaining (position, resultSet) {
    this.reset()
    if (!this.node) return

    while (true) {
      this.checkIntersection(position, position, resultSet)

      if (compare(position, this.nodePosition) < 0) {
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
    while (this.node && compare(this.nodePosition, end) <= 0) {
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

    while (this.node && compare(this.nodePosition, end) <= 0) {
      addToSet(resultSet, this.node.startMarkerIds)
      this.moveToSuccessor()
    }
  }

  findEndingIn (start, end, resultSet) {
    this.reset()
    if (!this.node) return

    this.seekToFirstNodeGreaterThanOrEqualTo(start)

    while (this.node && compare(this.nodePosition, end) <= 0) {
      addToSet(resultSet, this.node.endMarkerIds)
      this.moveToSuccessor()
    }
  }

  dump () {
    this.reset()

    while (this.node && this.node.left) this.descendLeft()

    let snapshot = {}

    while (this.node) {
      this.node.startMarkerIds.forEach(markerId => {
        snapshot[markerId] = {start: this.nodePosition, end: null}
      })

      this.node.endMarkerIds.forEach(markerId => {
        snapshot[markerId].end = this.nodePosition
      })

      this.moveToSuccessor()
    }

    return snapshot
  }

  seekToFirstNodeGreaterThanOrEqualTo (position) {
    while (true) {
      let comparison = compare(position, this.nodePosition)

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

    if (compare(this.nodePosition, position) < 0) this.moveToSuccessor()
  }

  markLeft (markerId, startPosition, endPosition) {
    if (!isZero(this.nodePosition) && compare(startPosition, this.leftAncestorPosition) <= 0 && compare(this.nodePosition, endPosition) <= 0) {
      this.node.leftMarkerIds.add(markerId)
    }
  }

  markRight (markerId, startPosition, endPosition) {
    if (compare(this.leftAncestorPosition, startPosition) < 0 &&
      compare(startPosition, this.nodePosition) <= 0 &&
      compare(this.rightAncestorPosition, endPosition) <= 0) {
      this.node.rightMarkerIds.add(markerId)
    }
  }

  ascend () {
    if (this.node.parent) {
      if (this.node.parent.left === this.node) {
        this.nodePosition = this.rightAncestorPosition
      } else {
        this.nodePosition = this.leftAncestorPosition
      }
      this.leftAncestorPosition = this.leftAncestorPositionStack.pop()
      this.rightAncestorPosition = this.rightAncestorPositionStack.pop()
      this.node = this.node.parent
    } else {
      this.node = null
      this.nodePosition = null
      this.leftAncestorPosition = {row: 0, column: 0}
      this.rightAncestorPosition = {row: Infinity, column: Infinity}
    }
  }

  descendLeft () {
    this.leftAncestorPositionStack.push(this.leftAncestorPosition)
    this.rightAncestorPositionStack.push(this.rightAncestorPosition)

    this.rightAncestorPosition = this.nodePosition
    this.node = this.node.left
    this.nodePosition = traverse(this.leftAncestorPosition, this.node.leftExtent)
  }

  descendRight () {
    this.leftAncestorPositionStack.push(this.leftAncestorPosition)
    this.rightAncestorPositionStack.push(this.rightAncestorPosition)

    this.leftAncestorPosition = this.nodePosition
    this.node = this.node.right
    this.nodePosition = traverse(this.leftAncestorPosition, this.node.leftExtent)
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

  insertLeftChild (position) {
    this.node.left = new Node(this.node, traversal(position, this.leftAncestorPosition))
  }

  insertRightChild (position) {
    this.node.right = new Node(this.node, traversal(position, this.nodePosition))
  }

  checkIntersection (start, end, resultSet) {
    if (compare(this.leftAncestorPosition, end) <= 0 && compare(start, this.nodePosition) <= 0) {
      addToSet(resultSet, this.node.leftMarkerIds)
    }

    if (compare(start, this.nodePosition) <= 0 && compare(this.nodePosition, end) <= 0) {
      addToSet(resultSet, this.node.startMarkerIds)
      addToSet(resultSet, this.node.endMarkerIds)
    }

    if (compare(this.nodePosition, end) <= 0 && compare(start, this.rightAncestorPosition) <= 0) {
      addToSet(resultSet, this.node.rightMarkerIds)
    }
  }
}
