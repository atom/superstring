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
    this.leftAncestorStack = [null]
    this.leftAncestorInputPositionStack = [ZERO_POINT]
    this.leftAncestorOutputPositionStack = [ZERO_POINT]

    this.rightAncestor = null
    this.rightAncestorInputPosition = INFINITY_POINT
    this.rightAncestorOutputPosition = INFINITY_POINT
    this.rightAncestorStack = [null]
    this.rightAncestorInputPositionStack = [INFINITY_POINT]
    this.rightAncestorOutputPositionStack = [INFINITY_POINT]

    this.inputStart = ZERO_POINT
    this.outputStart = ZERO_POINT

    this.setCurrentNode(this.patch.root)
  }

  rewind () {
    this.reset()

    while (this.currentNode && this.currentNode.left) {
      this.descendLeft()
    }
  }

  getChanges () {
    this.rewind()

    let changes = []
    while (this.moveToSuccessor()) {
      let inChange = this.inChange()
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

  inChange () {
    return this.currentNode && !this.currentNode.isChangeStart
  }

  getInputStart () {
    return this.inputStart
  }

  getInputEnd () {
    return this.inputEnd
  }

  getInputExtent () {
    return traversalDistance(this.inputEnd, this.inputStart)
  }

  getOutputStart () {
    return this.outputStart
  }

  getOutputEnd () {
    return this.outputEnd
  }

  getOutputExtent () {
    return traversalDistance(this.outputEnd, this.outputStart)
  }

  getReplacementText () {
    return this.currentNode.changeText
  }

  seekToInputPosition (inputPosition) {
    this.reset()

    while (true) {
      if (comparePoints(inputPosition, this.inputEnd) < 0) {
        if (comparePoints(inputPosition, this.inputStart) >= 0) {
          return
        } else {
          if (!this.currentNode.left) throw new Error('Unexpected iterator state')
          this.descendLeft()
        }
      } else {
        this.descendRight()
      }
    }
  }

  seekToOutputPosition (outputPosition) {
    this.reset()

    while (true) {
      if (comparePoints(outputPosition, this.outputEnd) < 0) {
        if (comparePoints(outputPosition, this.outputStart) >= 0) {
          return
        } else {
          if (!this.currentNode.left) throw new Error('Unexpected iterator state')
          this.descendLeft()
        }
      } else {
        this.descendRight()
      }
    }
  }

  translateInputPosition (inputPosition) {
    if (comparePoints(inputPosition, this.inputStart) < 0 || comparePoints(inputPosition, this.inputEnd) > 0) {
      throw new Error('Point out of range')
    }

    let overshoot = traversalDistance(inputPosition, this.inputStart)
    return minPoint(traverse(this.outputStart, overshoot), this.outputEnd)
  }

  translateOutputPosition (outputPosition) {
    if (comparePoints(outputPosition, this.outputStart) < 0 || comparePoints(outputPosition, this.outputEnd) > 0) {
      throw new Error('Point out of range')
    }

    let overshoot = traversalDistance(outputPosition, this.outputStart)
    return minPoint(traverse(this.inputStart, overshoot), this.inputEnd)
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
      return this.buildInsertedSpliceBoundaryResult(this.patch.root, boundaryOutputPosition)
    }

    while (true) {
      this.inputEnd = traverse(this.leftAncestorInputPosition, this.currentNode.inputLeftExtent)
      this.outputEnd = traverse(this.leftAncestorOutputPosition, this.currentNode.outputLeftExtent)

      if (this.currentNode.isChangeStart) {
        if (comparePoints(boundaryOutputPosition, this.outputEnd) < 0) { // boundaryOutputPosition < this.outputEnd
          if (this.currentNode.left) {
            this.descendLeft()
          } else {
            return this.insertLeftChildDuringSplice(boundaryOutputPosition, insertingChangeStart)
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
              return this.insertRightChildDuringSplice(boundaryOutputPosition, insertingChangeStart)
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
              return this.insertLeftChildDuringSplice(boundaryOutputPosition, insertingChangeStart)
            }
          }
        } else { // boundaryOutputPosition > this.outputEnd
          if (this.currentNode.right) {
            this.descendRight()
          } else {
            return this.insertRightChildDuringSplice(boundaryOutputPosition, insertingChangeStart)
          }
        }
      }
    }
  }

  insertSpliceInputBoundary (inputPosition, insertingSpliceStart) {
    this.reset()

    if (!this.currentNode) {
      let node = new Node(null, inputPosition, inputPosition)
      node.isChangeStart = true
      this.patch.root = node
      return node
    }

    while (true) {
      if (comparePoints(inputPosition, this.inputEnd) < 0) {
        if (this.currentNode.left) {
          this.descendLeft()
        } else {
          return this.insertLeftChildDuringSpliceInput(inputPosition, insertingSpliceStart)
        }
      } else {
        if (this.currentNode.right) {
          this.descendRight()
        } else {
          return this.insertRightChildDuringSpliceInput(inputPosition, insertingSpliceStart)
        }
      }
    }
  }

  setCurrentNode (node) {
    this.currentNode = node

    if (node && node.left) {
      this.inputStart = traverse(this.leftAncestorInputPosition, node.left.inputExtent)
      this.outputStart = traverse(this.leftAncestorOutputPosition, node.left.outputExtent)
    } else {
      this.inputStart = this.leftAncestorInputPosition
      this.outputStart = this.leftAncestorOutputPosition
    }

    this.inputEnd = traverse(this.leftAncestorInputPosition, node ? node.inputLeftExtent : INFINITY_POINT)
    this.outputEnd = traverse(this.leftAncestorOutputPosition, node ? node.outputLeftExtent : INFINITY_POINT)
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
      let previousInputEnd = this.inputEnd
      let previousOutputEnd = this.outputEnd

      while (this.currentNode.parent && this.currentNode.parent.right === this.currentNode) {
        this.ascend()
      }
      this.ascend()

      if (!this.currentNode) { // advanced off right edge of tree
        this.inputStart = previousInputEnd
        this.outputStart = previousOutputEnd
        this.inputEnd = INFINITY_POINT
        this.outputEnd = INFINITY_POINT
      }
      return true
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

  insertLeftChildDuringSplice (outputPosition, isChangeStart) {
    let outputLeftExtent = traversalDistance(outputPosition, this.leftAncestorOutputPosition)
    let inputLeftExtent = minPoint(outputLeftExtent, this.currentNode.inputLeftExtent)
    let newNode = new Node(this.currentNode, inputLeftExtent, outputLeftExtent)
    newNode.isChangeStart = isChangeStart
    this.currentNode.left = newNode
    return this.buildInsertedSpliceBoundaryResult(newNode, outputPosition)
  }

  insertRightChildDuringSplice (outputPosition, isChangeStart) {
    let outputLeftExtent = traversalDistance(outputPosition, this.outputEnd)
    let inputLeftExtent = minPoint(outputLeftExtent, traversalDistance(this.rightAncestorInputPosition, this.inputEnd))
    let newNode = new Node(this.currentNode, inputLeftExtent, outputLeftExtent)
    newNode.isChangeStart = isChangeStart
    this.currentNode.right = newNode
    return this.buildInsertedSpliceBoundaryResult(newNode, outputPosition)
  }

  buildInsertedSpliceBoundaryResult (insertedNode, insertedNodeOutputPosition) {
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

  insertLeftChildDuringSpliceInput (inputPosition, insertingSpliceStart) {
    let inputLeftExtent = traversalDistance(inputPosition, this.leftAncestorInputPosition)
    let outputLeftExtent = minPoint(inputLeftExtent, this.currentNode.outputLeftExtent)
    let newNode = new Node(this.currentNode, inputLeftExtent, outputLeftExtent)
    this.currentNode.left = newNode
    this.descendLeft()
    this.initializeSpliceInputNode(newNode, insertingSpliceStart)
    return newNode
  }

  insertRightChildDuringSpliceInput (inputPosition, insertingSpliceStart) {
    let inputLeftExtent = traversalDistance(inputPosition, this.inputEnd)
    let outputLeftExtent = minPoint(inputLeftExtent, traversalDistance(this.rightAncestorOutputPosition, this.outputEnd))
    let newNode = new Node(this.currentNode, inputLeftExtent, outputLeftExtent)
    this.currentNode.right = newNode
    this.descendRight()
    this.initializeSpliceInputNode(newNode, insertingSpliceStart)
    return newNode
  }

  initializeSpliceInputNode (newNode, insertingSpliceStart) {
    if (insertingSpliceStart) {
      if (this.leftAncestor && this.leftAncestor.isChangeStart) {
        newNode.isChangeStart = false
        newNode.changeText = getPrefix(this.rightAncestor.changeText, traversalDistance(this.outputEnd, this.outputStart))
        this.rightAncestor.changeText = getSuffix(this.rightAncestor.changeText, traversalDistance(this.outputEnd, this.outputStart))
      } else {
        newNode.isChangeStart = true // this will cause the node to be deleted after the splice
      }
    } else { // inserting splice end
      if (this.rightAncestor && !this.rightAncestor.isChangeStart) {
        newNode.isChangeStart = true
        this.rightAncestor.changeText = getSuffix(this.rightAncestor.changeText, traversalDistance(this.outputEnd, this.outputStart))
      } else {
        newNode.isChangeStart = false // this will cause the node to be deleted after the splice
      }
    }
  }
}
