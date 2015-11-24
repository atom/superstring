import {ZERO_POINT, traverse, format as formatPoint} from '../../src/point-helpers'
import Patch from '../../src/patch'
import Node from '../../src/node'

Patch.prototype.toHTML = function () {
  if (this.root) {
    return this.root.toHTML()
  } else {
    return ''
  }
}

Node.prototype.toHTML = function (leftAncestorInputPosition = ZERO_POINT, leftAncestorOutputPosition = ZERO_POINT) {
  let s = '<style>'
  s += 'table { width: 100%; }'
  s += 'td { width: 50%; text-align: center; border: 1px solid gray; white-space: nowrap; }'
  s += '</style>'

  s += '<table>'

  s += '<tr>'
  let changeStart = this.isChangeStart ? '&gt;&gt; ' : ''
  let changeEnd = !this.isChangeStart ? ' &lt;&lt;' : ''
  let inputPosition = traverse(leftAncestorInputPosition, this.inputLeftExtent)
  let outputPosition = traverse(leftAncestorOutputPosition, this.outputLeftExtent)
  s += '<td colspan="2">' + changeEnd + formatPoint(inputPosition) + ' / ' + formatPoint(outputPosition) + ' {' + JSON.stringify(this.changeText)  + '} ' + changeStart + '</td>'
  s += '</tr>'

  if (this.left || this.right) {
    s += '<tr>'
    s += '<td>'
    if (this.left) {
      s += this.left.toHTML(leftAncestorInputPosition, leftAncestorOutputPosition)
    } else {
      s += '&nbsp;'
    }
    s += '</td>'
    s += '<td>'
    if (this.right) {
      s += this.right.toHTML(inputPosition, outputPosition)
    } else {
      s += '&nbsp;'
    }
    s += '</td>'
    s += '</tr>'
  }

  s += '</table>'

  return s
}
