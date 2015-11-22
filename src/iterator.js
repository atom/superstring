'use babel'

import {ZERO_POINT, INFINITY_POINT, traverse} from './point-helpers'

const NOT_DONE = {done: false}
const DONE = {done: true}

export default class Iterator {
  constructor (tree, rewind) {
    this.tree = tree
    this.inputOffset = ZERO_POINT
    this.outputOffset = ZERO_POINT
    this.inputOffsetStack = []
    this.outputOffsetStack = []
    this.setNode(tree.root)

    if (rewind && this.node) {
      while (this.node.left) {
        this.descendLeft()
      }
    }
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
    return !!this.node && !this.node.isChangeStart
  }

  getChangeText () {
    return this.node.changeText
  }

  setNode (node) {
    this.node = node

    if (node) {
      if (node.left) {
        this.inputStart = traverse(this.inputOffset, node.left.inputExtent)
        this.outputStart = traverse(this.outputOffset, node.left.outputExtent)
      } else {
        this.inputStart = this.inputOffset
        this.outputStart = this.outputOffset
      }

      this.inputEnd = traverse(this.inputOffset, node.inputLeftExtent)
      this.outputEnd = traverse(this.outputOffset, node.outputLeftExtent)
    } else {
      this.inputStart = ZERO_POINT
      this.inputEnd = INFINITY_POINT
      this.outputStart = ZERO_POINT
      this.outputEnd = INFINITY_POINT
    }
  }

  next () {
    if (!this.node) {
      return DONE
    }

    if (this.node.right) {
      this.descendRight()
      while (this.node.left) {
        this.descendLeft()
      }
      return NOT_DONE
    } else {
      while (this.node.parent && this.node.parent.right === this.node) {
        this.ascend()
      }
      if (this.node.parent) {
        this.ascend()
        return NOT_DONE
      } else {
        return DONE
      }
    }
  }

  ascend () {
    this.inputOffset = this.inputOffsetStack.pop()
    this.outputOffset = this.outputOffsetStack.pop()
    this.setNode(this.node.parent)
  }

  descendLeft () {
    this.inputOffsetStack.push(this.inputOffset)
    this.outputOffsetStack.push(this.outputOffset)
    this.setNode(this.node.left)
  }

  descendRight () {
    this.inputOffsetStack.push(this.inputOffset)
    this.outputOffsetStack.push(this.outputOffset)
    this.inputOffset = traverse(this.inputOffset, this.node.inputLeftExtent)
    this.outputOffset = traverse(this.outputOffset, this.node.outputLeftExtent)
    this.setNode(this.node.right)
  }
}
