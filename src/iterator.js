import {ZERO_POINT, INFINITY_POINT, traverse, traversalDistance, compare as comparePoints, min as minPoint, isZero as isZeroPoint} from './point-helpers'
import {getPrefix, getSuffix, characterIndexForPoint} from './text-helpers'
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
    this.leftAncestorInputPositionStack = [ZERO_POINT]
    this.leftAncestorOutputPositionStack = [ZERO_POINT]

    this.rightAncestor = null
    this.rightAncestorInputPosition = INFINITY_POINT
    this.rightAncestorOutputPosition = INFINITY_POINT
    this.rightAncestorStack = []
    this.rightAncestorInputPositionStack = [INFINITY_POINT]
    this.rightAncestorOutputPositionStack = [INFINITY_POINT]

    this.inputStart = ZERO_POINT
    this.outputStart = ZERO_POINT

    this.setCurrentNode(this.patch.root)
  }

  moveToBeginning () {
    this.reset()

    while (this.currentNode && this.currentNode.left) {
      this.descendLeft()
    }
  }

  moveToEnd () {
    this.reset()

    while (this.currentNode && this.currentNode.right) {
      this.descendRight()
    }
  }

  getChanges () {
    this.moveToBeginning()

    let changes = []
    while (this.moveToSuccessor()) {
      if (!this.inChange()) continue

      let change = {
        oldStart: this.inputStart,
        newStart: this.outputStart,
        oldExtent: traversalDistance(this.inputEnd, this.inputStart),
        newExtent: traversalDistance(this.outputEnd, this.outputStart),
      }
      if (this.currentNode.newText != null) change.newText = this.currentNode.newText
      if (this.currentNode.oldText != null) change.oldText = this.currentNode.oldText

      changes.push(change)
    }

    return changes
  }

  inChange () {
    return this.currentNode && this.currentNode.isChangeEnd
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

  getNewText () {
    return this.currentNode.newText
  }

  getOldText () {
    return this.currentNode.oldText
  }

  insertSpliceBoundary (boundaryOutputPosition, spliceStartNode) {
    this.reset()

    let insertingStart = (spliceStartNode == null)

    if (!this.currentNode) {
      this.patch.root = new Node(null, boundaryOutputPosition, boundaryOutputPosition)
      this.patch.nodesCount++
      return {node: this.patch.root}
    }

    while (true) {
      this.inputEnd = traverse(this.leftAncestorInputPosition, this.currentNode.inputLeftExtent)
      this.outputEnd = traverse(this.leftAncestorOutputPosition, this.currentNode.outputLeftExtent)

      let comparison = comparePoints(boundaryOutputPosition, this.outputEnd)
      if (comparison < 0) {
        if (this.currentNode.left) {
          this.descendLeft()
        } else {
          let outputLeftExtent = traversalDistance(boundaryOutputPosition, this.leftAncestorOutputPosition)
          let inputLeftExtent = minPoint(outputLeftExtent, this.currentNode.inputLeftExtent)
          let newNode = new Node(this.currentNode, inputLeftExtent, outputLeftExtent)
          this.currentNode.left = newNode
          this.descendLeft()
          this.patch.nodesCount++
          break
        }
      } else if (comparison === 0 && this.currentNode !== spliceStartNode) {
        return {node: this.currentNode}
      } else { // comparison > 0
        if (this.currentNode.right) {
          this.descendRight()
        } else {
          let outputLeftExtent = traversalDistance(boundaryOutputPosition, this.outputEnd)
          let inputLeftExtent = minPoint(outputLeftExtent, traversalDistance(this.rightAncestorInputPosition, this.inputEnd))
          let newNode = new Node(this.currentNode, inputLeftExtent, outputLeftExtent)
          this.currentNode.right = newNode
          this.descendRight()
          this.patch.nodesCount++
          break
        }
      }
    }

    let intersectsChange = false
    if (this.rightAncestor && this.rightAncestor.isChangeEnd) {
      this.currentNode.isChangeStart = true
      this.currentNode.isChangeEnd = true
      let {newText, oldText} = this.rightAncestor
      if (newText != null) {
        let boundaryIndex = characterIndexForPoint(newText, traversalDistance(boundaryOutputPosition, this.leftAncestorOutputPosition))
        if (insertingStart) this.currentNode.newText = newText.substring(0, boundaryIndex)
        this.rightAncestor.newText = newText.substring(boundaryIndex)
      }
      if (oldText != null) {
        let boundaryIndex = characterIndexForPoint(oldText, traversalDistance(boundaryOutputPosition, this.leftAncestorOutputPosition))
        if (insertingStart) {
          this.currentNode.oldText = oldText.substring(0, boundaryIndex)
          this.rightAncestor.oldText = oldText.substring(boundaryIndex)
        }
        intersectsChange = true
      }
    }

    return {node: this.currentNode, intersectsChange}
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

  moveToPredecessor () {
    if (!this.currentNode) return false

    if (this.currentNode.left) {
      this.descendLeft()
      while (this.currentNode.right) {
        this.descendRight()
      }
      return true
    } else {
      while (this.currentNode.parent && this.currentNode.parent.left === this.currentNode) {
        this.ascend()
      }
      this.ascend()

      return this.currentNode != null
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
}
