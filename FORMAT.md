# HDRC1

One ASCII line of magic, then one line per block.

```
HDRC1
<header80hex> <tx0hex> [tx1hex ...]
```

Header is 80 bytes, little-endian, Bitcoin layout:

| offset | size | field |
|---|---|---|
| 0 | 4 | version |
| 4 | 32 | prev_hash (zero in genesis) |
| 36 | 32 | merkle root |
| 68 | 4 | unix time |
| 72 | 4 | compact `bits` |
| 76 | 4 | nonce |

`pow_hash = SHA-256(SHA-256(header))`. The 32-byte digest is treated as a little-endian uint256 and must be `<=` the target decoded from `bits` (Bitcoin compact: mantissa × 256^(exp-3)).

Merkle: `sha256d(tx)` per leaf; odd level duplicates the last hash; parent is `sha256d(left||right)`.

`verify` checks: genesis prev is 32 zero bytes, each next prev equals previous pow_hash, merkle matches txs, PoW holds.
