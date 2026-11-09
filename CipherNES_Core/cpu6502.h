#pragma once
#include <cstdint>
#include <array>
#include <vector>

struct Bus;

// A pragmatic 6502 core suitable for NES CPU emulation.
//
// Notes:
// - This file only defines the CPU. Bus/PPU/APU/Cart mapping is handled elsewhere.
// - The public surface area is intentionally small to match your current skeleton.
// - step() executes exactly one instruction and returns the number of cycles consumed
//   for that instruction (including common page-cross penalties where applicable).
struct CPU6502 {
    Bus* bus = nullptr;

    // Registers
    uint8_t  A = 0;
    uint8_t  X = 0;
    uint8_t  Y = 0;
    uint8_t  SP = 0xFD;
    uint8_t  P  = 0x24;     // typical power-up-ish state (not exact)
    uint16_t PC = 0x0000;

    enum Flags : uint8_t {
        C = 1 << 0, // Carry
        Z = 1 << 1, // Zero
        I = 1 << 2, // Interrupt Disable
        D = 1 << 3, // Decimal (unused on NES but still exists)
        B = 1 << 4, // Break
        U = 1 << 5, // Unused (always 1 when pushed to stack)
        V = 1 << 6, // Overflow
        N = 1 << 7  // Negative
    };

    void connect(Bus* b) { bus = b; }

    void reset(bool hardReset = true);

    // Optional interrupt entry points (not wired by your current bus yet).
    // These are provided so you can hook PPU/APU/cartridge IRQ/NMI later
    // without changing the CPU core again.
    void irq();
    void nmi();
    void cancelPendingNmi();

    // Cycle-accurate ticking. Each call to clock() advances the CPU by exactly
    // one CPU cycle.
    void clock();

    bool upcomingBusAccessIsWrite() const;
    bool upcomingBusAddress(uint16_t& addr) const;
    // Classify the next externally visible CPU bus cycle once. The DMA arbiter
    // needs both the address and read/write direction; computing them together
    // avoids decoding the same microcycle twice on every CPU clock.
    bool upcomingBusAccess(uint16_t& addr, bool& isWrite) const;
    void notifyDmaCycle();

    // True when the CPU is ready to start a new instruction (i.e. no remaining
    // cycles for the current instruction/interrupt).
    bool complete() const { return cycles == 0; }

    // Execute a single instruction. Returns cycles used; returns -1 on bus null.
    int step();

    // Save-state support
    void saveState(std::vector<uint8_t>& out) const;
    bool loadState(const uint8_t*& p, const uint8_t* end);

private:
    // Core bus ops
    uint8_t read(uint16_t addr);
    void    write(uint16_t addr, uint8_t data);

    // Flag helpers
    void setFlag(Flags f, bool v);
    bool getFlag(Flags f) const;

    // Legacy helpers from your initial skeleton (kept for compatibility)
    void lda_imm();
    void sta_abs();
    void inx();
    void jmp_abs();
    void nop();

private:
    // Internal execution state
    uint8_t  fetched  = 0x00;
    uint16_t addr_abs = 0x0000;
    uint16_t addr_base = 0x0000;
    uint16_t addr_rel = 0x0000;
    uint8_t  opcode   = 0x00;

    // Remaining cycles for the current instruction/interrupt sequence
    uint8_t  cycles   = 0x00;
    uint8_t  microCycle = 0x00;
    uint8_t  microLo = 0x00;
    uint8_t  microHi = 0x00;
    uint8_t  microData = 0x00;
    uint16_t microBase = 0x0000;
    bool     microPageCrossed = false;
    bool     microFetchCached = false;
    bool     microBranchTaken = false;
    bool     unstableStoreIgnoreH = false;
    bool     microInterrupt = false;
    uint16_t microVector = 0x0000;

    // Interrupt latches (serviced between instructions)
    bool irqPending = false;
    bool nmiPending = false;
    bool interruptArmed = false;
    uint16_t interruptArmedVector = 0x0000;

    struct INSTRUCTION {
        const char* name;
        uint8_t (CPU6502::*operate)(void);
        uint8_t (CPU6502::*addrmode)(void);
        uint8_t cycles;
        uint8_t traits;

        INSTRUCTION()
            : name(nullptr), operate(nullptr), addrmode(nullptr), cycles(0), traits(0) {}
        INSTRUCTION(const char* n, uint8_t (CPU6502::*op)(void),
                    uint8_t (CPU6502::*am)(void), uint8_t c)
            : name(n), operate(op), addrmode(am), cycles(c), traits(0) {}
    };

    std::array<INSTRUCTION, 256> lookup{};
    bool lookupInitialized = false;

private:
    void initLookup();
    void executeMicroCycle();
    void finishMicroInstruction();
    void executeInterruptMicroCycle();
    void pollInterruptsForNextInstruction();
    enum InstructionTrait : uint8_t { TRAIT_RMW = 1u, TRAIT_STORE = 2u };
    bool isReadModifyWriteOp() const;
    bool isStoreOp() const;
    bool needsIndexedDummyRead() const;

    // Addressing modes
    uint8_t IMP(); // Implied
    uint8_t ACC(); // Accumulator
    uint8_t IMM(); // Immediate
    uint8_t ZP0(); // Zero page
    uint8_t ZPX(); // Zero page, X
    uint8_t ZPY(); // Zero page, Y
    uint8_t ABS(); // Absolute
    uint8_t JSR_ABS(); // JSR's unusual absolute operand timing
    uint8_t ABX(); // Absolute, X
    uint8_t ABY(); // Absolute, Y
    uint8_t IND(); // Indirect
    uint8_t IZX(); // Indirect, X
    uint8_t IZY(); // Indirect, Y
    uint8_t REL(); // Relative (branch)

    // Operations
    uint8_t XXX(); // Illegal/unimplemented opcode

    uint8_t ADC();
    uint8_t SBC();

    uint8_t AND();
    uint8_t ORA();
    uint8_t EOR();

    uint8_t ASL();
    uint8_t LSR();
    uint8_t ROL();
    uint8_t ROR();

    uint8_t BIT();

    uint8_t LDA();
    uint8_t LDX();
    uint8_t LDY();

    uint8_t STA();
    uint8_t STX();
    uint8_t STY();

    uint8_t CMP();
    uint8_t CPX();
    uint8_t CPY();

    uint8_t INC();
    uint8_t INX_OP();
    uint8_t INY();

    uint8_t DEC();
    uint8_t DEX();
    uint8_t DEY();

    uint8_t JMP();
    uint8_t JSR();
    uint8_t RTS();
    uint8_t RTI();
    uint8_t BRK();

    uint8_t BCC();
    uint8_t BCS();
    uint8_t BEQ();
    uint8_t BMI();
    uint8_t BNE();
    uint8_t BPL();
    uint8_t BVC();
    uint8_t BVS();

    uint8_t CLC();
    uint8_t CLD();
    uint8_t CLI();
    uint8_t CLV();

    uint8_t SEC();
    uint8_t SED();
    uint8_t SEI();

    uint8_t NOP_OP();

    // Undocumented/illegal opcodes (widely emulated; some NES software uses them)
    uint8_t NOP_ILL(); // illegal NOP variants (read operand when applicable)
    uint8_t LAX();
    uint8_t SAX();
    uint8_t DCP();
    uint8_t ISC();
    uint8_t SLO();
    uint8_t RLA();
    uint8_t SRE();
    uint8_t RRA();
    uint8_t ANC();
    uint8_t ALR();
    uint8_t ARR();
    uint8_t AXS();
    uint8_t LAS();
    uint8_t TAS();
    uint8_t AHX();
    uint8_t SHX();
    uint8_t SHY();
    uint8_t XAA();

    uint8_t PHA();
    uint8_t PHP();
    uint8_t PLA();
    uint8_t PLP();

    uint8_t TAX();
    uint8_t TAY();
    uint8_t TSX();
    uint8_t TXA();
    uint8_t TXS();
    uint8_t TYA();

    // Helpers
    uint8_t fetch();

    void push(uint8_t v);
    uint8_t pop();

    uint8_t branchIf(bool cond);

    // Interrupt helper (common entry for IRQ/NMI)
    void interrupt(uint16_t vectorAddr);
};
