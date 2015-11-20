import MarkerIndex from '../../src/js/marker-index'
import Node from '../../src/js/node'
import {traverse, format as formatPoint} from '../../src/js/point-helpers'

MarkerIndex.prototype.toHTML = function () {
  if (!this.root) return ''

  let s = '<style>'
  s += 'table { width: 100%; }'
  s += 'td { width: 50%; text-align: center; border: 1px solid gray; white-space: nowrap; }'
  s += '</style>'
  s += this.root.toHTML({row: 0, column: 0})
  return s
}

Node.prototype.toHTML = function (leftAncestorPosition) {
  let position = traverse(leftAncestorPosition, this.leftExtent)

  let s = ''
  s += '<table>'

  s += '<tr>'
  s += '<td colspan="2">'
  for (let id of this.leftMarkerIds) {
    s += id + ' '
  }
  s += '[ '
  for (let id of this.endMarkerIds) {
    s += id + ' '
  }
  s += formatPoint(position)
  for (let id of this.startMarkerIds) {
    s += ' ' + id
  }
  s += ' ]'
  for (let id of this.rightMarkerIds) {
    s += ' ' + id
  }
  s += '</td>'
  s += '</tr>'

  if (this.left || this.right) {
    s += '<tr>'
    s += '<td>'
    if (this.left) {
      s += this.left.toHTML(leftAncestorPosition)
    } else {
      s += '&nbsp;'
    }
    s += '</td>'
    s += '<td>'
    if (this.right) {
      s += this.right.toHTML(position)
    } else {
      s += '&nbsp;'
    }
    s += '</td>'
    s += '</tr>'
  }

  s += '</table>'

  return s
}
