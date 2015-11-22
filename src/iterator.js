import {ZERO_POINT, INFINITY_POINT, traverse, traversalDistance, compare as comparePoints, min as minPoint} from './point-helpers'
import {getPrefix, getSuffix} from './text-helpers'
import Node from './node'

export default class Iterator {
  constructor (patch) {
    this.patch = patch
  }

  reset () {
    this.leftAncestor = null
    this.leftAncestorInputPosition = ZERO_POINT
    this.leftAncestorOutputPosition = ZERO_POINT
    this.leftAncestorStack = []
    this.leftAncestorInputPositionStack = []
    this.leftAncestorOutputPositionStack = []

    this.rightAncestor = null
    this.rightAncestorInputPosition = INFINITY_POINT
    this.rightAncestorOutputPosition = INFINITY_POINT
    this.rightAncestorStack = []
    this.rightAncestorInputPositionStack = []
    this.rightAncestorOutputPositionStack = []

    this.setCurrentNode(this.patch.root)
  }

  getChanges () {
    this.reset()

    while (this.currentNode && this.currentNode.left) {
      this.descendLeft()
    }

    let changes = []
    while (this.moveToSuccessor()) {
      let inChange = !this.currentNode.isChangeStart
      if (inChange) {
        changes.push({
          start: this.outputStart,
          replacedExtent: traversalDistance(this.inputEnd, this.inputStart),
          replacementText: this.currentNode.changeText
        })
      }
    }

    return changes
  }

  insertSpliceStart (spliceOutputStart) {
    this.reset()

    let {startNode, endNode, startNodeOutputPosition} = this.insertSpliceBoundary(spliceOutputStart, true)

    let prefix
    if (comparePoints(spliceOutputStart, startNodeOutputPosition) === 0) {
      prefix = ''
    } else {
      prefix = getPrefix(endNode.changeText, traversalDistance(spliceOutputStart, startNodeOutputPosition))
    }

    return {startNode, prefix}
  }

  insertSpliceEnd (spliceOutputEnd) {
    this.reset()

    let {startNode, endNode, startNodeOutputPosition, endNodeOutputPosition} = this.insertSpliceBoundary(spliceOutputEnd, false)

    let suffix, suffixExtent
    if (comparePoints(spliceOutputEnd, endNodeOutputPosition) === 0) {
      suffix = ''
      suffixExtent = ZERO_POINT
    } else {
      suffix = getSuffix(endNode.changeText, traversalDistance(spliceOutputEnd, startNodeOutputPosition))
      suffixExtent = traversalDistance(endNodeOutputPosition, spliceOutputEnd)
    }

    return {endNode, suffix, suffixExtent}
  }

  insertSpliceBoundary (boundaryOutputPosition, insertingChangeStart) {
    if (!this.currentNode) {
      this.patch.root = new Node(null, boundaryOutputPosition, boundaryOutputPosition)
      this.patch.root.isChangeStart = insertingChangeStart
      return this.buildInsertedNodeResult(this.patch.root, boundaryOutputPosition)
    }

    while (true) {
      this.inputEnd = traverse(this.leftAncestorInputPosition, this.currentNode.inputLeftExtent)
      this.outputEnd = traverse(this.leftAncestorOutputPosition, this.currentNode.outputLeftExtent)

      if (this.currentNode.isChangeStart) {
        if (comparePoints(boundaryOutputPosition, this.outputEnd) < 0) { // boundaryOutputPosition < this.outputEnd
          if (this.currentNode.left) {
            this.descendLeft()
          } else {
            return this.insertLeftNode(boundaryOutputPosition, insertingChangeStart)
          }
        } else { // boundaryOutputPosition >= this.outputEnd
          if (this.currentNode.right) {
            this.descendRight()
          } else {
            if (insertingChangeStart || this.rightAncestorIsChangeEnd()) {
              return {
                startNode: this.currentNode,
                endNode: this.rightAncestorIsChangeEnd() ? this.rightAncestor : null,
                startNodeOutputPosition: this.outputEnd,
                endNodeOutputPosition: this.rightAncestorIsChangeEnd() ? this.rightAncestorOutputPosition : null
              }
            } else {
              return this.insertRightNode(boundaryOutputPosition, insertingChangeStart)
            }
          }
        }
      } else { // !this.currentNode.isChangeStart (it's a change end)
        if (comparePoints(boundaryOutputPosition, this.outputEnd) <= 0) { // boundaryOutputPosition <= this.outputEnd
          if (this.currentNode.left) {
            this.descendLeft()
          } else {
            if (!insertingChangeStart || this.leftAncestorIsChangeStart()) {
              return {
                startNode: this.leftAncestorIsChangeStart() ? this.leftAncestor : null,
                endNode: this.currentNode,
                startNodeOutputPosition: this.leftAncestorIsChangeStart() ? this.leftAncestorOutputPosition : null,
                endNodeOutputPosition: this.outputEnd
              }
            } else {
              return this.insertLeftNode(boundaryOutputPosition, insertingChangeStart)
            }
          }
        } else { // boundaryOutputPosition > this.outputEnd
          if (this.currentNode.right) {
            this.descendRight()
          } else {
            return this.insertRightNode(boundaryOutputPosition, insertingChangeStart)
          }
        }
      }
    }
  }

  setCurrentNode (node) {
    this.currentNode = node

    if (node) {
      if (node.left) {
        this.inputStart = traverse(this.leftAncestorInputPosition, node.left.inputExtent)
        this.outputStart = traverse(this.leftAncestorOutputPosition, node.left.outputExtent)
      } else {
        this.inputStart = this.leftAncestorInputPosition
        this.outputStart = this.leftAncestorOutputPosition
      }

      this.inputEnd = traverse(this.leftAncestorInputPosition, node.inputLeftExtent)
      this.outputEnd = traverse(this.leftAncestorOutputPosition, node.outputLeftExtent)
    } else {
      this.inputStart = ZERO_POINT
      this.inputEnd = INFINITY_POINT
      this.outputStart = ZERO_POINT
      this.outputEnd = INFINITY_POINT
    }
  }

  moveToSuccessor () {
    if (!this.currentNode) return false

    if (this.currentNode.right) {
      this.descendRight()
      while (this.currentNode.left) {
        this.descendLeft()
      }
      return true
    } else {
      while (this.currentNode.parent && this.currentNode.parent.right === this.currentNode) {
        this.ascend()
      }
      if (this.currentNode.parent) {
        this.ascend()
        return true
      } else {
        return false
      }
    }
  }

  ascend () {
    this.leftAncestor = this.leftAncestorStack.pop()
    this.leftAncestorInputPosition = this.leftAncestorInputPositionStack.pop()
    this.leftAncestorOutputPosition = this.leftAncestorOutputPositionStack.pop()
    this.rightAncestor = this.rightAncestorStack.pop()
    this.rightAncestorInputPosition = this.rightAncestorInputPositionStack.pop()
    this.rightAncestorOutputPosition = this.rightAncestorOutputPositionStack.pop()
    this.setCurrentNode(this.currentNode.parent)
  }

  descendLeft () {
    this.pushToAncestorStacks()
    this.rightAncestor = this.currentNode
    this.rightAncestorInputPosition = this.inputEnd
    this.rightAncestorOutputPosition = this.outputEnd
    this.setCurrentNode(this.currentNode.left)
  }

  descendRight () {
    this.pushToAncestorStacks()
    this.leftAncestor = this.currentNode
    this.leftAncestorInputPosition = this.inputEnd
    this.leftAncestorOutputPosition = this.outputEnd
    this.setCurrentNode(this.currentNode.right)
  }

  pushToAncestorStacks () {
    this.leftAncestorStack.push(this.leftAncestor)
    this.leftAncestorInputPositionStack.push(this.leftAncestorInputPosition)
    this.leftAncestorOutputPositionStack.push(this.leftAncestorOutputPosition)
    this.rightAncestorStack.push(this.rightAncestor)
    this.rightAncestorInputPositionStack.push(this.rightAncestorInputPosition)
    this.rightAncestorOutputPositionStack.push(this.rightAncestorOutputPosition)
  }

  leftAncestorIsChangeStart () {
    return this.leftAncestor && this.leftAncestor.isChangeStart
  }

  rightAncestorIsChangeEnd () {
    return this.rightAncestor && !this.rightAncestor.isChangeStart
  }

  insertLeftNode (outputPosition, isChangeStart) {
    let outputLeftExtent = traversalDistance(outputPosition, this.leftAncestorOutputPosition)
    let inputLeftExtent = minPoint(outputLeftExtent, this.currentNode.inputLeftExtent)
    let newNode = new Node(this.currentNode, inputLeftExtent, outputLeftExtent)
    newNode.isChangeStart = isChangeStart
    this.currentNode.left = newNode
    return this.buildInsertedNodeResult(newNode, outputPosition)
  }

  insertRightNode (outputPosition, isChangeStart) {
    let outputLeftExtent = traversalDistance(outputPosition, this.outputEnd)
    let inputLeftExtent = minPoint(outputLeftExtent, traversalDistance(this.rightAncestorInputPosition, this.inputEnd))
    let newNode = new Node(this.currentNode, inputLeftExtent, outputLeftExtent)
    newNode.isChangeStart = isChangeStart
    this.currentNode.right = newNode
    return this.buildInsertedNodeResult(newNode, outputPosition)
  }

  buildInsertedNodeResult (insertedNode, insertedNodeOutputPosition) {
    if (insertedNode.isChangeStart) {
      return {
        startNode: insertedNode,
        startNodeOutputPosition: insertedNodeOutputPosition,
        endNode: this.rightAncestorIsChangeEnd() ? this.rightAncestor : null,
        endNodeOutputPosition: this.rightAncestorIsChangeEnd() ? this.rightAncestorOutputPosition : null
      }
    } else {
      return {
        startNode: this.leftAncestorIsChangeStart() ? this.leftAncestor : null,
        startNodeOutputPosition: this.leftAncestorIsChangeStart() ? this.leftAncestorOutputPosition : null,
        endNode: insertedNode,
        endNodeOutputPosition: insertedNodeOutputPosition
      }
    }
  }
}
