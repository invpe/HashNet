# HASHNET server
# https://github.com/invpe/HashNet/
import asyncio
import json
import time
import logging
import hashlib
import random
from collections import defaultdict

CHUNK_SIZE = 1_000_000
POOL_URL = "solo.ckpool.org"
POOL_PORT = 3333
BTC_ADDRESS = "bc1qe08ayxpntqhj0q0hs3j8jl9zd2gzd026quje3z"
ESP32_PORT = 5000
HEARBEAT_CHECK = 30
MINIMAL_DISTANCE_THRESHOLD = 5
VERSION = "1.2"
target = None
best_extranonce2 = 0
best_distance_to_target = 0  # in bits
mined_blocks = 0  # Count of mined blocks
server_start_time = time.time()
total_chunks = 0xFFFFFFFF // CHUNK_SIZE + 1
nonce_chunks = [(i * CHUNK_SIZE, min((i + 1) * CHUNK_SIZE, 0xFFFFFFFF + 1)) for i in range(total_chunks)]
completed_chunks = set()
connected_clients = {}
blocked_clients = defaultdict(lambda: 0)
client_stats = defaultdict(lambda: {"combinations_per_sec": 0, "last_seen": time.time(), "busy": False})
current_work = {}

def hash_ip_address(ip_address):
    """Hash an IP address using SHA-256."""
    return hashlib.sha256(ip_address.encode('utf-8')).hexdigest()


def generate_random_extranonce2():
    """Generate a random uint64_t value for extranonce2."""
    return f"{random.randint(0, 0xFFFFFFFFFFFFFFFF):016x}"

# Logging setup
logging.basicConfig(
    filename="mining_server.log",
    level=logging.INFO,
    format="%(asctime)s %(levelname)s: %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)

def log_and_print(message, level="info"):
    """Logs and prints a message to the console."""
    if hasattr(logging, level):
        getattr(logging, level)(message)
    else:
        logging.error(f"Invalid logging level: {level}. Message: {message}")
    print(message)

async def connect_to_pool():
    global extranonce1, extranonce2_size
    while True:
        try:
            reader, writer = await asyncio.open_connection(POOL_URL, POOL_PORT)

            # Send subscription and authorization messages
            await send_to_pool(writer, {"id": 1, "method": "mining.subscribe", "params": []})
            response = await get_pool_response(reader)
            extranonce1, extranonce2_size = response['result'][1], response['result'][2]

            await send_to_pool(writer, {"id": 2, "method": "mining.authorize", "params": [BTC_ADDRESS, "x"]})
            await get_pool_response(reader)

            # Start listening for jobs in a new task
            asyncio.create_task(listen_for_jobs(reader))
            break  # Exit the loop once connected
        except Exception as e:
            log_and_print(f"Failed to connect to pool: {e}. Retrying...", "error")
            await asyncio.sleep(5)  # Retry after 5 seconds

            # Reset the state on connection failure
            await handle_pool_disconnection()

async def listen_for_jobs(reader):
    """Listen and handle job notifications from the pool."""
    while True:
        try:
            line = await reader.readline()
            if not line:
                log_and_print("Pool connection closed unexpectedly.", "error")
                await handle_pool_disconnection()
                break

            data = json.loads(line.decode('utf-8'))
            if data.get("method") == "mining.notify":
                await handle_new_job(data["params"])
        except json.JSONDecodeError as e:
            log_and_print(f"JSON decode error: {e} - Received data: '{line}'", "error")
        except Exception as e:
            log_and_print(f"Unexpected error while listening for jobs: {e}", "error")
            await handle_pool_disconnection()
            break

async def handle_new_job(params):
    """Handles a new job received from the pool."""
    global current_work, best_extranonce2, best_distance_to_target,target
    job_keys = ['job_id', 'prevhash', 'coinb1', 'coinb2', 'merkle_branch', 'version', 'nbits', 'ntime']
    
    try:
        # Extract parameters up to the 'ntime'
        current_work = {key: params[i] for i, key in enumerate(job_keys)}
        current_work.update({'extranonce1': extranonce1, 'extranonce2_size': extranonce2_size})
        
        # Add clean_jobs flag if it exists
        clean_jobs = params[8] if len(params) > 8 else None
        current_work['clean_jobs'] = clean_jobs

        # Calculate target from nbits
        nbits = current_work['nbits']
        target = nbits_to_target(nbits) 
        

        # Log the reversed target in hex format
        log_and_print(f"Calculated target from nbits: {target.hex()}")

        # Log the job details including clean_jobs flag
        log_and_print(f"Received new job from pool:\n{json.dumps(current_work, indent=4)}") 

        # Handle clean_jobs flag
        if clean_jobs:
            log_and_print("Clean jobs flag detected. Resetting nonce chunks, best_extranonce2, and disconnecting busy clients.")
            
            reset_nonce_chunks()
            await disconnect_busy_clients()

            # Reset best_extranonce2 and best_distance_to_target
            best_extranonce2 = None
            best_distance_to_target = 0
            
    except IndexError as e:
        log_and_print(f"Index error while processing job parameters: {e}", "error")
    except Exception as e:
        log_and_print(f"Unexpected error in handle_new_job: {e}", "error")



def reset_nonce_chunks():
    """Resets the nonce chunks and clears completed chunks."""
    global nonce_chunks, completed_chunks
        
    nonce_chunks = [(i * CHUNK_SIZE, min((i + 1) * CHUNK_SIZE, 0xFFFFFFFF + 1)) for i in range(total_chunks)]
    completed_chunks.clear()
    log_and_print(f"[POOL] Cleaning jobs; resetting {len(nonce_chunks)} nonce chunks.")

async def disconnect_busy_clients():
    """Disconnects all busy clients."""
    to_disconnect = [writer for writer, stats in client_stats.items() if stats["busy"]]
    for writer in to_disconnect:
        log_and_print(f"[INFO] Disconnecting busy client {connected_clients.get(writer, 'Unknown')} due to clean_jobs.")
        await disconnect_client(writer)

async def distribute_work_continuously():
    """Continuously distributes work chunks to connected clients."""
    while True:
        if connected_clients and current_work:
            available_clients = [writer for writer in connected_clients if not client_stats[writer]["busy"] and nonce_chunks]
            for writer in available_clients:
                await assign_random_work_chunk(writer,best_extranonce2)
        await asyncio.sleep(1)

async def reset_miners_with_lucky_extranonce2(lucky_extranonce2):
    """Disconnect all miners and redistribute work with the lucky extranonce2."""
    await disconnect_all_clients()  # Disconnect all clients

    # Wait for a brief moment to ensure all clients are disconnected
    await asyncio.sleep(1)

    # Redistribute work with the lucky extranonce2
    for writer in connected_clients:
        await assign_random_work_chunk(writer, lucky_extranonce2)

async def assign_random_work_chunk(writer, extranonce2=None):
    """Assigns a random work chunk to a client, optionally with a specific extranonce2."""
    if not nonce_chunks:
        log_and_print(f"No more nonce chunks available for client {connected_clients.get(writer)}.", "warning")
        return
    
    chunk_index = random.randint(0, len(nonce_chunks) - 1)
    start_nonce, end_nonce = nonce_chunks.pop(chunk_index)

    if extranonce2 is None:
        extranonce2 = generate_random_extranonce2()  # Generate random extranonce2 if not provided
   
    work_message = {
        **current_work,
        'method': 'work',
        'nonce_start': start_nonce,
        'nonce_end': end_nonce,
        'merkle_branch': current_work['merkle_branch'],
        'extranonce2': extranonce2,
        'sversion': VERSION
    }
    client_stats[writer]["busy"] = True
    client_stats[writer]["extranonce2"] = extranonce2
    client_stats[writer]["start_nonce"] = start_nonce
    client_stats[writer]["end_nonce"] = end_nonce

    await send_to_client(writer, work_message)
    log_and_print(f"[INFO] Distributed work chunk {start_nonce}-{end_nonce} with extranonce2 {extranonce2} to {connected_clients[writer]}")


async def send_to_pool(writer, message):
    """Send a message to the pool."""
    try:
        writer.write(json.dumps(message).encode('utf-8') + b'\n')
        await writer.drain()
    except Exception as e:
        log_and_print(f"Failed to send message to pool: {e}", "error")

async def get_pool_response(reader):
    """Receive and parse a response from the pool."""
    try:
        response = await reader.readline()
        if not response:
            log_and_print("No response from pool.", "error")
            return None
        data = json.loads(response.decode('utf-8'))
        log_and_print(f"[POOL] Response: {data}")
        return data
    except json.JSONDecodeError as e:
        log_and_print(f"JSON decode error while reading pool response: {e}", "error")
    except Exception as e:
        log_and_print(f"Unexpected error while getting pool response: {e}", "error")
    return None

async def send_to_client(writer, message):
    """Send a message to a client."""
    try:
        writer.write(json.dumps(message).encode('utf-8') + b'\n')
        await writer.drain()
    except Exception as e:
        log_and_print(f"Failed to send message to client: {e}", "error")

async def disconnect_client(writer):
    """Disconnect a client by closing its connection."""
    addr = connected_clients.pop(writer, "Unknown client")
    client_stats.pop(writer, None)
    log_and_print(f"[INFO] Disconnecting client {addr}")
    try:
        writer.close()
        await writer.wait_closed()
    except Exception as e:
        log_and_print(f"Error while disconnecting client {addr}: {e}", "error")

async def handle_esp32_client(reader, writer):
    addr = writer.get_extra_info('peername') or "Unknown"
    if blocked_clients[addr] > time.time():
        log_and_print(f"[INFO] Blocked client {addr} attempted to connect. Disconnecting.")
        await disconnect_client(writer)
        return

    log_and_print(f"[INFO] New ESP32 connected: {addr}")
    connected_clients[writer] = addr

    try:
        await handle_client_messages(reader, writer)
    finally:
        await disconnect_client(writer)

async def handle_client_messages(reader, writer):
    addr = connected_clients.get(writer)
    try:
        while True:
            data = await reader.read(1024)
            if not data:
                break

            message = data.decode('utf-8').strip()
            if not message:
                continue

            # Attempt to decode the message
            try:
                data = json.loads(message)
                method = data.get('method')
                if method == 'found':
                    await submit_nonce_to_pool(writer, data)
                elif method == 'completed':
                    await mark_chunk_completed(writer, data)
                elif method == 'stats':
                    update_client_stats(writer, data)
                else:
                    log_and_print(f"Received unknown method '{method}' from client {addr}", "warning")
            except json.JSONDecodeError as e:
                log_and_print(f"JSON decode error: {e} - Received: '{message}'", "error")
                # Block the client for 5 minutes
                blocked_clients[addr] = time.time() + 5 * 60
                await disconnect_client(writer)
                break
    except ConnectionResetError:
        log_and_print(f"Connection reset by peer: {addr}", "warning")
    except Exception as e:
        log_and_print(f"Error handling client message: {e}", "error")
    finally:
        if writer in connected_clients:
            await disconnect_client(writer)


async def submit_nonce_to_pool(writer, data):
    """Submit a found nonce to the pool."""
    global mined_blocks  # Declare the global mined_blocks variable 
    nonce_data = {
        "params": [BTC_ADDRESS, data['job_id'], extranonce1, data['extranonce2'], current_work['ntime'], data['nonce']],
        "id": 3,
        "method": "mining.submit"
    }
    log_and_print(f"Submitting found nonce to pool: {nonce_data}")

    try:
        pool_reader, pool_writer = await asyncio.open_connection(POOL_URL, POOL_PORT)
        await send_to_pool(pool_writer, nonce_data)
        pool_response = await get_pool_response(pool_reader)
        await pool_writer.wait_closed()

        if pool_response and pool_response.get('result', False):
            log_and_print(f"[INFO] Nonce {data['nonce']} successfully mined by {connected_clients[writer]}")
            mined_blocks += 1  # Increment the global mined_blocks count
        else:
            log_and_print(f"[WARNING] Nonce {data['nonce']} rejected by the pool for client {connected_clients[writer]}")

        client_stats[writer]["busy"] = False
        
    except Exception as e:
        log_and_print(f"Failed to submit nonce to pool: {e}", "error")


async def mark_chunk_completed(writer, data):
    """Mark a nonce chunk as completed by a client."""    
    completed_chunks.add((data['nonce_start'], data['nonce_end']))
    client_stats[writer]["busy"] = False
    log_and_print(f"[NODE] {connected_clients[writer]} Completed chunk {data['nonce_start']} {data['nonce_end']}")


def update_client_stats(writer, data):
    global best_distance_to_target,best_extranonce2

    """Update the stats for a client."""
    client_stats[writer].update({
        "combinations_per_sec": data.get("combinations_per_sec", 0),
        "distance": data.get("distance", 0),
        "ident": data.get("ident", 0),
        "best_hash": data.get("besthash", 0),
        "version": data.get("version", 0),
        "last_seen": time.time()
    })
    
    # Track the closest distance to the target (higher value is better)
    client_distance = data.get("distance", 0)
    if client_distance > best_distance_to_target:
        best_distance_to_target = client_distance
        log_and_print(f"[INFO] New best distance to target: {best_distance_to_target} reported by client {data.get('ident', 'unknown')}")

        if best_distance_to_target >= MINIMAL_DISTANCE_THRESHOLD:
            best_extranonce2 = client_stats[writer].get('extranonce2', '0000000000000000')
            log_and_print(f"[INFO] Updating best extranonce2 to: {best_extranonce2}")
 
def nbits_to_target(nbits: str) -> bytes:
    """Convert the nbits value into the target hash."""
    nbits_value = int(nbits, 16)  # Convert nbits from hex to integer
    exponent = nbits_value >> 24  # Get the exponent (first byte)
    coefficient = nbits_value & 0xFFFFFF  # Get the coefficient (last three bytes)
    
    # Calculate the target: coefficient * 2^(8*(exponent-3))
    target = coefficient * (1 << (8 * (exponent - 3)))

    # Convert target to 256-bit hash (32 bytes) and return it as bytes
    target_bytes = target.to_bytes(32, byteorder='big')
    
    return target_bytes

async def monitor_client_stats():
    """Monitor and log client stats periodically."""
    while True:
        current_time = time.time()
        to_remove = [writer for writer, stats in client_stats.items() if current_time - stats["last_seen"] > HEARBEAT_CHECK]

        for writer in to_remove:
            log_and_print(f"[INFO] Client {connected_clients[writer]} is unresponsive. Removing...", "warning")
            await disconnect_client(writer)

        total_combinations_per_sec = sum(stats["combinations_per_sec"] for stats in client_stats.values())
        percentage_completed = (len(completed_chunks) / total_chunks) * 100 if total_chunks else 0
        log_and_print(f"[INFO] Connected nodes: {len(connected_clients)}, Total combinations/sec: {total_combinations_per_sec}, "
                      f"{percentage_completed:.2f}% completed, Best distance: {best_distance_to_target} {best_extranonce2}")

        save_stats_to_file()
        await asyncio.sleep(5)  # Adjusted to save every 5 seconds to reduce I/O overhead

def save_stats_to_file():
    """Save client stats to a JSON file."""
    nodes = []
    for writer, stats in client_stats.items():
        addr = connected_clients[writer]
        hashed_addr = hash_ip_address(addr[0])  # Hash the IP address here
        node_details = {
            "address": f"{hashed_addr}:{addr[1]}",  # Use the hashed IP in the stats
            "combinations_per_sec": stats["combinations_per_sec"],
            "last_seen": stats["last_seen"],
            "busy": stats["busy"],
            "half_shares": stats.get("half_shares", 0),
            "shares": stats.get("shares", 0),
            "best_distance": stats.get("distance", 0),
            "ident": stats.get("ident", ""),
            "extranonce2": stats.get("extranonce2", 0),
            "start_nonce": stats.get("start_nonce", 0),
            "end_nonce": stats.get("end_nonce", 0),
            "best_hash": stats.get("best_hash",0),
            "version": stats.get("version", "")
        }
        nodes.append(node_details)

    percentage_completed = (len(completed_chunks) / total_chunks) * 100 if total_chunks else 0
    total_chunks_completed = len(completed_chunks)

    target_hex = target.hex() if target else ""

    
    mining_stats = {
        "ChunksCompletionPercentage": percentage_completed,
        "TotalChunksCompleted": total_chunks_completed,
        "TotalMinedBlocks": mined_blocks,
        "BestDistance": best_distance_to_target,     
        "Target": target_hex,   
        "ServerStartTime": time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(server_start_time)),
        "CurrentTime": time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
    }

    stats_dict = {
        "Nodes": nodes,
        "Mining": [mining_stats],
    }

    with open("stats.json", "w") as stats_file:
        json.dump(stats_dict, stats_file, indent=4)



async def handle_pool_disconnection():
    """Handles the scenario when the pool connection is lost."""
    log_and_print("[POOL] Handling pool disconnection. Dropping all clients and resetting state...", "warning")

    # Disconnect all clients
    await disconnect_all_clients()

    # Reset nonce chunks and completed chunks
    reset_nonce_chunks()

    # Reconnect to the pool
    await connect_to_pool()

async def disconnect_all_clients():
    """Disconnects all connected clients."""
    for writer in list(connected_clients.keys()):
        await disconnect_client(writer)

async def main():
    """Main entry point for the server."""
    try:
        server = await asyncio.start_server(handle_esp32_client, '0.0.0.0', ESP32_PORT)
        log_and_print(f"TCP server started on port {ESP32_PORT}")

        asyncio.create_task(monitor_client_stats())
        asyncio.create_task(distribute_work_continuously())

        await connect_to_pool()  # Initial connection to the pool

        async with server:
            await server.serve_forever()
    except Exception as e:
        log_and_print(f"Error in main server loop: {e}", "critical")

# Run the server
asyncio.run(main())
