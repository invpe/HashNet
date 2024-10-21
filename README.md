![image](https://github.com/user-attachments/assets/13fb6942-b753-4638-9d8b-3013faff74d7)


# HashNet Xperiment

This project is a fun experiment where ESP32 nodes work together to try and solve a Bitcoin block by finding a valid nonce/extranonce2 value. Yes, it's basically the usual mining setup—but with a statistical twist!

## How It Works

- The server sends nonce chunks to each ESP32 from a predefined range.
- I randomize the extranonce2 for each node to start, allowing them to explore different parts of the search space.
- When a node finds a promising result (a hash with lots of leading zeros), we fix that extranonce2 and redistribute the workload, while still randomizing the nonce.

This strategy helps focus the search in areas where good results are more likely to be found.

Keep in mind, like any other mining setup, there’s no guarantee of earning rewards. Even though the nodes are working hard, solving Bitcoin blocks depends on luck and global competition, so rewards are never guaranteed (or even likely).

## Solar-powered setup

The plan is to run this project using an affordable solar setup. This allows the entire experiment to operate with zero electricity costs, fully powered by solar energy—making the mining efforts essentially invisible since there's nothing to pay for.

## Performance
Here's how the ESP32 nodes perform with different SHA256 implementations:

- Basic Implementation: ~16kh/sec per node (single thread)
- Double-Barrel Implementation: ~32kh/sec (dual core)
- NerdSHA256Plus: ~55kh/sec (dual core)

