# HashNet Xperiment

The project is simply an experiment, where ESP32 nodes join up together solving a BTC block (hashing to find a valid nonce/extranonce2 value).
Yeah yeah, the usual mining thing...

However i approached it from a different (statistical) angle.

1. Server distributes `nonce` chunks to clients (ESP32) from a fixed range. 
2. I randomize the `extranonce2` for each client initially, allowing the miners to explore different areas of the search space. 
3. Once a client finds a good result (one with many leading zeros in the hash), we use that `extranonce2` as a base and redistribute work, randomizing the nonce but fixing the `extranonce2`. 

This strategy refines the search in promising regions.

## Performance

Current implementation does 16kh/sec per node (single thread), the double-core version makes ~32kh/sec:

```[INFO] Connected nodes: 4, Total combinations/sec: 126046, 0.44% completed, Best distance: 2 None```

feel free to speed things up if you know how ;-)


