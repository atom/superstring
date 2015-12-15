let idCounter = 0

export default class Node {
  constructor(parent, inputLeftExtent, outputLeftExtent) {
    this.parent = parent
    this.left = null
    this.right = null
    this.inputLeftExtent = inputLeftExtent
    this.outputLeftExtent = outputLeftExtent
    this.inputExtent = inputLeftExtent
    this.outputExtent = outputLeftExtent

    this.id = ++idCounter
    this.priority = Infinity
    this.isChangeStart = false
    this.isChangeEnd = false
    this.changeText = null
  }
}
