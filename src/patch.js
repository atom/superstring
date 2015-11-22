import Random from 'random-seed'
import {ZERO_POINT, INFINITY_POINT, traverse, traversalDistance} from './point-helpers'
import {getExtent} from './text-helpers'
import Iterator from './iterator'

export default class Patch {
  constructor (seed = Date.now()) {
    this.randomGenerator = new Random(seed)
    this.root = null
    this.iterator = new Iterator(this)
  }

  spliceWithText (start, replacedExtent, replacementText) {
    this.splice(start, replacedExtent, getExtent(replacementText), {text: replacementText})
  }

  splice (outputStart, replacedExtent, replacementExtent, options) {
    let outputOldEnd = traverse(outputStart, replacedExtent)
    let outputNewEnd = traverse(outputStart, replacementExtent)

    let {startNode, prefix} = this.iterator.insertSpliceStart(outputStart)
    let {endNode, suffix, suffixExtent} = this.iterator.insertSpliceEnd(outputOldEnd)
    startNode.priority = -1
    this.bubbleNodeUp(startNode)
    endNode.priority = -2
    this.bubbleNodeUp(endNode)

    startNode.right = null
    startNode.inputExtent = startNode.inputLeftExtent
    startNode.outputExtent = startNode.outputLeftExtent

    let endNodeOutputRightExtent = traversalDistance(endNode.outputExtent, endNode.outputLeftExtent)
    endNode.outputLeftExtent = traverse(outputNewEnd, suffixExtent)
    endNode.outputExtent = traverse(endNode.outputLeftExtent, endNodeOutputRightExtent)
    endNode.changeText = prefix + options.text + suffix

    startNode.priority = this.generateRandom()
    this.bubbleNodeDown(startNode)
    endNode.priority = this.generateRandom()
    this.bubbleNodeDown(endNode)
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

    if (root.parent) {
      if (root === root.parent.left) {
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

    pivot.inputLeftExtent = traverse(root.inputLeftExtent, pivot.inputLeftExtent)
    pivot.inputExtent = traverse(pivot.inputLeftExtent, (pivot.right ? pivot.right.inputExtent : ZERO_POINT))
    root.inputExtent = traverse(root.inputLeftExtent, (root.right ? root.right.inputExtent : ZERO_POINT))

    pivot.outputLeftExtent = traverse(root.outputLeftExtent, pivot.outputLeftExtent)
    pivot.outputExtent = traverse(pivot.outputLeftExtent, (pivot.right ? pivot.right.outputExtent : ZERO_POINT))
    root.outputExtent = traverse(root.outputLeftExtent, (root.right ? root.right.outputExtent : ZERO_POINT))
  }

  rotateNodeRight (pivot) {
    let root = pivot.parent

    if (root.parent) {
      if (root === root.parent.left) {
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

    root.inputLeftExtent = traversalDistance(root.inputLeftExtent, pivot.inputLeftExtent)
    root.inputExtent = traversalDistance(root.inputExtent, pivot.inputLeftExtent)
    pivot.inputExtent = traverse(pivot.inputLeftExtent, root.inputExtent)

    root.outputLeftExtent = traversalDistance(root.outputLeftExtent, pivot.outputLeftExtent)
    root.outputExtent = traversalDistance(root.outputExtent, pivot.outputLeftExtent)
    pivot.outputExtent = traverse(pivot.outputLeftExtent, root.outputExtent)
  }

  generateRandom () {
    return this.randomGenerator.random()
  }

  getChanges () {
    return this.iterator.getChanges()
  }
}
