# Entangld

# Hybrid Qubit Computation Methodology (Quantum + Classic)

## Overview

This C++ implementation provides a quantum-inspired qubit system that simulates key quantum computing concepts including superposition, entanglement, measurement, and decoherence. The system uses shared memory for inter-process communication, allowing qubits to be shared across different processes.

## Key Features

- Single qubit operations (state manipulation, gates, measurement)
- Multi-qubit entanglement (Bell states, GHZ states)
- Measurement propagation between entangled qubits
- Automatic decoherence over time
- Shared memory based communication
- Thread-safe operations

## Data Structures

### `QubitState` Struct

Represents the state of a single qubit in shared memory:

```cpp
struct QubitState {
    double alpha_real;        // Real part of |0> coefficient
    double alpha_imag;        // Imaginary part of |0> coefficient
    double beta_real;         // Real part of |1> coefficient
    double beta_imag;         // Imaginary part of |1> coefficient
    uint8_t measured;         // 0=|0>, 1=|1>, 2=not measured (superposition)
    char links[4][64];        // Names of up to 4 shared-memory peers
    uint32_t link_count;      // Number of linked qubits
    uint32_t task_id;         // Owner task identifier
    uint64_t created_at;      // Creation timestamp (ms)
    uint64_t decohere_timeout_ms; // Time until decoherence (ms)
};
```

## `Qubit` Class

### Public Methods

#### Constructor/Destructor
```cpp
Qubit(const std::string &name, uint32_t taskId, uint64_t decohereTimeoutMs = 5000)
```
- Creates or opens a shared memory qubit with given name
- `taskId` identifies the owning process
- `decohereTimeoutMs` sets time until automatic decoherence (default 5000ms)

```cpp
~Qubit()
```
- Cleans up shared memory resources

#### State Operations
```cpp
void initSuperposition()
```
- Initializes qubit to equal superposition state (|0> + |1>)/√2

```cpp
uint8_t measure()
```
- Measures the qubit, collapsing the state probabilistically
- Returns 0 (|0>) or 1 (|1>)
- Propagates measurement to all entangled qubits

```cpp
void applyGate(char gate)
```
- Applies a quantum gate to the qubit:
  - 'H': Hadamard gate (creates superposition)
  - 'X': Pauli-X gate (bit flip)
  - 'Z': Pauli-Z gate (phase flip)

```cpp
void entangle(const std::vector<std::string>& peers)
```
- Entangles this qubit with up to 4 others (by shared memory name)

```cpp
void setState(double ar, double ai, double br, double bi)
```
- Sets custom state amplitudes:
  - `ar`, `ai`: Real and imaginary parts of |0> coefficient
  - `br`, `bi`: Real and imaginary parts of |1> coefficient

#### Information
```cpp
void printState() const
```
- Prints current state information to stdout

```cpp
bool isMeasured() const
```
- Returns true if qubit has been measured (collapsed)

```cpp
uint8_t getMeasurement() const
```
- Returns measurement result (only valid if measured)

```cpp
const std::string& name() const
```
- Returns shared memory name of this qubit

## Utility Functions

```cpp
void formGHZGroup(std::vector<Qubit*>& qubits)
```
- Creates a GHZ (Greenberger-Horne-Zeilinger) entangled state among 2-5 qubits
- All qubits will be in superposition and fully entangled

## Testing Framework

The implementation includes a comprehensive test suite:

1. **Single Qubit Operations**
   - State initialization
   - Gate operations
   - Measurement statistics

2. **Bell State (2-Qubit Entanglement)**
   - Entanglement creation
   - Measurement correlation testing

3. **GHZ State (3-Qubit Entanglement)**
   - Multi-qubit entanglement
   - Measurement propagation

4. **Decoherence**
   - Tests automatic state collapse after timeout

5. **Advanced Entanglement (4-Qubit)**
   - Larger entangled systems
   - Complex measurement propagation

## Usage Example

```cpp
// Create two entangled qubits (Bell state)
Qubit q1("qubit1", 1234);
Qubit q2("qubit2", 1234);

q1.setState(1.0, 0.0, 0.0, 0.0); // |0>
q1.applyGate('H'); // (|0> + |1>)/√2
q2.setState(1.0, 0.0, 0.0, 0.0); // |0>

// Entangle them
q1.entangle({"qubit2"});
q2.entangle({"qubit1"});

// Measure - results will be correlated
uint8_t result1 = q1.measure();
uint8_t result2 = q2.measure();

// Print states
q1.printState();
q2.printState();
```

## Implementation Details

- **Thread Safety**: All operations are protected by mutex locks
- **Shared Memory**: Uses POSIX shared memory (`shm_open`, `mmap`)
- **Decoherence**: Background thread checks for timeout and collapses state
- **Measurement Propagation**: Automatically propagates to all linked qubits

## Limitations

- Maximum of 4 entangled qubits per system
- Shared memory names must be unique system-wide
- No error recovery for shared memory failures
- Basic quantum operations only (no arbitrary unitary gates)

This implementation provides a practical simulation of key quantum computing concepts while demonstrating shared memory inter-process communication and thread-safe design.
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


===== QUANTUM QUBIT SYSTEM TEST SUITE =====
Testing all features of the quantum-inspired qubit implementation

===== TEST 1: SINGLE QUBIT OPERATIONS =====
Qubit 'qubit_single': Collapsed to |0>
Links: 0
Decoherence: 5000ms

[Initialized to |0>]
Qubit 'qubit_single': |ψ> = (1.000+0.000i)|0> + (0.000+0.000i)|1>
Links: 0
Decoherence: 5000ms

[Applied Hadamard gate]
Qubit 'qubit_single': |ψ> = (0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 0
Decoherence: 5000ms

Measurement statistics (10000 trials):
|0>: 4962 (49.620%)
|1>: 5038 (50.380%)

[Testing gates]
After X gate: Qubit 'qubit_single': |ψ> = (0.000+0.000i)|0> + (1.000+0.000i)|1>
Links: 0
Decoherence: 5000ms
After Z gate: Qubit 'qubit_single': |ψ> = (0.000+0.000i)|0> + (-1.000+-0.000i)|1>
Links: 0
Decoherence: 5000ms
After H gate: Qubit 'qubit_single': |ψ> = (-0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 0
Decoherence: 5000ms
TEST 1 COMPLETE


===== TEST 2: BELL STATE (2-QUBIT ENTANGLEMENT) =====
Initial Bell state prepared:
Qubit 'bell_qubit1': |ψ> = (0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 1 bell_qubit2
Decoherence: 5000ms
Qubit 'bell_qubit2': |ψ> = (1.000+0.000i)|0> + (0.000+0.000i)|1>
Links: 1 bell_qubit1
Decoherence: 5000ms

Correlation statistics (1000 trials):
Same measurement: 1000 (100.000%)
Different measurement: 0 (0.000%)
TEST 2 COMPLETE


===== TEST 3: GHZ STATE (3-QUBIT ENTANGLEMENT) =====
Initial GHZ state prepared:
Qubit 'ghz_qubit1': |ψ> = (0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 2 ghz_qubit2 ghz_qubit3
Decoherence: 5000ms
Qubit 'ghz_qubit2': |ψ> = (0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 2 ghz_qubit1 ghz_qubit3
Decoherence: 5000ms
Qubit 'ghz_qubit3': |ψ> = (0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 2 ghz_qubit1 ghz_qubit2
Decoherence: 5000ms

Correlation statistics (1000 trials):
All same: 1000 (100.000%)
Not all same: 0 (0.000%)

Testing measurement propagation:
Before measurement:
Qubit 'ghz_qubit1': |ψ> = (0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 2 ghz_qubit2 ghz_qubit3
Decoherence: 5000ms
Qubit 'ghz_qubit2': |ψ> = (0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 2 ghz_qubit1 ghz_qubit3
Decoherence: 5000ms
Qubit 'ghz_qubit3': |ψ> = (0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 2 ghz_qubit1 ghz_qubit2
Decoherence: 5000ms

Measuring qubit 1...
Result: 0
After measurement:
Qubit 'ghz_qubit1': Collapsed to |0>
Links: 2 ghz_qubit2 ghz_qubit3
Decoherence: 5000ms
Qubit 'ghz_qubit2': Collapsed to |0>
Links: 2 ghz_qubit1 ghz_qubit3
Decoherence: 5000ms
Qubit 'ghz_qubit3': Collapsed to |0>
Links: 2 ghz_qubit1 ghz_qubit2
Decoherence: 5000ms
SUCCESS: All qubits collapsed to same state
TEST 3 COMPLETE


===== TEST 4: DECOHERENCE =====
Initial state:
Qubit 'decoherence_qubit': |ψ> = (0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 0
Decoherence: 500ms

Waiting 300ms (should not decohere)...
Qubit still in superposition (correct)

Waiting 500ms more (should decohere)...
Qubit collapsed to |1> (correct)
Qubit 'decoherence_qubit': Collapsed to |1>
Links: 0
Decoherence: 500ms
TEST 4 COMPLETE


===== TEST 5: ADVANCED ENTANGLEMENT (4-QUBIT) =====
4-qubit GHZ state prepared:
Qubit 'adv_qubit1': |ψ> = (0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 3 adv_qubit2 adv_qubit3 adv_qubit4
Decoherence: 5000ms
Qubit 'adv_qubit2': |ψ> = (0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 3 adv_qubit1 adv_qubit3 adv_qubit4
Decoherence: 5000ms
Qubit 'adv_qubit3': |ψ> = (0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 3 adv_qubit1 adv_qubit2 adv_qubit4
Decoherence: 5000ms
Qubit 'adv_qubit4': |ψ> = (0.707+0.000i)|0> + (0.707+0.000i)|1>
Links: 3 adv_qubit1 adv_qubit2 adv_qubit3
Decoherence: 5000ms

Measuring first qubit...
Result: 1
All qubits after measurement:
Qubit 'adv_qubit1': Collapsed to |1>
Links: 3 adv_qubit2 adv_qubit3 adv_qubit4
Decoherence: 5000ms
Qubit 'adv_qubit2': Collapsed to |1>
Links: 3 adv_qubit1 adv_qubit3 adv_qubit4
Decoherence: 5000ms
Qubit 'adv_qubit3': Collapsed to |1>
Links: 3 adv_qubit1 adv_qubit2 adv_qubit4
Decoherence: 5000ms
Qubit 'adv_qubit4': Collapsed to |1>
Links: 3 adv_qubit1 adv_qubit2 adv_qubit3
Decoherence: 5000ms
SUCCESS: All qubits collapsed to same state
TEST 5 COMPLETE


===== ALL TESTS COMPLETED SUCCESSFULLY =====


```  


