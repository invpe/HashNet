# HashNet Xperiment

![image](https://github.com/user-attachments/assets/9413b1f2-8232-41f3-a692-752d57f4993e)


The project is simply an experiment, where ESP32 nodes join up together solving a BTC block (hashing to find a valid nonce/extranonce2 value).
Yeah yeah, the usual mining thing...

However i approached it from a different (statistical) angle.

1. Server distributes `nonce` chunks to clients (ESP32) from a fixed range. 
2. I randomize the `extranonce2` for each client initially, allowing the miners to explore different areas of the search space. 
3. Once a client finds a good result (one with many leading zeros in the hash), we use that `extranonce2` as a base and redistribute work, randomizing the nonce but fixing the `extranonce2`. 

This strategy refines the search in promising regions.

## Performance

So here's how things look with different SHA256 implementations:

- Basic implementation does 16kh/sec per node (single thread)

- Double-barrel implementation ~32kh/sec (dual core)
```[INFO] Connected nodes: 4, Total combinations/sec: 126046, 0.44% completed, Best distance: 2 None```

- [NerdSHA256Plus](https://github.com/BitMaker-hub/NerdMiner_v2/tree/dev/src/ShaTests) does ~55kh/sec (dual core)

feel free to speed things up if you know how ;-)


