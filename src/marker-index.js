import Random from 'random-seed'

const MAX_PRIORITY = 2147483647 // max 32 bit signed int (unboxed in v8)

export default class MarkerIndex {
  constructor (seed) {
    this.random = new Random(seed)
    this.root = new Node(null, 0)
    this.root.priority = -MAX_PRIORITY
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

    startNode.startMarkerIds.add(markerId)
    endNode.endMarkerIds.add(markerId)

    if (startNode !== this.root) {
      startNode.priority = this.random(MAX_PRIORITY)
      this.bubbleNodeUp(startNode)
    }

    if (endNode !== this.root) {
      endNode.priority = this.random(MAX_PRIORITY)
      this.bubbleNodeUp(endNode)
    }

    this.startNodesById[markerId] = startNode
    this.endNodesById[markerId] = endNode
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

    if (startNode !== this.root && (startNode.startMarkerIds.size + startNode.endMarkerIds.size) === 0) {
      this.deleteNode(startNode)
    }

    if (endNode !== this.root && endNode !== startNode && (endNode.startMarkerIds.size + endNode.endMarkerIds.size) === 0) {
      this.deleteNode(endNode)
    }

    delete this.startNodesById[markerId]
    delete this.endNodesById[markerId]
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

    if (root === this.root) {
      throw new Error('Rotating root node downward is not allowed')
    }

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

    for (let markerId of root.rightMarkerIds) {
      pivot.rightMarkerIds.add(markerId)
    }

    for (let markerId of pivot.leftMarkerIds) {
      if (root.leftMarkerIds.has(markerId)) {
        root.leftMarkerIds.delete(markerId)
      } else {
        pivot.leftMarkerIds.delete(markerId)
        root.rightMarkerIds.add(markerId)
      }
    }
  }

  rotateNodeRight(pivot) {
    let root = pivot.parent

    if (root === this.root) {
      throw new Error('Rotating root node downward is not allowed')
    }

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
      pivot.leftMarkerIds.add(markerId)
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

  toHTML () {
    if (!this.root) return ''

    let s = '<style>'
    s += 'table { width: 100%; }'
    s += 'td { width: 50%; text-align: center; border: 1px solid gray; white-space: nowrap; }'
    s += '</style>'
    s += this.root.toHTML(0)
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
    if (this.nodeOffset > 0 && startOffset <= this.leftAncestorOffset && this.nodeOffset <= endOffset) {
      this.node.leftMarkerIds.add(markerId)
    }
  }

  markRight (markerId, startOffset, endOffset) {
    if (startOffset <= this.nodeOffset && this.rightAncestorOffset <= endOffset) {
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
      this.addToSet(resultSet, this.node.leftMarkerIds)
    }

    if (start <= this.nodeOffset && this.nodeOffset <= end) {
      this.addToSet(resultSet, this.node.startMarkerIds)
    }

    if (start <= this.nodeOffset && this.nodeOffset <= end) {
      this.addToSet(resultSet, this.node.endMarkerIds)
    }

    if (this.nodeOffset <= end && start <= this.rightAncestorOffset) {
      this.addToSet(resultSet, this.node.rightMarkerIds)
    }
  }

  addToSet (target, source) {
    for (let id of source) target.add(id)
  }
}

let idCounter = 1

class Node {
  constructor (parent, leftExtent) {
    this.parent = parent
    this.left = null
    this.right = null
    this.leftExtent = leftExtent
    this.leftMarkerIds = new Set
    this.rightMarkerIds = new Set
    this.startMarkerIds = new Set
    this.endMarkerIds = new Set
    this.priority = null
    this.id = idCounter++
  }

  toHTML (leftAncestorOffset) {
    let offset = leftAncestorOffset + this.leftExtent

    let s = ''
    s += '<table>'

    s += '<tr>'
    s += '<td colspan="2">'
    for (let id of this.leftMarkerIds) {
      s += id + ' '
    }
    s += '<< '
    for (let id of this.endMarkerIds) {
      s += id + ' '
    }
    s += '(( '
    s += offset
    s += ' ))'
    for (let id of this.startMarkerIds) {
      s += ' ' + id
    }
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
        s += this.left.toHTML(leftAncestorOffset)
      } else {
        s += '&nbsp;'
      }
      s += '</td>'
      s += '<td>'
      if (this.right) {
        s += this.right.toHTML(offset)
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
