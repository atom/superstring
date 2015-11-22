import {format as formatPoint} from './point-helpers'

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
    this.changeText = null
  }

  toHTML() {
    let s = '<style>';
    s += 'table { width: 100%; }';
    s += 'td { width: 50%; text-align: center; border: 1px solid gray; white-space: nowrap; }';
    s += '</style>';

    s += '<table>';

    s += '<tr>';

    let changeStart = this.isChangeStart ? '&lt;&lt; ' : '';
    let changeEnd = !this.isChangeStart ? ' &gt;&gt;' : '';

    s += '<td colspan="2">' + changeStart + formatPoint(this.inputLeftExtent) + ' / ' + formatPoint(this.outputLeftExtent) + changeEnd + '</td>';
    s += '</tr>';

    if (this.left || this.right) {
      s += '<tr>';
      s += '<td>';
      if (this.left) {
        s += this.left.toHTML();
      } else {
        s += '&nbsp;';
      }
      s += '</td>';
      s += '<td>';
      if (this.right) {
        s += this.right.toHTML();
      } else {
        s += '&nbsp;';
      }
      s += '</td>';
      s += '</tr>';
    }

    s += '</table>';

    return s;
  }
}
