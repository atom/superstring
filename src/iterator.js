import Node from './node'
import {addToSet} from './helpers'

export default class Iterator {
  constructor (markerIndex) {
    this.markerIndex = markerIndex
  }

  reset () {
    this.node = this.markerIndex.root
    this.nodeOffset = this.node ? this.node.leftExtent : null
    this.leftAncestorOffset = 0
    this.rightAncestorOffset = Infinity
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
      if (startOffset === this.nodeOffset) {
        this.markRight(markerId, startOffset, endOffset)
        return this.node
      } else if (startOffset < this.nodeOffset) {
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
      if (endOffset === this.nodeOffset) {
        this.markLeft(markerId, startOffset, endOffset)
        return this.node
      } else if (endOffset < this.nodeOffset) {
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
        if (offset === this.nodeOffset && !isInsertionEnd) {
          return this.node
        } else if (offset < this.nodeOffset) {
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
      if (start < this.nodeOffset) {
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
    } while (this.node && this.nodeOffset <= end)
  }

  findContaining (offset, resultSet) {
    this.reset()
    if (!this.node) return

    while (true) {
      this.checkIntersection(offset, offset, resultSet)

      if (offset < this.nodeOffset) {
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

    this.seekToLeastUpperBound(start - 1)

    let started = new Set()
    while (this.node && this.nodeOffset <= end) {
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

    this.seekToLeastUpperBound(start - 1)

    while (this.node && this.nodeOffset <= end) {
      addToSet(resultSet, this.node.startMarkerIds)
      this.moveToSuccessor()
    }
  }

  findEndingIn (start, end, resultSet) {
    this.reset()
    if (!this.node) return

    this.seekToLeastUpperBound(start - 1)

    while (this.node && this.nodeOffset <= end) {
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

  seekToLeastUpperBound (offset) {
    while (true) {
      if (offset === this.nodeOffset) {
        break
      } else if (offset < this.nodeOffset) {
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

    if (this.nodeOffset <= offset) this.moveToSuccessor()
  }

  markLeft (markerId, startOffset, endOffset) {
    if (this.nodeOffset > 0 && startOffset <= this.leftAncestorOffset && this.nodeOffset <= endOffset) {
      this.node.leftMarkerIds.add(markerId)
    }
  }

  markRight (markerId, startOffset, endOffset) {
    if (this.leftAncestorOffset < startOffset &&
      startOffset <= this.nodeOffset &&
      this.rightAncestorOffset <= endOffset) {
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
      this.nodeOffset = this.leftAncestorOffset + this.node.leftExtent
    } else {
      this.node = null
      this.nodeOffset = null
      this.leftAncestorOffset = 0
      this.rightAncestorOffset = Infinity
    }
  }

  descendLeft () {
    this.leftAncestorOffsetStack.push(this.leftAncestorOffset)
    this.rightAncestorOffsetStack.push(this.rightAncestorOffset)

    this.rightAncestorOffset = this.nodeOffset
    this.node = this.node.left
    this.nodeOffset = this.leftAncestorOffset + this.node.leftExtent
  }

  descendRight () {
    this.leftAncestorOffsetStack.push(this.leftAncestorOffset)
    this.rightAncestorOffsetStack.push(this.rightAncestorOffset)

    this.leftAncestorOffset = this.nodeOffset
    this.node = this.node.right
    this.nodeOffset = this.leftAncestorOffset + this.node.leftExtent
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
    this.node.left = new Node(this.node, offset - this.leftAncestorOffset)
  }

  insertRightChild (offset) {
    this.node.right = new Node(this.node, offset - this.nodeOffset)
  }

  checkIntersection (start, end, resultSet) {
    if (this.leftAncestorOffset <= end && start <= this.nodeOffset) {
      addToSet(resultSet, this.node.leftMarkerIds)
    }

    if (start <= this.nodeOffset && this.nodeOffset <= end) {
      addToSet(resultSet, this.node.startMarkerIds)
    }

    if (start <= this.nodeOffset && this.nodeOffset <= end) {
      addToSet(resultSet, this.node.endMarkerIds)
    }

    if (this.nodeOffset <= end && start <= this.rightAncestorOffset) {
      addToSet(resultSet, this.node.rightMarkerIds)
    }
  }
}
