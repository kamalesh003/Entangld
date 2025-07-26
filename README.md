# Entangld
# **EntangId Documentation**  
*A Quantum-Inspired Distributed Qubit Simulation Framework*  

---

## **1. Overview**  
**EntangId** is a C++ library that simulates quantum computing primitives (qubits, gates, entanglement) using shared memory for distributed coordination. It provides:  
- **Qubit state management** with complex amplitudes  
- **Entanglement** between up to 4 nodes  
- **Automatic decoherence** with timeout-based collapse  
- **Thread-safe operations** via mutex locks  

**Key Innovation**: Uses POSIX shared memory (`shm_open`, `mmap`) to enable quantum-like behavior across processes/machines.  

---

## **2. Core Components**  

### **2.1 QubitState Structure**  
```cpp  
struct QubitState {  
    double alpha_real, alpha_imag;  // |0> amplitude  
    double beta_real, beta_imag;    // |1> amplitude  
    uint8_t measured;               // 0, 1, or 2 (superposition)  
    char links[4][64];              // Names of entangled peers  
    uint32_t link_count;            // Active links  
    uint32_t task_id;               // Owner ID  
    uint64_t created_at;            // Timestamp (ms)  
    uint64_t decohere_timeout_ms;   // Auto-collapse deadline  
};  
```  
**Purpose**: Stores the quantum state in shared memory for cross-process access.  

---

### **2.2 Qubit Class**  
#### **Key Methods**  
| Method | Description |  
|--------|-------------|  
| `initSuperposition()` | Sets qubit to (|0> + |1>)/√2 |  
| `measure()` | Collapses state to |0> or |1> (probabilistic) |  
| `applyGate(gate)` | Applies H/X/Z gate (thread-safe) |  
| `entangle(peers)` | Links qubits for measurement propagation |  
| `setState(ar, ai, br, bi)` | Custom amplitudes |  

#### **Private Mechanisms**  
- **Shared Memory**: Managed via `shm_open`/`mmap`  
- **Decoherence Thread**: Periodically checks for timeout-based collapse  
- **Mutex Locks**: Ensure atomic operations (`std::mutex`)  

---

## **3. Entanglement & GHZ States**  
### **3.1 formGHZGroup(qubits)**  
Creates a Greenberger-Horne-Zeilinger (GHZ) state among 2–5 qubits:  
```cpp  
std::vector<Qubit*> qubits = {&q1, &q2, &q3};  
formGHZGroup(qubits); // All qubits entangled  
```  
**Behavior**:  
1. Each qubit links to all others (`entangle()`)  
2. Sets uniform superposition state (`setState()`)  

**Measurement Propagation**:  
- Calling `measure()` on one qubit collapses all entangled peers via `propagateToLinks()`.  

---

## **4. Decoherence Model**  
### **4.1 Timeout-Based Collapse**  
```cpp  
Qubit q("qubit", 1, 5000); // 5s timeout  
q.initSuperposition();  
```  
**Rules**:  
- If `measure()` isn’t called within `decohere_timeout_ms`, the background thread forces a random collapse.  
- Propagates to linked qubits (simulating quantum decoherence).  

---

## **5. Use Cases**  
### **5.1 Financial Settlements**  
**Problem**: Slow atomic commits in cross-border payments.  
**Solution**:  
```cpp  
QuantumSettlement tx({ "bankA", "bankB", "clearinghouse" });  
if (tx.executeTransfer(1e6, 0.9e6)) { // $1M ↔ €0.9M  
    updateLedgers(); // All-or-nothing commit  
}  
```  

### **5.2 Military Command Systems**  
**Problem**: Secure launch authorization.  
**Solution**:  
```cpp  
Qubit president("nuke_cmd", 0xDEFCON);  
Qubit silo1("silo_alpha", 0xDEFCON);  
formGHZGroup({&president, &silo1});  
if (president.measure() == silo1.getMeasurement()) {  
    launch_missiles(); // Consensus verified  
}  
```  

---

## **6. Technical Constraints**  
1. **Max 4 Entangled Peers**: Due to `links[4][64]` array size.  
2. **Shared Memory Requirements**:  
   - Requires `-lrt` on Linux.  
   - Permissions: `0666` (read/write for all).  
3. **Thread Safety**: All operations use `std::lock_guard`.  

---

## **7. Build & Test**  
### **7.1 Compilation**  
```bash  
!g++ -std=c++11 -pthread -o qubit_test qubit.cpp
```  

### **7.2 Test Suite**  
| Test | Description |  
|------|-------------|  
| `test_single_qubit()` | Validates gates/measurement |  
| `test_bell_state()` | 2-qubit entanglement |  
| `test_ghz_state()` | 3-qubit GHZ correlations |  
| `test_decoherence()` | Timeout collapse verification |  

Run tests:  
```bash  
!./qubit_test 
```  


