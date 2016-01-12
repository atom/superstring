# atom-patch [![Build Status](https://travis-ci.org/atom/atom-patch.svg?branch=master)](https://travis-ci.org/atom/atom-patch)

This is a data structure will efficiently represents a transformation from input text to output text, allowing points in the input and output text to be bi-directionally mapped and the results of applying the patch to be visualized in real time as the patch is updated.

We plan to use it in Atom anywhere spatial or textual transformations need to be represented in an incremental way that can be interacted with, such as the mapping of 2d coordinates to 1d file offsets and the applications of transformations such as tab expansions, soft wraps, and folds.
