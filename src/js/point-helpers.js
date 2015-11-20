export function traverse (start, traversal) {
  if (traversal.row === 0) {
    return {
      row: start.row,
      column: start.column + traversal.column
    }
  } else {
    return {
      row: start.row + traversal.row,
      column: traversal.column
    }
  }
}

export function traversal (end, start) {
  if (end.row === start.row) {
    return {row: 0, column: end.column - start.column}
  } else {
    return {row: end.row - start.row, column: end.column}
  }
}

export function compare (a, b) {
  if (a.row < b.row) {
    return -1
  } else if (a.row > b.row) {
    return 1
  } else {
    if (a.column < b.column) {
      return -1
    } else if (a.column > b.column) {
      return 1
    } else {
      return 0
    }
  }
}

export function max (a, b) {
  if (compare(a, b) > 0) {
    return a
  } else {
    return b
  }
}

export function isZero (point) {
  return point.row === 0 && point.column === 0
}

export function format (point) {
  return `(${point.row}, ${point.column})`
}
