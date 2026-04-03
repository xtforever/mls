import socket
import time
import random
import string
import subprocess
import os
import sys

PORT = 8082
SERVER_EXE = "tests/memcached_server.exed"

def random_string(length):
    return ''.join(random.choice(string.printable) for _ in range(length))

def random_bytes(length):
    return bytes([random.randint(0, 255) for _ in range(length)])

def send_malformed_request(raw_data, chunked=False):
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(2)
            s.connect(('localhost', PORT))
            if chunked and len(raw_data) > 1:
                # Send data in small random chunks
                pos = 0
                while pos < len(raw_data):
                    chunk_size = random.randint(1, 10)
                    s.sendall(raw_data[pos:pos+chunk_size])
                    pos += chunk_size
                    time.sleep(0.001)
            else:
                s.sendall(raw_data)
            s.recv(1024)
    except (socket.timeout, ConnectionResetError, BrokenPipeError):
        pass
    except ConnectionRefusedError:
        print("\n[CRASH] Server connection refused! The server might have crashed.")
        return False
    return True

def generate_fuzz_cases():
    cases = []
    
    # Existing cases...
    cases.append(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
    cases.append(b"GET /" + b"A" * 10000 + b" HTTP/1.1\r\n\r\n")
    cases.append(b"POST /set HTTP/1.1\r\nContent-Length: 5000\r\n\r\n" + b"D" * 5000)
    
    # 15. Format string attacks in headers
    cases.append(b"GET / HTTP/1.1\r\nX-Bad: %s%s%s%s%n\r\n\r\n")
    
    # 16. Very long header list
    long_headers = b"GET / HTTP/1.1\r\n"
    for i in range(200):
        long_headers += f"X-Header-{i}: {i}\r\n".encode()
    long_headers += b"\r\n"
    cases.append(long_headers)
    
    # 17. Rapid fire small requests on same connection (if we supported keep-alive, but we close)
    # 18. Boundary cases for Content-Length
    cases.append(b"POST /set HTTP/1.1\r\nContent-Length: 0\r\n\r\n")
    cases.append(b"POST /set HTTP/1.1\r\nContent-Length: 1\r\n\r\n")
    
    # 19. Invalid base64 in body when X-Base64 is true
    cases.append(b"POST /set HTTP/1.1\r\nX-Key: k\r\nX-Base64: true\r\nContent-Length: 5\r\n\r\n!!!!!")

    # 20. "base64:" prefix with invalid data
    cases.append(b"POST /set HTTP/1.1\r\nX-Key: k\r\nContent-Length: 12\r\n\r\nbase64:!!!!")

    return cases

def run_fuzz():
    server_proc = subprocess.Popen([SERVER_EXE, str(PORT)], 
                                   stdout=subprocess.PIPE, 
                                   stderr=subprocess.PIPE)
    time.sleep(0.5)
    
    if server_proc.poll() is not None:
        print("Failed to start server")
        return

    print(f"Started aggressive fuzzing server (PID {server_proc.pid}) on port {PORT}")
    
    cases = generate_fuzz_cases()
    
    try:
        for i, case in enumerate(cases):
            chunked = (i % 2 == 0)
            print(f"Sending case {i+1}/{len(cases)} (chunked={chunked})... ", end="", flush=True)
            if not send_malformed_request(case, chunked=chunked):
                break
            print("Done")
            
        for i in range(200):
            if i % 10 == 0: print(f"Random fuzz {i+1}/200...")
            case = random_bytes(random.randint(1, 5000))
            if not send_malformed_request(case, chunked=(i % 5 == 0)):
                break

    except KeyboardInterrupt:
        print("\nFuzzing interrupted by user")
    finally:
        server_proc.terminate()
        stdout, stderr = server_proc.communicate()
        err_out = stderr.decode(errors='replace')
        if "AddressSanitizer" in err_out:
            print("\n[FAILURE] AddressSanitizer detected an issue!")
            print(err_out)
        elif server_proc.returncode != 0 and server_proc.returncode != -15:
            print(f"\n[FAILURE] Server exited with code {server_proc.returncode}")
            print(err_out)
        else:
            print("\n[SUCCESS] Aggressive fuzzing session completed without obvious crash.")

if __name__ == "__main__":
    run_fuzz()
