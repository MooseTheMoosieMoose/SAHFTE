# Notes

## Z-Order Curves

Z-order curves are the spatial hashing technique I am trying first since they are very easy to implement, and produce essentially an octree for free. Z-Order curves are calculated by interleaving the bits of integer coordinates:

```
-> Let x, y, z be positive integer values less than 2^21 (2,097,152), represented as binary numbers, corresponding to their cartesian position, within the total bounding volume of the search space
-> The limits on x, y, z are from the maximum bit density of a uint64_t
-> The corresponding Z-order to this position is found by interleaving these values
-> Let x = 0b111 (7), let y = 0b000 (0), let z = 101 (5), the Z-order of this position is 0b101100101 (357)
-> The Z-order of a curve corresponds to the position in a depth first traversal of the corresponding octree created under these conditions