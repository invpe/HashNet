<img width="921" alt="image" src="https://github.com/user-attachments/assets/15075bea-f4f9-415f-b586-73c74e686475">

# HashNet

Welcome to the world of blockchain meets chaos science! Here’s the scoop:

We’ve got a server playing matchmaker, splitting mining tasks from a solo mining pool into juicy “chunks” for miners. But here’s the twist—it’s not just mining; it’s a fitness contest. The server randomizes a thing called extranonce2, a funky little number miners fight over like treasure hunters sniffing for gold. Each miner tries to find the best “score”—a magical combo of zeroes in the block hash, getting them closer to the target value, calculated from nbits like some cryptographic carnival game.

What happens next? The server tracks the top-performing extranonce2 values like a blockchain talent scout, dishing out fresh chunks from the most promising search spaces. But wait—it’s not all work and no play. To avoid getting stuck in boring local optima (yawn!), the server sometimes shakes things up with random extranonce2 values, ensuring our miners stay on their toes.

Oh, and the miners? They’re not running souped-up rigs—nope. It’s an energy-saving showdown! Picture ESP32 microcontrollers and iPhones grinding hashes like tiny crypto superheroes, proving you don’t need a power-hungry beast to have some blockchain fun.

So, it’s not just an experiment; it’s a stats-meets-strategy battle royale.  

![image](https://github.com/user-attachments/assets/843f0a21-4fb6-4be0-8443-e09d9de35adc)

