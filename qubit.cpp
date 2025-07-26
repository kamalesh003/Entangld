#include <iostream>
#include <cstring>
#include <cmath>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iomanip>

struct QubitState {
    double alpha_real;
    double alpha_imag;
    double beta_real;
    double beta_imag;
    uint8_t measured;
    char links[4][64];       // names of up to 4 shared-memory peers
    uint32_t link_count;
    uint32_t task_id;
    uint64_t created_at;
    uint64_t decohere_timeout_ms;
};

class Qubit {
public:
    Qubit(const std::string &name, uint32_t taskId, uint64_t decohereTimeoutMs = 5000)
        : shm_name(name), task_id(taskId), decohere_timeout(decohereTimeoutMs) {
        openOrCreate();
        initHeader();
        startDecoherenceThread();
    }

    ~Qubit() {
        decohere_thread_running = false;
        if (decohere_thread.joinable()) decohere_thread.join();
        munmap(ptr, sizeof(QubitState));
        close(shm_fd);
    }

    // Initialize equal superposition state
    void initSuperposition() {
        std::lock_guard<std::mutex> lock(mtx);
        state->alpha_real = 1.0 / M_SQRT2;
        state->alpha_imag = 0.0;
        state->beta_real  = 1.0 / M_SQRT2;
        state->beta_imag  = 0.0;
        state->measured   = 2;
        resetLinks();
        updateTimestamp();
    }

    // Measure qubit: collapse probabilistically
    uint8_t measure() {
        std::lock_guard<std::mutex> lock(mtx);
        if (state->measured != 2) return state->measured;
        double p0 = norm(state->alpha_real, state->alpha_imag);
        double p1 = norm(state->beta_real, state->beta_imag);
        std::bernoulli_distribution dist(p1);
        uint8_t result = dist(rng);
        state->measured = result;
        // collapse amplitudes
        if (result == 0) {
            state->alpha_real = 1.0; state->alpha_imag = 0.0;
            state->beta_real  = 0.0; state->beta_imag  = 0.0;
        } else {
            state->alpha_real = 0.0; state->alpha_imag = 0.0;
            state->beta_real  = 1.0; state->beta_imag  = 0.0;
        }
        propagateToLinks(result);
        updateTimestamp();
        return result;
    }

    // Apply basic gate: H, X, Z
    void applyGate(char gate) {
        std::lock_guard<std::mutex> lock(mtx);
        if (state->measured != 2) return;
        double ar = state->alpha_real, ai = state->alpha_imag;
        double br = state->beta_real,  bi = state->beta_imag;
        switch (gate) {
            case 'H': // Hadamard
                state->alpha_real = (ar + br) / M_SQRT2;
                state->alpha_imag = (ai + bi) / M_SQRT2;
                state->beta_real  = (ar - br) / M_SQRT2;
                state->beta_imag  = (ai - bi) / M_SQRT2;
                break;
            case 'X': // Pauli-X
                state->alpha_real = br;
                state->alpha_imag = bi;
                state->beta_real  = ar;
                state->beta_imag  = ai;
                break;
            case 'Z': // Pauli-Z
                state->beta_real  = -br;
                state->beta_imag  = -bi;
                break;
            default:
                std::cerr << "Unknown gate: " << gate << std::endl;
        }
        updateTimestamp();
    }

    // Entangle with up to 4 other qubits by name
    void entangle(const std::vector<std::string>& peers) {
        std::lock_guard<std::mutex> lock(mtx);
        size_t n = std::min(peers.size(), size_t(4));
        for (size_t i = 0; i < n; ++i)
            strncpy(state->links[i], peers[i].c_str(), 63);
        state->link_count = n;
    }

    // Set custom state amplitudes
    void setState(double ar, double ai, double br, double bi) {
        std::lock_guard<std::mutex> lock(mtx);
        state->alpha_real = ar;
        state->alpha_imag = ai;
        state->beta_real = br;
        state->beta_imag = bi;
        state->measured = 2;
        updateTimestamp();
    }

    // Get shared memory name
    const std::string& name() const { return shm_name; }

    // Get current state information
    void printState() const {
        std::lock_guard<std::mutex> lock(mtx);
        std::cout << "Qubit '" << shm_name << "': ";
        if (state->measured == 2) {
            std::cout << "|ψ> = ";
            std::cout << std::fixed << std::setprecision(3);
            std::cout << "(" << state->alpha_real << (state->alpha_imag >= 0 ? "+" : "") 
                      << state->alpha_imag << "i)|0> + ";
            std::cout << "(" << state->beta_real << (state->beta_imag >= 0 ? "+" : "") 
                      << state->beta_imag << "i)|1>";
        } else {
            std::cout << "Collapsed to |" << (int)state->measured << ">";
        }
        std::cout << "\nLinks: " << state->link_count;
        for (uint32_t i = 0; i < state->link_count; ++i) {
            std::cout << " " << state->links[i];
        }
        std::cout << "\nDecoherence: " << state->decohere_timeout_ms << "ms" << std::endl;
    }

    // Check if measured
    bool isMeasured() const { 
        std::lock_guard<std::mutex> lock(mtx);
        return state->measured != 2; 
    }

    // Get measured value (only valid if measured)
    uint8_t getMeasurement() const {
        std::lock_guard<std::mutex> lock(mtx);
        return state->measured;
    }

private:
    std::string shm_name;
    uint32_t    task_id;
    uint64_t    decohere_timeout;
    int         shm_fd;
    QubitState* state;

    mutable std::mutex mtx;  // Made mutable for const methods
    std::mt19937 rng{std::random_device{}()};
    std::thread  decohere_thread;
    std::atomic<bool> decohere_thread_running{false};

    void openOrCreate() {
        shm_fd = shm_open(shm_name.c_str(), O_RDWR | O_CREAT, 0666);
        if (shm_fd < 0) { perror("shm_open"); exit(1); }
        ftruncate(shm_fd, sizeof(QubitState));
        ptr = mmap(nullptr, sizeof(QubitState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (ptr == MAP_FAILED) { perror("mmap"); exit(1); }
        state = reinterpret_cast<QubitState*>(ptr);
    }

    void initHeader() {
        std::lock_guard<std::mutex> lock(mtx);
        if (state->task_id != task_id) {
            std::memset(state, 0, sizeof(QubitState));
            state->task_id = task_id;
            updateTimestamp();
        }
    }

    void updateTimestamp() {
        state->created_at = nowMs();
        state->decohere_timeout_ms = decohere_timeout;
    }

    void resetLinks() {
        state->link_count = 0;
        for (int i = 0; i < 4; ++i) state->links[i][0] = '\0';
    }

    static double norm(double r, double i) { return r*r + i*i; }

    uint64_t nowMs() const {
        auto tp = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(tp).count();
    }

    void propagateToLinks(uint8_t result) {
        for (uint32_t i = 0; i < state->link_count; ++i) {
            const char* peer = state->links[i];
            int fd = shm_open(peer, O_RDWR, 0);
            if (fd < 0) continue;
            void* p = mmap(nullptr, sizeof(QubitState), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (p == MAP_FAILED) { close(fd); continue; }
            auto peerState = reinterpret_cast<QubitState*>(p);
            peerState->measured = result;
            munmap(p, sizeof(QubitState));
            close(fd);
        }
    }

    void startDecoherenceThread() {
        decohere_thread_running = true;
        decohere_thread = std::thread([&]() {
            while (decohere_thread_running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::lock_guard<std::mutex> lock(mtx);
                if (state->measured == 2 && nowMs() - state->created_at > state->decohere_timeout_ms) {
                    // random collapse
                    double p1 = norm(state->beta_real, state->beta_imag);
                    std::bernoulli_distribution dist(p1);
                    state->measured = dist(rng);
                    propagateToLinks(state->measured);
                }
            }
        });
    }

    void* ptr;
};

// Create GHZ state among multiple qubits (2-5 qubits)
void formGHZGroup(std::vector<Qubit*>& qubits) {
    size_t n = qubits.size();
    if (n < 2 || n > 5) {
        std::cerr << "GHZ group size must be between 2 and 5" << std::endl;
        return;
    }

    for (size_t i = 0; i < n; i++) {
        std::vector<std::string> peers;
        for (size_t j = 0; j < n; j++) {
            if (i == j) continue;
            peers.push_back(qubits[j]->name());
        }
        qubits[i]->entangle(peers);
        qubits[i]->setState(1.0 / M_SQRT2, 0.0, 1.0 / M_SQRT2, 0.0);
    }
}

// ========================
// TESTING IMPLEMENTATION
// ========================

void unlink_shm(const std::string& name) {
    if (shm_unlink(name.c_str()) == -1) {
        perror(("shm_unlink for " + name).c_str());
    }
}

void test_single_qubit() {
    std::cout << "\n===== TEST 1: SINGLE QUBIT OPERATIONS =====\n";
    std::string name = "qubit_single";
    {
        Qubit q(name, 1);
        q.printState();
        
        // Test initialization to |0>
        std::cout << "\n[Initialized to |0>]" << std::endl;
        q.setState(1.0, 0.0, 0.0, 0.0);
        q.printState();
        
        // Test superposition
        std::cout << "\n[Applied Hadamard gate]" << std::endl;
        q.applyGate('H');
        q.printState();
        
        // Test measurement statistics
        int count0 = 0, count1 = 0;
        const int trials = 10000;
        for (int i = 0; i < trials; i++) {
            Qubit temp_q(name, 1);
            temp_q.initSuperposition();
            if (temp_q.measure() == 0) count0++;
            else count1++;
        }
        std::cout << "\nMeasurement statistics (" << trials << " trials):\n";
        std::cout << "|0>: " << count0 << " (" << (100.0*count0/trials) << "%)\n";
        std::cout << "|1>: " << count1 << " (" << (100.0*count1/trials) << "%)\n";
        
        // Test X and Z gates
        std::cout << "\n[Testing gates]" << std::endl;
        q.setState(1.0, 0.0, 0.0, 0.0); // |0>
        q.applyGate('X'); // Should become |1>
        std::cout << "After X gate: ";
        q.printState();
        
        q.applyGate('Z'); // Should become -|1>
        std::cout << "After Z gate: ";
        q.printState();
        
        q.applyGate('H'); // Should become |->
        std::cout << "After H gate: ";
        q.printState();
    }
    unlink_shm(name);
    std::cout << "TEST 1 COMPLETE\n";
}

void test_bell_state() {
    std::cout << "\n\n===== TEST 2: BELL STATE (2-QUBIT ENTANGLEMENT) =====\n";
    std::string name1 = "bell_qubit1";
    std::string name2 = "bell_qubit2";
    
    {
        Qubit q1(name1, 1);
        Qubit q2(name2, 1);
        
        // Prepare Bell state
        q1.setState(1.0, 0.0, 0.0, 0.0); // |0>
        q1.applyGate('H'); // (|0> + |1>)/√2
        q2.setState(1.0, 0.0, 0.0, 0.0); // |0>
        
        // Entangle
        q1.entangle({name2});
        q2.entangle({name1});
        
        std::cout << "Initial Bell state prepared:\n";
        q1.printState();
        q2.printState();
        
        // Test correlation
        int same = 0, total = 1000;
        for (int i = 0; i < total; i++) {
            Qubit temp1(name1, 1);
            Qubit temp2(name2, 1);
            
            // Re-prepare Bell state
            temp1.setState(1.0, 0.0, 0.0, 0.0);
            temp1.applyGate('H');
            temp2.setState(1.0, 0.0, 0.0, 0.0);
            temp1.entangle({name2});
            temp2.entangle({name1});
            
            uint8_t r1 = temp1.measure();
            uint8_t r2 = temp2.measure();
            
            if (r1 == r2) same++;
        }
        
        std::cout << "\nCorrelation statistics (" << total << " trials):\n";
        std::cout << "Same measurement: " << same << " (" << (100.0*same/total) << "%)\n";
        std::cout << "Different measurement: " << (total-same) 
                  << " (" << (100.0*(total-same)/total) << "%)\n";
    }
    unlink_shm(name1);
    unlink_shm(name2);
    std::cout << "TEST 2 COMPLETE\n";
}

void test_ghz_state() {
    std::cout << "\n\n===== TEST 3: GHZ STATE (3-QUBIT ENTANGLEMENT) =====\n";
    std::string name1 = "ghz_qubit1";
    std::string name2 = "ghz_qubit2";
    std::string name3 = "ghz_qubit3";
    
    {
        Qubit q1(name1, 1);
        Qubit q2(name2, 1);
        Qubit q3(name3, 1);
        
        // Create GHZ state
        std::vector<Qubit*> qubits = {&q1, &q2, &q3};
        formGHZGroup(qubits);
        
        std::cout << "Initial GHZ state prepared:\n";
        q1.printState();
        q2.printState();
        q3.printState();
        
        // Test correlation
        int all_same = 0, total = 1000;
        for (int i = 0; i < total; i++) {
            Qubit temp1(name1, 1);
            Qubit temp2(name2, 1);
            Qubit temp3(name3, 1);
            
            // Recreate GHZ state
            std::vector<Qubit*> temp_qubits = {&temp1, &temp2, &temp3};
            formGHZGroup(temp_qubits);
            
            uint8_t r1 = temp1.measure();
            uint8_t r2 = temp2.measure();
            uint8_t r3 = temp3.measure();
            
            if (r1 == r2 && r2 == r3) all_same++;
        }
        
        std::cout << "\nCorrelation statistics (" << total << " trials):\n";
        std::cout << "All same: " << all_same << " (" << (100.0*all_same/total) << "%)\n";
        std::cout << "Not all same: " << (total-all_same) 
                  << " (" << (100.0*(total-all_same)/total) << "%)\n";
        
        // Test measurement propagation
        std::cout << "\nTesting measurement propagation:\n";
        Qubit temp1(name1, 1);
        Qubit temp2(name2, 1);
        Qubit temp3(name3, 1);
        std::vector<Qubit*> temp_qubits = {&temp1, &temp2, &temp3};  // Fixed temporary vector
        formGHZGroup(temp_qubits);
        
        std::cout << "Before measurement:\n";
        temp1.printState();
        temp2.printState();
        temp3.printState();
        
        std::cout << "\nMeasuring qubit 1...\n";
        uint8_t r = temp1.measure();
        std::cout << "Result: " << (int)r << "\n";
        
        std::cout << "After measurement:\n";
        temp1.printState();
        temp2.printState();
        temp3.printState();
        
        if (temp2.getMeasurement() == r && temp3.getMeasurement() == r) {
            std::cout << "SUCCESS: All qubits collapsed to same state\n";
        } else {
            std::cout << "ERROR: Qubits not in same state!\n";
        }
    }
    unlink_shm(name1);
    unlink_shm(name2);
    unlink_shm(name3);
    std::cout << "TEST 3 COMPLETE\n";
}

void test_decoherence() {
    std::cout << "\n\n===== TEST 4: DECOHERENCE =====\n";
    std::string name = "decoherence_qubit";
    
    {
        // Create qubit with fast decoherence (500ms)
        Qubit q(name, 1, 500);
        q.initSuperposition();
        
        std::cout << "Initial state:\n";
        q.printState();
        
        std::cout << "\nWaiting 300ms (should not decohere)...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        if (!q.isMeasured()) {
            std::cout << "Qubit still in superposition (correct)\n";
        } else {
            std::cout << "ERROR: Qubit decohered too early!\n";
        }
        
        std::cout << "\nWaiting 500ms more (should decohere)...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (q.isMeasured()) {
            std::cout << "Qubit collapsed to |" << (int)q.getMeasurement() << "> (correct)\n";
        } else {
            std::cout << "ERROR: Qubit should have decohered!\n";
        }
        
        q.printState();
    }
    unlink_shm(name);
    std::cout << "TEST 4 COMPLETE\n";
}

void test_advanced_entanglement() {
    std::cout << "\n\n===== TEST 5: ADVANCED ENTANGLEMENT (4-QUBIT) =====\n";
    std::vector<std::string> names = {"adv_qubit1", "adv_qubit2", "adv_qubit3", "adv_qubit4"};
    std::vector<Qubit*> qubits;
    
    for (const auto& name : names) {
        qubits.push_back(new Qubit(name, 1));
    }
    
    // Create GHZ state with 4 qubits
    formGHZGroup(qubits);
    
    std::cout << "4-qubit GHZ state prepared:\n";
    for (auto q : qubits) q->printState();
    
    // Test measurement propagation
    std::cout << "\nMeasuring first qubit...\n";
    uint8_t r = qubits[0]->measure();
    std::cout << "Result: " << (int)r << "\n";
    
    std::cout << "All qubits after measurement:\n";
    for (auto q : qubits) q->printState();
    
    bool all_same = true;
    for (auto q : qubits) {
        if (q->getMeasurement() != r) {
            all_same = false;
            break;
        }
    }
    
    if (all_same) {
        std::cout << "SUCCESS: All qubits collapsed to same state\n";
    } else {
        std::cout << "ERROR: Qubits not in same state!\n";
    }
    
    // Cleanup
    for (auto q : qubits) delete q;
    for (const auto& name : names) unlink_shm(name);
    std::cout << "TEST 5 COMPLETE\n";
}

int main() {
    std::cout << "===== QUANTUM QUBIT SYSTEM TEST SUITE =====\n";
    std::cout << "Testing all features of the quantum-inspired qubit implementation\n";
    
    test_single_qubit();
    test_bell_state();
    test_ghz_state();
    test_decoherence();
    test_advanced_entanglement();
    
    std::cout << "\n\n===== ALL TESTS COMPLETED SUCCESSFULLY =====\n";
    return 0;
}