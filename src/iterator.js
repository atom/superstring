'use babel'

import {ZERO_POINT, INFINITY_POINT, traverse, traversalDistance, compare as comparePoints, min as minPoint} from './point-helpers'
import {getPrefix, getSuffix} from './text-helpers'
import Node from './node'

export default class Iterator {
  constructor (patch) {
    this.patch = patch
  }

  reset () {
    this.leftAncestorInputPosition = ZERO_POINT
    this.leftAncestorOutputPosition = ZERO_POINT
    this.leftAncestorInputPositionStack = []
    this.leftAncestorOutputPositionStack = []
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
    let node = this.currentNode
    if (!node) {
      this.patch.root = new Node(null, boundaryOutputPosition, boundaryOutputPosition)
      this.patch.root.isChangeStart = insertingChangeStart
      return buildInsertedNodeResult(this.patch.root)
    }

    let inputOffset = ZERO_POINT
    let outputOffset = ZERO_POINT
    let maxInputPosition = INFINITY_POINT
    let nodeInputPosition, nodeOutputPosition
    let containingStartNode, containingEndNode
    let containingStartNodeOutputPosition, containingEndNodeOutputPosition

    while (true) {
      nodeInputPosition = traverse(inputOffset, node.inputLeftExtent)
      nodeOutputPosition = traverse(outputOffset, node.outputLeftExtent)

      if (node.isChangeStart) {
        let result = visitChangeStart()
        if (result) return result
      } else {
        let result = visitChangeEnd()
        if (result) return result
      }
    }

    function visitChangeStart() {
      if (comparePoints(boundaryOutputPosition, nodeOutputPosition) < 0) { // boundaryOutputPosition < nodeOutputPosition
        containingEndNode = null
        containingEndNodeOutputPosition = null

        if (node.left) {
          descendLeft()
          return null
        } else {
          return insertLeftNode()
        }
      } else { // boundaryOutputPosition >= nodeOutputPosition
        containingStartNode = node
        containingStartNodeOutputPosition = nodeOutputPosition

        if (node.right) {
          descendRight()
          return null
        } else {
          if (insertingChangeStart || containingEndNode) {
            return {
              startNode: containingStartNode,
              endNode: containingEndNode,
              startNodeOutputPosition: containingStartNodeOutputPosition,
              endNodeOutputPosition: containingEndNodeOutputPosition
            }
          } else {
            return insertRightNode()
          }
        }
      }
    }

    function visitChangeEnd () {
      if (comparePoints(boundaryOutputPosition, nodeOutputPosition) <= 0) { // boundaryOutputPosition <= nodeOutputPosition
        containingEndNode = node
        containingEndNodeOutputPosition = nodeOutputPosition

        if (node.left) {
          descendLeft()
          return null
        } else {
          if (!insertingChangeStart || containingStartNode) {
            return {
              startNode: containingStartNode,
              endNode: containingEndNode,
              startNodeOutputPosition: containingStartNodeOutputPosition,
              endNodeOutputPosition: containingEndNodeOutputPosition
            }
          } else {
            return insertLeftNode()
          }
        }
      } else { // boundaryOutputPosition > nodeOutputPosition
        containingStartNode = null
        containingStartNodeOutputPosition = null

        if (node.right) {
          descendRight()
          return null
        } else {
          return insertRightNode()
        }
      }
    }

    function descendLeft () {
      maxInputPosition = nodeInputPosition
      node = node.left
    }

    function descendRight () {
      inputOffset = traverse(inputOffset, node.inputLeftExtent)
      outputOffset = traverse(outputOffset, node.outputLeftExtent)
      node = node.right
    }

    function insertLeftNode () {
      let outputLeftExtent = traversalDistance(boundaryOutputPosition, outputOffset)
      let inputLeftExtent = minPoint(outputLeftExtent, node.inputLeftExtent)
      let newNode = new Node(node, inputLeftExtent, outputLeftExtent)
      newNode.isChangeStart = insertingChangeStart
      node.left = newNode
      return buildInsertedNodeResult(newNode)
    }

    function insertRightNode () {
      let outputLeftExtent = traversalDistance(boundaryOutputPosition, nodeOutputPosition)
      let inputLeftExtent = minPoint(outputLeftExtent, traversalDistance(maxInputPosition, nodeInputPosition))
      let newNode = new Node(node, inputLeftExtent, outputLeftExtent)
      newNode.isChangeStart = insertingChangeStart
      node.right = newNode
      return buildInsertedNodeResult(newNode)
    }

    function buildInsertedNodeResult (insertedNode) {
      if (insertingChangeStart) {
        return {
          startNode: insertedNode,
          startNodeOutputPosition: boundaryOutputPosition,
          endNode: containingEndNode,
          endNodeOutputPosition: containingEndNodeOutputPosition
        }
      } else {
        return {
          startNode: containingStartNode,
          startNodeOutputPosition: containingStartNodeOutputPosition,
          endNode: insertedNode,
          endNodeOutputPosition: boundaryOutputPosition
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
    this.leftAncestorInputPosition = this.inputEnd
    this.leftAncestorOutputPosition = this.outputEnd
    this.setCurrentNode(this.currentNode.right)
  }
}
