![image](https://github.com/user-attachments/assets/f0e342ad-a39e-4dc9-b08b-f358db93b864)

<div align="center">
<a href=https://github.com/invpe/HashNet/releases/tag/1.2><img src="https://badgen.net/static/>/Releases/34ebd5?scale=2?"></a><BR>
<a href=https://github.com/invpe/HashNet/issues><img src="https://badgen.net/static/>/Issues/8e44ad?scale=2?"></a>   
<a href=https://hashnet.amstaff.uk/f2d312928410f600aa7afaaf5d76ae60b2bd2504592bde42e127ab1d7d278301/><img src="https://badgen.net/static/>/Explorer/3498db?scale=2?"></a>
  <BR>
</div>


# HashNet


Welcome to the world of blockchains meets chaos science! 

A server playing matchmaker, splitting mining tasks from a solo mining pool into juicy “chunks” for miners. But here’s the twist—it’s not just mining; it’s a fitness contest. The server randomizes a thing called extranonce2, a funky little number miners fight over like treasure hunters sniffing for gold. Each miner tries to find the best “score”—a magical combo of zeroes in the block hash, getting them closer to the target value, calculated from nbits like some cryptographic carnival game.

What happens next? The server tracks the top-performing extranonce2 values like a blockchain talent scout, dishing out fresh chunks from the most promising search spaces. But wait—it’s not all work and no play. To avoid getting stuck in boring local optima (yawn!), the server sometimes shakes things up with random extranonce2 values, ensuring our miners stay on their toes.

Oh, and the miners? They’re not running souped-up rigs—nope. It’s an energy-saving showdown! Picture ESP32 microcontrollers and iPhones grinding hashes like tiny crypto superheroes, proving you don’t need a power-hungry beast to have some blockchain fun.

So, it’s not just an experiment; it’s a stats-meets-strategy battle royale.  
