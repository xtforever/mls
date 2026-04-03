import requests
import time
import subprocess

PORT = 8083
URL = f"http://localhost:{PORT}"

def test_lru():
    # Start server with small memory limit (already 1MB in C code)
    server_proc = subprocess.Popen(["tests/memcached_server.exed", str(PORT)],
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE,
                                   text=True)
    time.sleep(0.5)
    
    try:
        # 1. Fill memory
        # Each entry: Key (~10 bytes) + Value (100k bytes) = ~100KB
        # 11 entries should trigger eviction (1.1MB > 1MB)
        print("Filling memory to trigger LRU eviction...")
        for i in range(15):
            key = f"key_{i:03d}"
            value = "X" * (100 * 1024) # 100KB
            headers = {"X-Key": key}
            try:
                r = requests.post(f"{URL}/set", data=value, headers=headers, timeout=5)
                print(f"Set {key}: {r.status_code}")
            except Exception as e:
                print(f"Set {key}: FAILED ({e})")
            time.sleep(0.1)

        # 2. Check memory stats
        try:
            r = requests.get(f"{URL}/memory", timeout=5)
            print("\nServer Memory Stats:")
            print(r.text)
        except Exception as e:
            print(f"\nMemory stats request failed: {e}")

        # 3. Verify eviction of early keys
        print("\nVerifying evictions...")
        for i in range(5):
            key = f"key_{i:03d}"
            try:
                r = requests.get(f"{URL}/get", headers={"X-Key": key}, timeout=10)
                print(f"Get {key}: {r.status_code} (Expected 404 for evicted keys)")
            except Exception as e:
                print(f"Get {key}: FAILED ({e})")

        # 4. Verify latest keys are still there
        print("\nVerifying recent keys...")
        for i in range(10, 15):
            key = f"key_{i:03d}"
            try:
                r = requests.get(f"{URL}/get", headers={"X-Key": key}, timeout=10)
                print(f"Get {key}: {r.status_code} (Expected 200)")
            except Exception as e:
                print(f"Get {key}: FAILED ({e})")

    finally:
        server_proc.terminate()
        stdout, stderr = server_proc.communicate()
        print("\n--- Server Log (LRU messages) ---")
        for line in stdout.splitlines():
            if "[LRU]" in line or "[DEBUG]" in line:
                print(line)
        if stderr:
            print("\n--- Server Errors ---")
            print(stderr)

if __name__ == "__main__":
    test_lru()
