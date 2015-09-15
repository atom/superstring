import Random from 'random-seed'

const MAX_PRIORITY = 2147483647 // max 32 bit signed int (unboxed in v8)

export default class MarkerIndex {
  constructor (seed) {
    this.random = new Random(seed)
    this.root = null
    this.startNodesById = {}
    this.endNodesById = {}
    this.cursor = new Cursor(this)
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
    let startNode = this.cursor.insertStart(markerId, startOffset, endOffset)
    let endNode = this.cursor.insertEnd(markerId, startOffset, endOffset)

    // startNode.priority = this.random(MAX_PRIORITY)
    // this.bubbleNodeUp(startNode)
    // endNode.priority = this.random(MAX_PRIORITY)
    // this.bubbleNodeUp(endNode)

    this.startNodesById[markerId] = startNode
    this.endNodesById[markerId] = endNode
  }

  splice (start, oldExtent, newExtent) {

  }

  findIntersecting (start, end, resultSet) {
    this.cursor.findIntersecting(start, end, resultSet)
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

  toHTML () {
    let s = '<style>'
    s += 'table { width: 100%; }'
    s += 'td { width: 50%; text-align: center; border: 1px solid gray; white-space: nowrap; }'
    s += '</style>'
    s += this.root.toHTML()
    return s
  }
}

class Cursor {
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

  insertStart (markerId, startOffset, endOffset) {
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

  insertEnd (markerId, startOffset, endOffset) {
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

    return resultSet
  }

  markLeft (markerId, startOffset, endOffset) {
    if (startOffset <= this.leftAncestorOffset && this.nodeOffset <= endOffset) {
      this.node.leftMarkerIds.add(markerId)
    }
    this.markPoint(markerId, startOffset, endOffset)
  }

  markRight (markerId, startOffset, endOffset) {
    this.markPoint(markerId, startOffset, endOffset)
    if (startOffset <= this.nodeOffset && this.rightAncestorOffset <= endOffset) {
      this.node.rightMarkerIds.add(markerId)
    }
  }

  markPoint (markerId, startOffset, endOffset) {
    if (startOffset <= this.nodeOffset && this.nodeOffset <= endOffset) {
      this.node.pointMarkerIds.add(markerId)
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
      this.addToSet(resultSet, this.node.leftMarkerIds)
    }

    if (start <= this.nodeOffset && this.nodeOffset <= end) {
      this.addToSet(resultSet, this.node.pointMarkerIds)
    }

    if (this.nodeOffset <= end && start <= this.rightAncestorOffset) {
      this.addToSet(resultSet, this.node.rightMarkerIds)
    }
  }

  addToSet (target, source) {
    for (let id of source) target.add(id)
  }
}

class Node {
  constructor (parent, leftExtent) {
    this.parent = parent
    this.leftExtent = leftExtent
    this.leftMarkerIds = new Set
    this.rightMarkerIds = new Set
    this.pointMarkerIds = new Set
  }

  toHTML() {
    let s = ''
    s += '<table>'

    s += '<tr>'
    s += '<td colspan="2">'
    for (let id of this.leftMarkerIds) {
      s += id + ' '
    }
    s += '<< '
    s += this.leftExtent
    s += ' >>'
    for (let id of this.rightMarkerIds) {
      s += ' ' + id
    }
    s += '</td>'
    s += '</tr>'

    if (this.left || this.right) {
      s += '<tr>'
      s += '<td>'
      if (this.left) {
        s += this.left.toHTML()
      } else {
        s += '&nbsp;'
      }
      s += '</td>'
      s += '<td>'
      if (this.right) {
        s += this.right.toHTML()
      } else {
        s += '&nbsp;'
      }
      s += '</td>'
      s += '</tr>'
    }

    s += '</table>'

    return s
  }
}
