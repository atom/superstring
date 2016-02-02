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
        let change = {
          start: this.outputStart,
          oldExtent: traversalDistance(this.inputEnd, this.inputStart),
          newExtent: traversalDistance(this.outputEnd, this.outputStart),
        }
        if (this.currentNode.newText != null) {
          change.newText = this.currentNode.newText
        }
        changes.push(change)
      }
    }

    return changes
  }

  getCurrentNode () {
    return this.currentNode
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

  getMetadata () {
    return this.currentNode ? this.currentNode.metadata : null
  }

  seekToInputPosition (inputPosition) {
    this.reset()

    if (!this.currentNode) return

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

    if (!this.currentNode) return

    while (true) {
      if (comparePoints(outputPosition, this.outputEnd) < 0) {
        if (comparePoints(outputPosition, this.outputStart) >= 0) {
          return
        } else {
          if (!this.currentNode.left) throw new Error('Unexpected iterator state')
          this.descendLeft()
        }
      } else {
        if (this.currentNode) {
          this.descendRight()
        } else {
          if (comparePoints(outputPosition, INFINITY_POINT) !== 0) throw new Error('Unexpected iterator state')
          return
        }
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

  insertSpliceBoundary (boundaryOutputPosition, spliceStartNode) {
    this.reset()

    let insertingStart = (spliceStartNode == null)

    if (!this.currentNode) {
      this.patch.root = new Node(null, boundaryOutputPosition, boundaryOutputPosition)
      return this.patch.root
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
          break
        }
      } else if (comparison === 0 && this.currentNode !== spliceStartNode) {
        return this.currentNode
      } else { // comparison > 0
        if (this.currentNode.right) {
          this.descendRight()
        } else {
          let outputLeftExtent = traversalDistance(boundaryOutputPosition, this.outputEnd)
          let inputLeftExtent = minPoint(outputLeftExtent, traversalDistance(this.rightAncestorInputPosition, this.inputEnd))
          let newNode = new Node(this.currentNode, inputLeftExtent, outputLeftExtent)
          this.currentNode.right = newNode
          this.descendRight()
          break
        }
      }
    }

    if (this.rightAncestor && this.rightAncestor.isChangeEnd) {
      this.currentNode.isChangeStart = true
      this.currentNode.isChangeEnd = true
      let {newText} = this.rightAncestor
      if (newText != null) {
        let boundaryIndex = characterIndexForPoint(newText, traversalDistance(boundaryOutputPosition, this.leftAncestorOutputPosition))
        if (insertingStart) this.currentNode.newText = newText.substring(0, boundaryIndex)
        this.rightAncestor.newText = newText.substring(boundaryIndex)
      }
    }

    return this.currentNode
  }

  insertSpliceInputBoundary (boundaryInputPosition, insertingStart, isInsertion) {
    this.reset()

    if (!this.currentNode) {
      this.patch.root = new Node(null, boundaryInputPosition, boundaryInputPosition)
      return this.patch.root
    }

    while (true) {
      this.inputEnd = traverse(this.leftAncestorInputPosition, this.currentNode.inputLeftExtent)
      this.outputEnd = traverse(this.leftAncestorOutputPosition, this.currentNode.outputLeftExtent)

      let comparison = comparePoints(boundaryInputPosition, this.inputEnd)
      if (comparison < 0) {
        if (this.currentNode.left) {
          this.descendLeft()
        } else {
          if ((insertingStart || !isInsertion) && this.leftAncestor && comparePoints(boundaryInputPosition, this.leftAncestorInputPosition) === 0) {
            return this.leftAncestor
          }

          let inputLeftExtent = traversalDistance(boundaryInputPosition, this.leftAncestorInputPosition)
          let outputLeftExtent = minPoint(inputLeftExtent, this.currentNode.outputLeftExtent)
          let newNode = new Node(this.currentNode, inputLeftExtent, outputLeftExtent)
          this.currentNode.left = newNode
          this.descendLeft()
          break
        }
      } else {
        if (this.currentNode.right) {
          this.descendRight()
        } else {
          if (insertingStart || !isInsertion) {
            if (comparePoints(boundaryInputPosition, this.inputEnd) === 0) {
              return this.currentNode
            }

            if (this.leftAncestor && comparePoints(boundaryInputPosition, this.leftAncestorInputPosition) === 0) {
              return this.leftAncestor
            }
          }

          let inputLeftExtent = traversalDistance(boundaryInputPosition, this.inputEnd)
          let outputLeftExtent = minPoint(inputLeftExtent, traversalDistance(this.rightAncestorOutputPosition, this.outputEnd))
          let newNode = new Node(this.currentNode, inputLeftExtent, outputLeftExtent)
          this.currentNode.right = newNode
          this.descendRight()
          break
        }
      }
    }

    if (this.rightAncestor && this.rightAncestor.isChangeEnd) {
      this.currentNode.isChangeStart = !insertingStart
      this.currentNode.isChangeEnd = insertingStart
      let {newText} = this.rightAncestor
      if (newText != null) {
        let boundaryIndex = characterIndexForPoint(newText, traversalDistance(this.outputEnd, this.leftAncestorOutputPosition))
        if (insertingStart) this.currentNode.newText = newText.substring(0, boundaryIndex)
        this.rightAncestor.newText = newText.substring(boundaryIndex)
      }
    }

    return this.currentNode
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
}
