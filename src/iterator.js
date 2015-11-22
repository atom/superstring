'use babel'

import {ZERO_POINT, INFINITY_POINT, traverse} from './point-helpers'

const NOT_DONE = {done: false}
const DONE = {done: true}

export default class Iterator {
  constructor (tree) {
    this.tree = tree
    this.leftAncestorInputPosition = ZERO_POINT
    this.leftAncestorOutputPosition = ZERO_POINT
    this.leftAncestorInputPositionStack = []
    this.leftAncestorOutputPositionStack = []
    this.setCurrentNode(tree.root)
  }

  getInputStart () {
    return this.inputStart
  }

  getInputEnd () {
    return this.inputEnd
  }

  getOutputStart () {
    return this.outputStart
  }

  getOutputEnd () {
    return this.outputEnd
  }

  inChange () {
    return !!this.currentNode && !this.currentNode.isChangeStart
  }

  getChangeText () {
    return this.currentNode.changeText
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

  next () {
    if (!this.currentNode) {
      return DONE
    }

    if (this.currentNode.right) {
      this.descendRight()
      while (this.currentNode.left) {
        this.descendLeft()
      }
      return NOT_DONE
    } else {
      while (this.currentNode.parent && this.currentNode.parent.right === this.currentNode) {
        this.ascend()
      }
      if (this.currentNode.parent) {
        this.ascend()
        return NOT_DONE
      } else {
        return DONE
      }
    }
  }

  ascend () {
    this.leftAncestorInputPosition = this.leftAncestorInputPositionStack.pop()
    this.leftAncestorOutputPosition = this.leftAncestorOutputPositionStack.pop()
    this.setCurrentNode(this.currentNode.parent)
  }

  descendLeft () {
    this.leftAncestorInputPositionStack.push(this.leftAncestorInputPosition)
    this.leftAncestorOutputPositionStack.push(this.leftAncestorOutputPosition)
    this.setCurrentNode(this.currentNode.left)
  }

  descendRight () {
    this.leftAncestorInputPositionStack.push(this.leftAncestorInputPosition)
    this.leftAncestorOutputPositionStack.push(this.leftAncestorOutputPosition)
    this.leftAncestorInputPosition = traverse(this.leftAncestorInputPosition, this.currentNode.inputLeftExtent)
    this.leftAncestorOutputPosition = traverse(this.leftAncestorOutputPosition, this.currentNode.outputLeftExtent)
    this.setCurrentNode(this.currentNode.right)
  }
}
