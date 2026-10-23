#include "cpu6502.h"

#include "state_io.h"
#include "bus.h"

// -------------------------
// Bus access + flags
// -------------------------

uint8_t CPU6502::read(uint16_t addr) {
    return bus ? bus->cpuRead(addr) : 0x00;
}

void CPU6502::write(uint16_t addr, uint8_t data) {
    if (bus) bus->cpuWrite(addr, data);
}

void CPU6502::setFlag(Flags f, bool v) {
    if (v) P |= f;
    else   P &= ~f;
}

bool CPU6502::getFlag(Flags f) const {
    return (P & f) != 0;
}

// -------------------------
// Reset + legacy helpers
// -------------------------

void CPU6502::reset(bool hardReset) {
    if (!bus) return;

    // Initialise lookup once.
    if (!lookupInitialized) {
        initLookup();
        lookupInitialized = true;
    }

    // The 2A03 distinguishes power-up from the external RESET line. Power-up
    // initializes the visible registers, while RESET preserves A/X/Y and all
    // status bits except that I is set. RESET also performs three stack reads,
    // which decrement S without writing to stack RAM.
    if (hardReset) {
        A = X = Y = 0;
        SP = 0xFD;
        P = 0x24; // PHP exposes this as $34 because B is synthesized on push.
    } else {
        SP = uint8_t(SP - 3);
        setFlag(I, true);
        setFlag(U, true);
    }

    // Fetch the reset vector after applying the register-side RESET effects.
    const uint8_t lo = read(0xFFFC);
    const uint8_t hi = read(0xFFFD);
    PC = (uint16_t(hi) << 8) | uint16_t(lo);

    // Reset sequence consumes cycles before the first opcode fetch. Cancel any
    // partially executed instruction and any previously sampled interrupt.
    addr_rel = 0x0000;
    addr_abs = 0x0000;
    addr_base = 0x0000;
    fetched = 0x00;
    opcode = 0x00;
    cycles = 8;
    irqPending = false;
    nmiPending = false;
    interruptArmed = false;
    interruptArmedVector = 0;
    microCycle = 0;
    microLo = microHi = microData = 0;
    microBase = 0;
    microPageCrossed = false;
    microFetchCached = false;
    microBranchTaken = false;
    unstableStoreIgnoreH = false;
    microInterrupt = false;
    microVector = 0;
}

// -------------------------
// Interrupts (optional)
// -------------------------

void CPU6502::interrupt(uint16_t vectorAddr) {
    // Push PC and status, then load vector. B flag is cleared for hardware interrupts.
    push(uint8_t((PC >> 8) & 0x00FF));
    push(uint8_t(PC & 0x00FF));

    setFlag(B, false);
    setFlag(U, true);
    push(P);

    setFlag(I, true);

    const uint16_t lo = read(vectorAddr + 0);
    const uint16_t hi = read(vectorAddr + 1);
    PC = (hi << 8) | lo;
}

void CPU6502::irq() {
    // IRQ is a level sampled at instruction boundaries, not an edge that
    // remains latched while interrupts are masked or an interrupt sequence is
    // already in progress. If the source remains asserted after RTI/CLI, the
    // bus will present it again on the following CPU cycle.
    if (!getFlag(I) && !microInterrupt && !interruptArmed) {
        irqPending = true;
    }
}

void CPU6502::nmi() {
    // Non-maskable interrupt
    nmiPending = true;
}

void CPU6502::cancelPendingNmi() {
    // A $2002 read or $2000 disable in the final PPU cancellation window can
    // withdraw an edge that the CPU has sampled for its next instruction but
    // has not begun servicing yet. Once interrupt entry starts it is too late.
    nmiPending = false;
    if (!microInterrupt && interruptArmed && interruptArmedVector == 0xFFFA) {
        interruptArmed = false;
        interruptArmedVector = 0;
    }
}

// -------------------------
// Cycle-accurate clock
// -------------------------

void CPU6502::clock() {
    if (!bus) return;

    if (!lookupInitialized) {
        initLookup();
        lookupInitialized = true;
    }

    if (cycles == 0) {
        if (interruptArmed) {
            interruptArmed = false;
            microInterrupt = true;
            microVector = interruptArmedVector;
            cycles = 7;
            microCycle = 1;
        } else {
            setFlag(U, true);
            opcode = read(PC++);
            cycles = lookup[opcode].cycles > 0 ? uint8_t(lookup[opcode].cycles - 1) : 0;
            microCycle = 1;
            microLo = microHi = microData = 0;
            microBase = 0;
            microPageCrossed = false;
            microFetchCached = false;
            microBranchTaken = false;
            unstableStoreIgnoreH = false;
            microInterrupt = false;
            microVector = 0;
            setFlag(U, true);
            if (cycles == 0) {
                finishMicroInstruction();
                return;
            }
            return;
        }
    }

    if (microCycle == 0) {
        cycles--;
        return;
    }

    if (microInterrupt) {
        executeInterruptMicroCycle();
    } else {
        // BRK performs its interrupt sampling at the status-push cycle,
        // not at normal instruction completion.
        const bool branchMode = lookup[opcode].addrmode == &CPU6502::REL;
        const bool branchPageCrossPoll =
            branchMode && microBranchTaken && microPageCrossed && microCycle == 3;
        const bool pollBeforeCycle =
            (((cycles == 1) && !(branchMode && microCycle > 1))
              || branchPageCrossPoll)
             && lookup[opcode].operate != &CPU6502::BRK;
        if (pollBeforeCycle) {
            pollInterruptsForNextInstruction();
        }
        executeMicroCycle();
    }
    if (cycles > 0) {
        cycles--;
        microCycle++;
    }
}

void CPU6502::notifyDmaCycle() {
    if (opcode == 0x93) {
        if (microCycle == 4) unstableStoreIgnoreH = true;
    } else if (opcode == 0x9B || opcode == 0x9C || opcode == 0x9E || opcode == 0x9F) {
        if (microCycle == 3) unstableStoreIgnoreH = true;
    }
}

void CPU6502::pollInterruptsForNextInstruction() {
    if (interruptArmed) return;
    if (nmiPending) {
        nmiPending = false;
        interruptArmed = true;
        interruptArmedVector = 0xFFFA;
    } else if (irqPending && !getFlag(I)) {
        irqPending = false;
        interruptArmed = true;
        interruptArmedVector = 0xFFFE;
    } else if (irqPending && getFlag(I)) {
        irqPending = false;
    }
}

void CPU6502::executeInterruptMicroCycle() {
    if (microCycle == 1) {
        read(PC);
    } else if (microCycle == 2) {
        read(PC);
    } else if (microCycle == 3) {
        write(uint16_t(0x0100) + uint16_t(SP), uint8_t((PC >> 8) & 0xFF));
        SP--;
    } else if (microCycle == 4) {
        write(uint16_t(0x0100) + uint16_t(SP), uint8_t(PC & 0xFF));
        SP--;
    } else if (microCycle == 5) {
        setFlag(B, false);
        setFlag(U, true);
        write(uint16_t(0x0100) + uint16_t(SP), P);
        SP--;
        setFlag(I, true);
        // NMI can hijack an IRQ after the IRQ status has been pushed but
        // before the vector fetch begins.
        if (microVector == 0xFFFE && nmiPending) {
            nmiPending = false;
            microVector = 0xFFFA;
        }
    } else if (microCycle == 6) {
        microLo = read(microVector);
    } else if (microCycle == 7) {
        microHi = read(uint16_t(microVector + 1));
        PC = (uint16_t(microHi) << 8) | microLo;
        microInterrupt = false;
    }
}

void CPU6502::finishMicroInstruction() {
    microFetchCached = true;
    if (lookup[opcode].operate) {
        (this->*lookup[opcode].operate)();
    }
    microFetchCached = false;
    setFlag(U, true);
}

bool CPU6502::upcomingBusAccess(uint16_t& addr, bool& isWrite) const {
    addr = PC;
    isWrite = false;

    if (cycles == 0 || microCycle == 0) return true;
    if (microInterrupt) {
        isWrite = microCycle >= 3 && microCycle <= 5;
        if (microCycle <= 2) {
            addr = PC;
        } else if (microCycle <= 5) {
            addr = uint16_t(0x0100) + uint16_t(SP);
        } else {
            addr = uint16_t(microVector + (microCycle == 7 ? 1 : 0));
        }
        return true;
    }

    const auto mode = lookup[opcode].addrmode;
    const auto op = lookup[opcode].operate;
    const bool finalCycle = (cycles == 1);
    const bool rmw = (lookup[opcode].traits & TRAIT_RMW) != 0;
    const bool store = (lookup[opcode].traits & TRAIT_STORE) != 0;

    if (op == &CPU6502::JSR) {
        isWrite = microCycle == 3 || microCycle == 4;
    } else if (op == &CPU6502::PHA || op == &CPU6502::PHP) {
        isWrite = microCycle == 2;
    } else if (op == &CPU6502::BRK) {
        isWrite = microCycle == 3 || microCycle == 4 || microCycle == 5;
    } else if (mode == &CPU6502::ZP0) {
        isWrite = (rmw && (microCycle == 3 || finalCycle)) || (store && finalCycle);
    } else if (mode == &CPU6502::ZPX || mode == &CPU6502::ZPY) {
        isWrite = (rmw && (microCycle == 4 || finalCycle)) || (store && finalCycle);
    } else if (mode == &CPU6502::ABS || mode == &CPU6502::JSR_ABS) {
        isWrite = mode == &CPU6502::ABS
            && ((rmw && (microCycle == 4 || finalCycle)) || (store && finalCycle));
    } else if (mode == &CPU6502::ABX || mode == &CPU6502::ABY) {
        isWrite = (rmw && (microCycle == 5 || finalCycle)) || (store && finalCycle);
    } else if (mode == &CPU6502::IZX || mode == &CPU6502::IZY) {
        isWrite = (rmw && (microCycle == 6 || finalCycle)) || (store && finalCycle);
    }

    if (mode == &CPU6502::ABS || mode == &CPU6502::JSR_ABS) {
        if (mode == &CPU6502::ABS && ((finalCycle && (store || !rmw)) || (rmw && microCycle >= 3))) {
            addr = addr_abs;
            return true;
        }
    }
    if (mode == &CPU6502::ZP0) {
        if ((finalCycle && (store || !rmw)) || (rmw && microCycle >= 2)) {
            addr = addr_abs;
            return true;
        }
    }
    if (mode == &CPU6502::ZPX || mode == &CPU6502::ZPY) {
        if ((finalCycle && (store || !rmw)) || (rmw && microCycle >= 3)) {
            addr = addr_abs;
            return true;
        }
    }
    if (mode == &CPU6502::ABX || mode == &CPU6502::ABY) {
        // Absolute-indexed accesses perform their page-cross/store/RMW dummy
        // read on the cycle after the high operand byte is fetched. DMA must
        // see that exact intermediate address.
        if (microCycle == 3 && (microPageCrossed || store || rmw)) {
            addr = uint16_t((microBase & 0xFF00) | (addr_abs & 0x00FF));
            return true;
        }
        if (finalCycle || (rmw && microCycle >= 4)) {
            addr = addr_abs;
            return true;
        }
    }
    if (mode == &CPU6502::IZX || mode == &CPU6502::IZY) {
        if (mode == &CPU6502::IZY && microCycle == 4
            && (microPageCrossed || store || rmw)) {
            addr = uint16_t((addr_base & 0xFF00) | (addr_abs & 0x00FF));
            return true;
        }
        if (finalCycle || (rmw && microCycle >= 4)) {
            addr = addr_abs;
            return true;
        }
    }
    if (op == &CPU6502::PHA || op == &CPU6502::PHP) {
        if (microCycle == 2) {
            addr = uint16_t(0x0100) + uint16_t(SP);
            return true;
        }
    }

    return false;
}

bool CPU6502::upcomingBusAccessIsWrite() const {
    uint16_t ignoredAddress = 0;
    bool isWrite = false;
    upcomingBusAccess(ignoredAddress, isWrite);
    return isWrite;
}

bool CPU6502::upcomingBusAddress(uint16_t& addr) const {
    bool ignoredWrite = false;
    return upcomingBusAccess(addr, ignoredWrite);
}

void CPU6502::executeMicroCycle() {
    auto mode = lookup[opcode].addrmode;
    const bool finalCycle = (cycles == 1);
    const bool rmw = isReadModifyWriteOp();
    const bool store = isStoreOp();
    auto op = lookup[opcode].operate;

    if (op == &CPU6502::JSR) {
        if (microCycle == 1) {
            addr_abs = read(PC++);
        } else if (microCycle == 2) {
            read(uint16_t(0x0100) + uint16_t(SP));
        } else if (microCycle == 3) {
            write(uint16_t(0x0100) + uint16_t(SP), uint8_t((PC >> 8) & 0xFF));
            SP--;
        } else if (microCycle == 4) {
            write(uint16_t(0x0100) + uint16_t(SP), uint8_t(PC & 0xFF));
            SP--;
        } else if (microCycle == 5) {
            microHi = read(PC++);
            PC = (uint16_t(microHi) << 8) | (addr_abs & 0x00FF);
        }
        return;
    }
    if (op == &CPU6502::RTS) {
        if (microCycle == 1) {
            read(PC);
        } else if (microCycle == 2) {
            read(uint16_t(0x0100) + uint16_t(SP));
        } else if (microCycle == 3) {
            SP++;
            microLo = read(uint16_t(0x0100) + uint16_t(SP));
        } else if (microCycle == 4) {
            SP++;
            microHi = read(uint16_t(0x0100) + uint16_t(SP));
            PC = (uint16_t(microHi) << 8) | microLo;
        } else if (microCycle == 5) {
            read(PC);
            PC++;
        }
        return;
    }
    if (op == &CPU6502::RTI) {
        if (microCycle == 1) {
            read(PC);
        } else if (microCycle == 2) {
            read(uint16_t(0x0100) + uint16_t(SP));
        } else if (microCycle == 3) {
            SP++;
            P = read(uint16_t(0x0100) + uint16_t(SP));
            setFlag(B, false);
            setFlag(U, true);
        } else if (microCycle == 4) {
            SP++;
            microLo = read(uint16_t(0x0100) + uint16_t(SP));
        } else if (microCycle == 5) {
            SP++;
            microHi = read(uint16_t(0x0100) + uint16_t(SP));
            PC = (uint16_t(microHi) << 8) | microLo;
        }
        return;
    }
    if (op == &CPU6502::BRK) {
        if (microCycle == 1) {
            microVector = 0xFFFE;
            read(PC++);
        } else if (microCycle == 2) {
            write(uint16_t(0x0100) + uint16_t(SP), uint8_t((PC >> 8) & 0xFF));
            SP--;
        } else if (microCycle == 3) {
            write(uint16_t(0x0100) + uint16_t(SP), uint8_t(PC & 0xFF));
            SP--;
        } else if (microCycle == 4) {
            setFlag(B, true);
            setFlag(U, true);
            write(uint16_t(0x0100) + uint16_t(SP), P);
            SP--;
            setFlag(B, false);
            setFlag(I, true);
            // BRK, IRQ, and NMI share the vector-fetch tail on the 6502.
            // An NMI sampled by the status-push cycle redirects the BRK to
            // the NMI vector while retaining BRK's already-pushed B flag.
            if (nmiPending) {
                nmiPending = false;
                microVector = 0xFFFA;
            }
        } else if (microCycle == 5) {
            microLo = read(microVector);
        } else if (microCycle == 6) {
            microHi = read(uint16_t(microVector + 1));
            PC = (uint16_t(microHi) << 8) | microLo;
        }
        return;
    }
    if (op == &CPU6502::PLA) {
        if (microCycle == 1) {
            read(PC);
        } else if (microCycle == 2) {
            read(uint16_t(0x0100) + uint16_t(SP));
        } else if (microCycle == 3) {
            SP++;
            A = read(uint16_t(0x0100) + uint16_t(SP));
            setFlag(Z, A == 0);
            setFlag(N, (A & 0x80) != 0);
        }
        return;
    }
    if (op == &CPU6502::PLP) {
        if (microCycle == 1) {
            read(PC);
        } else if (microCycle == 2) {
            read(uint16_t(0x0100) + uint16_t(SP));
        } else if (microCycle == 3) {
            pollInterruptsForNextInstruction();
            SP++;
            P = read(uint16_t(0x0100) + uint16_t(SP));
            setFlag(U, true);
            setFlag(B, false);
        }
        return;
    }

    if (mode == &CPU6502::IMP) {
        if (microCycle == 1) read(PC);
        if (finalCycle) finishMicroInstruction();
        return;
    }
    if (mode == &CPU6502::ACC) {
        if (microCycle == 1) read(PC);
        fetched = A;
        if (finalCycle) finishMicroInstruction();
        return;
    }
    if (mode == &CPU6502::IMM) {
        if (microCycle == 1) {
            addr_abs = PC++;
            fetched = read(addr_abs);
        }
        if (finalCycle) finishMicroInstruction();
        return;
    }
    if (mode == &CPU6502::ZP0) {
        if (microCycle == 1) {
            addr_abs = read(PC++) & 0x00FF;
        } else if (!store && microCycle == 2) {
            fetched = read(addr_abs);
        } else if (rmw && microCycle == 3) {
            write(addr_abs, fetched);
        } else if (store && finalCycle) {
            finishMicroInstruction();
            return;
        }
        if (finalCycle && !store) finishMicroInstruction();
        return;
    }
    if (mode == &CPU6502::ZPX || mode == &CPU6502::ZPY) {
        const uint8_t idx = (mode == &CPU6502::ZPX) ? X : Y;
        if (microCycle == 1) {
            microLo = read(PC++);
        } else if (microCycle == 2) {
            read(microLo);
            addr_abs = uint8_t(microLo + idx);
        } else if (!store && microCycle == 3) {
            fetched = read(addr_abs);
        } else if (rmw && microCycle == 4) {
            write(addr_abs, fetched);
        } else if (store && finalCycle) {
            finishMicroInstruction();
            return;
        }
        if (finalCycle && !store) finishMicroInstruction();
        return;
    }
    if (mode == &CPU6502::ABS || mode == &CPU6502::JSR_ABS) {
        if (mode == &CPU6502::JSR_ABS) {
            if (microCycle == 1) {
                addr_abs = read(PC++);
            } else if (finalCycle) {
                finishMicroInstruction();
            }
            return;
        }
        if (microCycle == 1) {
            microLo = read(PC++);
        } else if (microCycle == 2) {
            microHi = read(PC++);
            addr_abs = (uint16_t(microHi) << 8) | microLo;
        } else if (!store && microCycle == 3) {
            fetched = read(addr_abs);
        } else if (rmw && microCycle == 4) {
            write(addr_abs, fetched);
        } else if (store && finalCycle) {
            finishMicroInstruction();
            return;
        }
        if (finalCycle && !store) finishMicroInstruction();
        return;
    }
    if (mode == &CPU6502::ABX || mode == &CPU6502::ABY) {
        const uint8_t idx = (mode == &CPU6502::ABX) ? X : Y;
        if (microCycle == 1) {
            microLo = read(PC++);
        } else if (microCycle == 2) {
            microHi = read(PC++);
            microBase = (uint16_t(microHi) << 8) | microLo;
            addr_base = microBase;
            addr_abs = microBase + idx;
            microPageCrossed = (addr_abs & 0xFF00) != (microBase & 0xFF00);
            if (microPageCrossed && !needsIndexedDummyRead()) {
                cycles++;
            }
        } else if (microCycle == 3
                   && (microPageCrossed || needsIndexedDummyRead())) {
            // The 6502 performs this intermediate-address read on its own
            // bus cycle.  Keeping it separate from the operand-high fetch is
            // essential for PPUDATA/controller side effects and DMA halts.
            read((microBase & 0xFF00) | (addr_abs & 0x00FF));
        } else if (!store && !rmw && finalCycle) {
            fetched = read(addr_abs);
        } else if (!store && rmw && microCycle == 4) {
            fetched = read(addr_abs);
        } else if (rmw && microCycle == 5) {
            write(addr_abs, fetched);
        } else if (store && finalCycle) {
            finishMicroInstruction();
            return;
        }
        if (finalCycle && !store) finishMicroInstruction();
        return;
    }
    if (mode == &CPU6502::IND) {
        if (microCycle == 1) {
            microLo = read(PC++);
        } else if (microCycle == 2) {
            microHi = read(PC++);
            microBase = (uint16_t(microHi) << 8) | microLo;
        } else if (microCycle == 3) {
            microLo = read(microBase);
        } else if (microCycle == 4) {
            uint16_t hiAddr = (microBase & 0xFF00) | uint8_t(microBase + 1);
            microHi = read(hiAddr);
            addr_abs = (uint16_t(microHi) << 8) | microLo;
        }
        if (finalCycle) finishMicroInstruction();
        return;
    }
    if (mode == &CPU6502::IZX) {
        if (microCycle == 1) {
            microLo = read(PC++);
        } else if (microCycle == 2) {
            read(microLo);
            microLo = uint8_t(microLo + X);
        } else if (microCycle == 3) {
            microBase = read(microLo);
        } else if (microCycle == 4) {
            microHi = read(uint8_t(microLo + 1));
            addr_abs = (uint16_t(microHi) << 8) | microBase;
        } else if (!store && microCycle == 5) {
            fetched = read(addr_abs);
        } else if (rmw && microCycle == 6) {
            write(addr_abs, fetched);
        } else if (store && finalCycle) {
            finishMicroInstruction();
            return;
        }
        if (finalCycle && !store) finishMicroInstruction();
        return;
    }
    if (mode == &CPU6502::IZY) {
        if (microCycle == 1) {
            microLo = read(PC++);
        } else if (microCycle == 2) {
            microBase = read(microLo);
        } else if (microCycle == 3) {
            microHi = read(uint8_t(microLo + 1));
            uint16_t base = (uint16_t(microHi) << 8) | microBase;
            addr_base = base;
            addr_abs = base + Y;
            microPageCrossed = (addr_abs & 0xFF00) != (base & 0xFF00);
            if (microPageCrossed && !store && !rmw) {
                cycles++;
            }
        } else if (microCycle == 4 && (microPageCrossed || store || rmw)) {
            read((addr_base & 0xFF00) | (addr_abs & 0x00FF));
        } else if (!store && !rmw && finalCycle) {
            fetched = read(addr_abs);
        } else if (!store && rmw && microCycle == 5) {
            fetched = read(addr_abs);
        } else if (rmw && microCycle == 6) {
            write(addr_abs, fetched);
        } else if (store && finalCycle) {
            finishMicroInstruction();
            return;
        }
        if (finalCycle && !store) finishMicroInstruction();
        return;
    }
    if (mode == &CPU6502::REL) {
        if (microCycle == 1) {
            addr_rel = read(PC++);
            if (addr_rel & 0x0080) addr_rel |= 0xFF00;
            const uint16_t oldPC = PC;
            bool cond = false;
            if (op == &CPU6502::BCC) cond = !getFlag(C);
            else if (op == &CPU6502::BCS) cond = getFlag(C);
            else if (op == &CPU6502::BEQ) cond = getFlag(Z);
            else if (op == &CPU6502::BMI) cond = getFlag(N);
            else if (op == &CPU6502::BNE) cond = !getFlag(Z);
            else if (op == &CPU6502::BPL) cond = !getFlag(N);
            else if (op == &CPU6502::BVC) cond = !getFlag(V);
            else if (op == &CPU6502::BVS) cond = getFlag(V);
            microBranchTaken = cond;
            if (cond) {
                microBase = uint16_t(int32_t(PC) + int16_t(addr_rel));
                microPageCrossed = (oldPC & 0xFF00) != (microBase & 0xFF00);
                cycles = uint8_t(cycles + 1 + (microPageCrossed ? 1 : 0));
            }
        } else if (microBranchTaken && microCycle == 2) {
            read(PC);
            if (!microPageCrossed) {
                PC = microBase;
            }
        } else if (microBranchTaken && microCycle == 3) {
            read((PC & 0xFF00) | (microBase & 0x00FF));
            PC = microBase;
        }
        return;
    }

    cycles--;
}

// Kept from your initial skeleton (wrappers around real core behavior)
void CPU6502::lda_imm() {
    // LDA #imm
    fetched = read(PC++);
    A = fetched;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
}

void CPU6502::sta_abs() {
    // STA abs
    const uint16_t lo = read(PC++);
    const uint16_t hi = read(PC++);
    const uint16_t addr = (hi << 8) | lo;
    write(addr, A);
}

void CPU6502::inx() {
    X++;
    setFlag(Z, X == 0);
    setFlag(N, (X & 0x80) != 0);
}

void CPU6502::jmp_abs() {
    const uint16_t lo = read(PC++);
    const uint16_t hi = read(PC++);
    PC = (hi << 8) | lo;
}

void CPU6502::nop() {
    // do nothing
}

// -------------------------
// Core instruction runner
// -------------------------

int CPU6502::step() {
    if (!bus) return -1;

    if (!lookupInitialized) {
        initLookup();
        lookupInitialized = true;
    }

    // Maintain legacy single-instruction stepping API by running the
    // cycle-accurate clock until the CPU is ready to start the next instruction.
    int used = 0;
    // If we're mid-instruction, finish it first
    do {
        clock();
        used++;
    } while (!complete());
    return used;
}

// -------------------------
// Stack helpers
// -------------------------

void CPU6502::push(uint8_t v) {
    write(uint16_t(0x0100) + uint16_t(SP), v);
    SP--;
}

uint8_t CPU6502::pop() {
    SP++;
    return read(uint16_t(0x0100) + uint16_t(SP));
}

// -------------------------
// Fetch helper
// -------------------------

uint8_t CPU6502::fetch() {
    if (microFetchCached) {
        return fetched;
    }
    // If implied/accumulator, fetched is either unused or A
    if (lookup[opcode].addrmode != &CPU6502::IMP && lookup[opcode].addrmode != &CPU6502::ACC) {
        fetched = read(addr_abs);
    }
    return fetched;
}

bool CPU6502::isReadModifyWriteOp() const {
    return (lookup[opcode].traits & TRAIT_RMW) != 0;
}

bool CPU6502::isStoreOp() const {
    return (lookup[opcode].traits & TRAIT_STORE) != 0;
}

bool CPU6502::needsIndexedDummyRead() const {
    return isStoreOp() || isReadModifyWriteOp();
}

// -------------------------
// Addressing modes
// Return 1 if page boundary crossed (for some modes), else 0
// -------------------------

uint8_t CPU6502::IMP() {
    fetched = A; // convenient default
    return 0;
}

uint8_t CPU6502::ACC() {
    fetched = A;
    return 0;
}

uint8_t CPU6502::IMM() {
    addr_abs = PC++;
    return 0;
}

uint8_t CPU6502::ZP0() {
    addr_abs = read(PC++);
    addr_abs &= 0x00FF;
    return 0;
}

uint8_t CPU6502::ZPX() {
    const uint16_t base = uint16_t(read(PC++));
    read(base & 0x00FF);
    addr_abs = (base + uint16_t(X)) & 0x00FF;
    return 0;
}

uint8_t CPU6502::ZPY() {
    const uint16_t base = uint16_t(read(PC++));
    read(base & 0x00FF);
    addr_abs = (base + uint16_t(Y)) & 0x00FF;
    return 0;
}

uint8_t CPU6502::ABS() {
    const uint16_t lo = read(PC++);
    const uint16_t hi = read(PC++);
    addr_abs = (hi << 8) | lo;
    return 0;
}

uint8_t CPU6502::JSR_ABS() {
    addr_abs = read(PC++);
    return 0;
}

uint8_t CPU6502::ABX() {
    const uint16_t lo = read(PC++);
    const uint16_t hi = read(PC++);
    const uint16_t base = (hi << 8) | lo;
    addr_base = base;
    addr_abs = base + uint16_t(X);
    const bool crossed = (addr_abs & 0xFF00) != (base & 0xFF00);
    const bool rmw = lookup[opcode].operate == &CPU6502::ASL ||
                     lookup[opcode].operate == &CPU6502::LSR ||
                     lookup[opcode].operate == &CPU6502::ROL ||
                     lookup[opcode].operate == &CPU6502::ROR ||
                     lookup[opcode].operate == &CPU6502::INC ||
                     lookup[opcode].operate == &CPU6502::DEC ||
                     lookup[opcode].operate == &CPU6502::SLO ||
                     lookup[opcode].operate == &CPU6502::RLA ||
                     lookup[opcode].operate == &CPU6502::SRE ||
                     lookup[opcode].operate == &CPU6502::RRA ||
                     lookup[opcode].operate == &CPU6502::DCP ||
                     lookup[opcode].operate == &CPU6502::ISC;
    if (crossed || lookup[opcode].operate == &CPU6502::STA || rmw) {
        read((base & 0xFF00) | (addr_abs & 0x00FF));
    }
    return crossed;
}

uint8_t CPU6502::ABY() {
    const uint16_t lo = read(PC++);
    const uint16_t hi = read(PC++);
    const uint16_t base = (hi << 8) | lo;
    addr_base = base;
    addr_abs = base + uint16_t(Y);
    const bool crossed = (addr_abs & 0xFF00) != (base & 0xFF00);
    const bool rmw = lookup[opcode].operate == &CPU6502::SLO ||
                     lookup[opcode].operate == &CPU6502::RLA ||
                     lookup[opcode].operate == &CPU6502::SRE ||
                     lookup[opcode].operate == &CPU6502::RRA ||
                     lookup[opcode].operate == &CPU6502::DCP ||
                     lookup[opcode].operate == &CPU6502::ISC;
    if (crossed || lookup[opcode].operate == &CPU6502::STA || rmw) {
        read((base & 0xFF00) | (addr_abs & 0x00FF));
    }
    return crossed;
}

uint8_t CPU6502::IND() {
    // 6502 page boundary hardware bug: indirect vector wraps within page
    const uint16_t ptr_lo = read(PC++);
    const uint16_t ptr_hi = read(PC++);
    const uint16_t ptr = (ptr_hi << 8) | ptr_lo;

    const uint16_t lo_addr = ptr;
    const uint16_t hi_addr = (ptr & 0xFF00) | uint16_t((ptr + 1) & 0x00FF);

    const uint16_t lo = read(lo_addr);
    const uint16_t hi = read(hi_addr);
    addr_abs = (hi << 8) | lo;
    return 0;
}

uint8_t CPU6502::IZX() {
    const uint16_t t = (uint16_t(read(PC++)) + uint16_t(X)) & 0x00FF;
    const uint16_t lo = read(t & 0x00FF);
    const uint16_t hi = read((t + 1) & 0x00FF);
    addr_abs = (hi << 8) | lo;
    return 0;
}

uint8_t CPU6502::IZY() {
    const uint16_t t = uint16_t(read(PC++)) & 0x00FF;
    const uint16_t lo = read(t & 0x00FF);
    const uint16_t hi = read((t + 1) & 0x00FF);
    const uint16_t base = (hi << 8) | lo;
    addr_base = base;
    addr_abs = base + uint16_t(Y);
    const bool crossed = (addr_abs & 0xFF00) != (base & 0xFF00);
    if (crossed) {
        read((base & 0xFF00) | (addr_abs & 0x00FF));
    }
    return crossed;
}

uint8_t CPU6502::REL() {
    addr_rel = uint16_t(read(PC++));
    if (addr_rel & 0x0080) addr_rel |= 0xFF00; // sign extend
    return 0;
}

// -------------------------
// Branch helper
// -------------------------

uint8_t CPU6502::branchIf(bool cond) {
    if (!cond) return 0;

    // Branch taken adds 1 cycle, +1 if page crossed
    const uint16_t oldPC = PC;
    const uint16_t target = uint16_t(int32_t(PC) + int16_t(addr_rel));
    read(oldPC);

    uint8_t extra = 1;
    if ((oldPC & 0xFF00) != (target & 0xFF00)) {
        read((oldPC & 0xFF00) | (target & 0x00FF));
        extra++;
    }

    PC = target;
    return extra; // 1 = taken, 2 = taken + page cross
}

// -------------------------
// Operations
// Return 1 if additional cycle required (for some RMW/page-cross cases)
// -------------------------

uint8_t CPU6502::XXX() {
    // Unimplemented/illegal opcode. Do nothing.
    return 0;
}

uint8_t CPU6502::ADC() {
    fetch();
    const uint16_t sum = uint16_t(A) + uint16_t(fetched) + uint16_t(getFlag(C) ? 1 : 0);

    setFlag(C, sum > 0xFF);
    setFlag(Z, (sum & 0x00FF) == 0);
    setFlag(V, (~(uint16_t(A) ^ uint16_t(fetched)) & (uint16_t(A) ^ sum) & 0x0080) != 0);
    setFlag(N, (sum & 0x0080) != 0);

    A = uint8_t(sum & 0x00FF);
    return 1;
}

uint8_t CPU6502::SBC() {
    fetch();
    const uint16_t value = uint16_t(fetched) ^ 0x00FF;
    const uint16_t sum = uint16_t(A) + value + uint16_t(getFlag(C) ? 1 : 0);

    setFlag(C, sum & 0xFF00);
    setFlag(Z, (sum & 0x00FF) == 0);
    setFlag(V, ((sum ^ uint16_t(A)) & (sum ^ value) & 0x0080) != 0);
    setFlag(N, (sum & 0x0080) != 0);

    A = uint8_t(sum & 0x00FF);
    return 1;
}

uint8_t CPU6502::AND() {
    fetch();
    A = A & fetched;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    return 1;
}

uint8_t CPU6502::ORA() {
    fetch();
    A = A | fetched;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    return 1;
}

uint8_t CPU6502::EOR() {
    fetch();
    A = A ^ fetched;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    return 1;
}

uint8_t CPU6502::ASL() {
    fetch();
    const uint16_t res = uint16_t(fetched) << 1;
    setFlag(C, (res & 0xFF00) != 0);
    setFlag(Z, (res & 0x00FF) == 0);
    setFlag(N, (res & 0x0080) != 0);

    if (lookup[opcode].addrmode == &CPU6502::ACC) {
        A = uint8_t(res & 0x00FF);
    } else {
        if (!microFetchCached) write(addr_abs, fetched);
        write(addr_abs, uint8_t(res & 0x00FF));
    }
    return 0;
}

uint8_t CPU6502::LSR() {
    fetch();
    setFlag(C, (fetched & 0x01) != 0);
    const uint16_t res = uint16_t(fetched) >> 1;
    setFlag(Z, (res & 0x00FF) == 0);
    setFlag(N, false);

    if (lookup[opcode].addrmode == &CPU6502::ACC) {
        A = uint8_t(res & 0x00FF);
    } else {
        if (!microFetchCached) write(addr_abs, fetched);
        write(addr_abs, uint8_t(res & 0x00FF));
    }
    return 0;
}

uint8_t CPU6502::ROL() {
    fetch();
    const uint16_t res = (uint16_t(fetched) << 1) | uint16_t(getFlag(C) ? 1 : 0);
    setFlag(C, (res & 0xFF00) != 0);
    setFlag(Z, (res & 0x00FF) == 0);
    setFlag(N, (res & 0x0080) != 0);

    if (lookup[opcode].addrmode == &CPU6502::ACC) {
        A = uint8_t(res & 0x00FF);
    } else {
        if (!microFetchCached) write(addr_abs, fetched);
        write(addr_abs, uint8_t(res & 0x00FF));
    }
    return 0;
}

uint8_t CPU6502::ROR() {
    fetch();
    const uint16_t res = (uint16_t(getFlag(C) ? 1 : 0) << 7) | (uint16_t(fetched) >> 1);
    setFlag(C, (fetched & 0x01) != 0);
    setFlag(Z, (res & 0x00FF) == 0);
    setFlag(N, (res & 0x0080) != 0);

    if (lookup[opcode].addrmode == &CPU6502::ACC) {
        A = uint8_t(res & 0x00FF);
    } else {
        if (!microFetchCached) write(addr_abs, fetched);
        write(addr_abs, uint8_t(res & 0x00FF));
    }
    return 0;
}

uint8_t CPU6502::BIT() {
    fetch();
    const uint8_t r = A & fetched;
    setFlag(Z, r == 0);
    setFlag(V, (fetched & 0x40) != 0);
    setFlag(N, (fetched & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::LDA() {
    fetch();
    A = fetched;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    return 1;
}

uint8_t CPU6502::LDX() {
    fetch();
    X = fetched;
    setFlag(Z, X == 0);
    setFlag(N, (X & 0x80) != 0);
    return 1;
}

uint8_t CPU6502::LDY() {
    fetch();
    Y = fetched;
    setFlag(Z, Y == 0);
    setFlag(N, (Y & 0x80) != 0);
    return 1;
}

uint8_t CPU6502::STA() {
    write(addr_abs, A);
    return 0;
}

uint8_t CPU6502::STX() {
    write(addr_abs, X);
    return 0;
}

uint8_t CPU6502::STY() {
    write(addr_abs, Y);
    return 0;
}

uint8_t CPU6502::CMP() {
    fetch();
    const uint16_t t = uint16_t(A) - uint16_t(fetched);
    setFlag(C, A >= fetched);
    setFlag(Z, (t & 0x00FF) == 0);
    setFlag(N, (t & 0x0080) != 0);
    return 1;
}

uint8_t CPU6502::CPX() {
    fetch();
    const uint16_t t = uint16_t(X) - uint16_t(fetched);
    setFlag(C, X >= fetched);
    setFlag(Z, (t & 0x00FF) == 0);
    setFlag(N, (t & 0x0080) != 0);
    return 0;
}

uint8_t CPU6502::CPY() {
    fetch();
    const uint16_t t = uint16_t(Y) - uint16_t(fetched);
    setFlag(C, Y >= fetched);
    setFlag(Z, (t & 0x00FF) == 0);
    setFlag(N, (t & 0x0080) != 0);
    return 0;
}

uint8_t CPU6502::INC() {
    fetch();
    const uint8_t v = uint8_t(fetched + 1);
    if (!microFetchCached) write(addr_abs, fetched);
    write(addr_abs, v);
    setFlag(Z, v == 0);
    setFlag(N, (v & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::INX_OP() {
    X++;
    setFlag(Z, X == 0);
    setFlag(N, (X & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::INY() {
    Y++;
    setFlag(Z, Y == 0);
    setFlag(N, (Y & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::DEC() {
    fetch();
    const uint8_t v = uint8_t(fetched - 1);
    if (!microFetchCached) write(addr_abs, fetched);
    write(addr_abs, v);
    setFlag(Z, v == 0);
    setFlag(N, (v & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::DEX() {
    X--;
    setFlag(Z, X == 0);
    setFlag(N, (X & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::DEY() {
    Y--;
    setFlag(Z, Y == 0);
    setFlag(N, (Y & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::JMP() {
    PC = addr_abs;
    return 0;
}

uint8_t CPU6502::JSR() {
    // JSR's bus order is unusual: fetch opcode, fetch low operand, dummy-read
    // stack, push return high, push return low, then fetch high operand.
    const uint16_t ret = PC;
    read(uint16_t(0x0100) + uint16_t(SP));
    push(uint8_t((ret >> 8) & 0x00FF));
    push(uint8_t(ret & 0x00FF));
    const uint8_t hi = read(PC++);
    addr_abs = uint16_t(hi) << 8 | (addr_abs & 0x00FF);
    PC = addr_abs;
    return 0;
}

uint8_t CPU6502::RTS() {
    const uint16_t lo = pop();
    const uint16_t hi = pop();
    PC = (hi << 8) | lo;
    PC++;
    return 0;
}

uint8_t CPU6502::RTI() {
    P = pop();
    setFlag(B, false);
    setFlag(U, true);
    const uint16_t lo = pop();
    const uint16_t hi = pop();
    PC = (hi << 8) | lo;
    return 0;
}

uint8_t CPU6502::BRK() {
    PC++;
    setFlag(I, true);

    push(uint8_t((PC >> 8) & 0x00FF));
    push(uint8_t(PC & 0x00FF));

    setFlag(B, true);
    push(P | U);
    setFlag(B, false);

    const uint16_t lo = read(0xFFFE);
    const uint16_t hi = read(0xFFFF);
    PC = (hi << 8) | lo;
    return 0;
}

uint8_t CPU6502::BCC() { return branchIf(!getFlag(C)); }
uint8_t CPU6502::BCS() { return branchIf( getFlag(C)); }
uint8_t CPU6502::BEQ() { return branchIf( getFlag(Z)); }
uint8_t CPU6502::BMI() { return branchIf( getFlag(N)); }
uint8_t CPU6502::BNE() { return branchIf(!getFlag(Z)); }
uint8_t CPU6502::BPL() { return branchIf(!getFlag(N)); }
uint8_t CPU6502::BVC() { return branchIf(!getFlag(V)); }
uint8_t CPU6502::BVS() { return branchIf( getFlag(V)); }

uint8_t CPU6502::CLC() { setFlag(C, false); return 0; }
uint8_t CPU6502::CLD() { setFlag(D, false); return 0; }
uint8_t CPU6502::CLI() { setFlag(I, false); return 0; }
uint8_t CPU6502::CLV() { setFlag(V, false); return 0; }

uint8_t CPU6502::SEC() { setFlag(C, true);  return 0; }
uint8_t CPU6502::SED() { setFlag(D, true);  return 0; }
uint8_t CPU6502::SEI() { setFlag(I, true);  return 0; }

uint8_t CPU6502::NOP_OP() { return 0; }

// -------------------------
// Undocumented opcodes
// -------------------------

uint8_t CPU6502::NOP_ILL() {
    // Many illegal NOPs still perform a read of the operand/addressed byte.
    // Doing a fetch here also enables the usual ABX/ABY page-cross penalty.
    fetch();
    return 1;
}

uint8_t CPU6502::LAX() {
    fetch();
    A = fetched;
    X = fetched;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    return 1;
}

uint8_t CPU6502::SAX() {
    write(addr_abs, A & X);
    return 0;
}

uint8_t CPU6502::DCP() {
    // DEC then CMP
    fetch();
    uint8_t v = uint8_t(fetched - 1);
    if (!microFetchCached) write(addr_abs, fetched);
    write(addr_abs, v);
    const uint16_t t = uint16_t(A) - uint16_t(v);
    setFlag(C, A >= v);
    setFlag(Z, (t & 0x00FF) == 0);
    setFlag(N, (t & 0x0080) != 0);
    return 0;
}

uint8_t CPU6502::ISC() {
    // INC then SBC
    fetch();
    uint8_t v = uint8_t(fetched + 1);
    if (!microFetchCached) write(addr_abs, fetched);
    write(addr_abs, v);
    // SBC with v
    const uint16_t value = uint16_t(v) ^ 0x00FF;
    const uint16_t sum = uint16_t(A) + value + uint16_t(getFlag(C) ? 1 : 0);
    setFlag(C, sum & 0xFF00);
    setFlag(Z, (sum & 0x00FF) == 0);
    setFlag(V, ((sum ^ uint16_t(A)) & (sum ^ value) & 0x0080) != 0);
    setFlag(N, (sum & 0x0080) != 0);
    A = uint8_t(sum & 0x00FF);
    return 0;
}

uint8_t CPU6502::SLO() {
    // ASL then ORA
    fetch();
    const uint16_t res = uint16_t(fetched) << 1;
    setFlag(C, (res & 0xFF00) != 0);
    const uint8_t v = uint8_t(res & 0x00FF);
    if (!microFetchCached) write(addr_abs, fetched);
    write(addr_abs, v);
    A = A | v;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::RLA() {
    // ROL then AND
    fetch();
    const uint16_t res = (uint16_t(fetched) << 1) | uint16_t(getFlag(C) ? 1 : 0);
    setFlag(C, (res & 0xFF00) != 0);
    const uint8_t v = uint8_t(res & 0x00FF);
    if (!microFetchCached) write(addr_abs, fetched);
    write(addr_abs, v);
    A = A & v;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::SRE() {
    // LSR then EOR
    fetch();
    setFlag(C, (fetched & 0x01) != 0);
    const uint8_t v = uint8_t(fetched >> 1);
    if (!microFetchCached) write(addr_abs, fetched);
    write(addr_abs, v);
    A = A ^ v;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::RRA() {
    // ROR then ADC
    fetch();
    const uint16_t res = (uint16_t(getFlag(C) ? 1 : 0) << 7) | (uint16_t(fetched) >> 1);
    setFlag(C, (fetched & 0x01) != 0);
    const uint8_t v = uint8_t(res & 0x00FF);
    if (!microFetchCached) write(addr_abs, fetched);
    write(addr_abs, v);

    const uint16_t sum = uint16_t(A) + uint16_t(v) + uint16_t(getFlag(C) ? 1 : 0);
    setFlag(C, sum > 0xFF);
    setFlag(Z, (sum & 0x00FF) == 0);
    setFlag(V, (~(uint16_t(A) ^ uint16_t(v)) & (uint16_t(A) ^ sum) & 0x0080) != 0);
    setFlag(N, (sum & 0x0080) != 0);
    A = uint8_t(sum & 0x00FF);
    return 0;
}

uint8_t CPU6502::ANC() {
    // AND immediate then carry = bit 7
    fetch();
    A = A & fetched;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    setFlag(C, (A & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::ALR() {
    // AND immediate then LSR A
    fetch();
    A = A & fetched;
    setFlag(C, (A & 0x01) != 0);
    A = uint8_t(A >> 1);
    setFlag(Z, A == 0);
    setFlag(N, false);
    return 0;
}

uint8_t CPU6502::ARR() {
    // AND immediate then ROR A, then special flag behavior
    fetch();
    A = A & fetched;
    const uint8_t oldCarry = getFlag(C) ? 0x80 : 0x00;
    const uint8_t r = uint8_t((oldCarry) | (A >> 1));
    A = r;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    setFlag(C, (A & 0x40) != 0);
    setFlag(V, ((A >> 6) ^ (A >> 5)) & 0x01);
    return 0;
}

uint8_t CPU6502::AXS() {
    // (A & X) - immediate -> X
    fetch();
    const uint16_t t = uint16_t(A & X) - uint16_t(fetched);
    X = uint8_t(t & 0x00FF);
    setFlag(C, (t & 0xFF00) == 0);
    setFlag(Z, X == 0);
    setFlag(N, (X & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::LAS() {
    fetch();
    const uint8_t v = uint8_t(fetched & SP);
    A = v;
    X = v;
    SP = v;
    setFlag(Z, v == 0);
    setFlag(N, (v & 0x80) != 0);
    return 1;
}

uint8_t CPU6502::TAS() {
    const uint8_t v = uint8_t(A & X);
    SP = v;
    const uint8_t hi = uint8_t(((addr_base >> 8) + 1) & 0xFF);
    const bool dmaAdjacent = bus && (bus->dmcDmaHaltedLastRead || bus->dmcDmaTransfer || bus->apu.hasDmcDmaRequest());
    uint16_t target = addr_abs;
    if ((addr_abs & 0xFF00) != (addr_base & 0xFF00)) {
        target = uint16_t((uint16_t(uint8_t((addr_abs >> 8) & X)) << 8) | (addr_abs & 0x00FF));
    }
    write(target, (dmaAdjacent || unstableStoreIgnoreH) ? v : uint8_t(A & (X | 0xF5) & hi));
    return 0;
}

uint8_t CPU6502::AHX() {
    const uint8_t hi = uint8_t(((addr_base >> 8) + 1) & 0xFF);
    const uint8_t v = uint8_t(A & X);
    const bool dmaAdjacent = bus && (bus->dmcDmaHaltedLastRead || bus->dmcDmaTransfer || bus->apu.hasDmcDmaRequest());
    uint16_t target = addr_abs;
    if ((addr_abs & 0xFF00) != (addr_base & 0xFF00)) {
        target = uint16_t((uint16_t(uint8_t((addr_abs >> 8) & X)) << 8) | (addr_abs & 0x00FF));
    }
    write(target, (dmaAdjacent || unstableStoreIgnoreH) ? v : uint8_t(A & (X | 0xF5) & hi));
    return 0;
}

uint8_t CPU6502::SHX() {
    const uint8_t hi = uint8_t(((addr_base >> 8) + 1) & 0xFF);
    const bool dmaAdjacent = bus && (bus->dmcDmaHaltedLastRead || bus->dmcDmaTransfer || bus->apu.hasDmcDmaRequest());
    uint16_t target = addr_abs;
    if ((addr_abs & 0xFF00) != (addr_base & 0xFF00)) {
        target = uint16_t((uint16_t(uint8_t((addr_abs >> 8) & X)) << 8) | (addr_abs & 0x00FF));
    }
    write(target, (dmaAdjacent || unstableStoreIgnoreH) ? X : uint8_t(X & hi));
    return 0;
}

uint8_t CPU6502::SHY() {
    const uint8_t hi = uint8_t(((addr_base >> 8) + 1) & 0xFF);
    const bool dmaAdjacent = bus && (bus->dmcDmaHaltedLastRead || bus->dmcDmaTransfer || bus->apu.hasDmcDmaRequest());
    uint16_t target = addr_abs;
    if ((addr_abs & 0xFF00) != (addr_base & 0xFF00)) {
        target = uint16_t((uint16_t(uint8_t((addr_abs >> 8) & Y)) << 8) | (addr_abs & 0x00FF));
    }
    write(target, (dmaAdjacent || unstableStoreIgnoreH) ? Y : uint8_t(Y & hi));
    return 0;
}

uint8_t CPU6502::XAA() {
    // Unstable; common approximation
    fetch();
    A = X & fetched;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::PHA() { push(A); return 0; }

uint8_t CPU6502::PHP() {
    // When pushing P, B and U are set
    push(P | B | U);
    return 0;
}

uint8_t CPU6502::PLA() {
    A = pop();
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::PLP() {
    P = pop();
    setFlag(U, true);
    setFlag(B, false);
    return 0;
}

uint8_t CPU6502::TAX() {
    X = A;
    setFlag(Z, X == 0);
    setFlag(N, (X & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::TAY() {
    Y = A;
    setFlag(Z, Y == 0);
    setFlag(N, (Y & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::TSX() {
    X = SP;
    setFlag(Z, X == 0);
    setFlag(N, (X & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::TXA() {
    A = X;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    return 0;
}

uint8_t CPU6502::TXS() {
    SP = X;
    return 0;
}

uint8_t CPU6502::TYA() {
    A = Y;
    setFlag(Z, A == 0);
    setFlag(N, (A & 0x80) != 0);
    return 0;
}

// -------------------------
// Opcode table
// -------------------------

void CPU6502::initLookup() {
    // Default everything to an "illegal" placeholder.
    // Any opcode not explicitly assigned below will execute XXX (no-op) with implied addressing.
    for (auto& e : lookup) e = {"???", &CPU6502::XXX, &CPU6502::IMP, 2};

    auto S = [&](uint8_t op, const char* n, uint8_t (CPU6502::*f)(), uint8_t (CPU6502::*a)(), uint8_t c) {
        lookup[op] = { n, f, a, c };
    };

    // =========================================================
    // OFFICIAL 6502 OPCODES (NES CPU)
    // =========================================================

    // 0x00
    S(0x00, "BRK", &CPU6502::BRK, &CPU6502::IMP, 7);
    S(0x01, "ORA", &CPU6502::ORA, &CPU6502::IZX, 6);
    S(0x05, "ORA", &CPU6502::ORA, &CPU6502::ZP0, 3);
    S(0x06, "ASL", &CPU6502::ASL, &CPU6502::ZP0, 5);
    S(0x08, "PHP", &CPU6502::PHP, &CPU6502::IMP, 3);
    S(0x09, "ORA", &CPU6502::ORA, &CPU6502::IMM, 2);
    S(0x0A, "ASL", &CPU6502::ASL, &CPU6502::ACC, 2);
    S(0x0D, "ORA", &CPU6502::ORA, &CPU6502::ABS, 4);
    S(0x0E, "ASL", &CPU6502::ASL, &CPU6502::ABS, 6);

    // 0x10
    S(0x10, "BPL", &CPU6502::BPL, &CPU6502::REL, 2);
    S(0x11, "ORA", &CPU6502::ORA, &CPU6502::IZY, 5);
    S(0x15, "ORA", &CPU6502::ORA, &CPU6502::ZPX, 4);
    S(0x16, "ASL", &CPU6502::ASL, &CPU6502::ZPX, 6);
    S(0x18, "CLC", &CPU6502::CLC, &CPU6502::IMP, 2);
    S(0x19, "ORA", &CPU6502::ORA, &CPU6502::ABY, 4);
    S(0x1D, "ORA", &CPU6502::ORA, &CPU6502::ABX, 4);
    S(0x1E, "ASL", &CPU6502::ASL, &CPU6502::ABX, 7);

    // 0x20
    S(0x20, "JSR", &CPU6502::JSR, &CPU6502::JSR_ABS, 6);
    S(0x21, "AND", &CPU6502::AND, &CPU6502::IZX, 6);
    S(0x24, "BIT", &CPU6502::BIT, &CPU6502::ZP0, 3);
    S(0x25, "AND", &CPU6502::AND, &CPU6502::ZP0, 3);
    S(0x26, "ROL", &CPU6502::ROL, &CPU6502::ZP0, 5);
    S(0x28, "PLP", &CPU6502::PLP, &CPU6502::IMP, 4);
    S(0x29, "AND", &CPU6502::AND, &CPU6502::IMM, 2);
    S(0x2A, "ROL", &CPU6502::ROL, &CPU6502::ACC, 2);
    S(0x2C, "BIT", &CPU6502::BIT, &CPU6502::ABS, 4);
    S(0x2D, "AND", &CPU6502::AND, &CPU6502::ABS, 4);
    S(0x2E, "ROL", &CPU6502::ROL, &CPU6502::ABS, 6);

    // 0x30
    S(0x30, "BMI", &CPU6502::BMI, &CPU6502::REL, 2);
    S(0x31, "AND", &CPU6502::AND, &CPU6502::IZY, 5);
    S(0x35, "AND", &CPU6502::AND, &CPU6502::ZPX, 4);
    S(0x36, "ROL", &CPU6502::ROL, &CPU6502::ZPX, 6);
    S(0x38, "SEC", &CPU6502::SEC, &CPU6502::IMP, 2);
    S(0x39, "AND", &CPU6502::AND, &CPU6502::ABY, 4);
    S(0x3D, "AND", &CPU6502::AND, &CPU6502::ABX, 4);
    S(0x3E, "ROL", &CPU6502::ROL, &CPU6502::ABX, 7);

    // 0x40
    S(0x40, "RTI", &CPU6502::RTI, &CPU6502::IMP, 6);
    S(0x41, "EOR", &CPU6502::EOR, &CPU6502::IZX, 6);
    S(0x45, "EOR", &CPU6502::EOR, &CPU6502::ZP0, 3);
    S(0x46, "LSR", &CPU6502::LSR, &CPU6502::ZP0, 5);
    S(0x48, "PHA", &CPU6502::PHA, &CPU6502::IMP, 3);
    S(0x49, "EOR", &CPU6502::EOR, &CPU6502::IMM, 2);
    S(0x4A, "LSR", &CPU6502::LSR, &CPU6502::ACC, 2);
    S(0x4C, "JMP", &CPU6502::JMP, &CPU6502::ABS, 3);
    S(0x4D, "EOR", &CPU6502::EOR, &CPU6502::ABS, 4);
    S(0x4E, "LSR", &CPU6502::LSR, &CPU6502::ABS, 6);

    // 0x50
    S(0x50, "BVC", &CPU6502::BVC, &CPU6502::REL, 2);
    S(0x51, "EOR", &CPU6502::EOR, &CPU6502::IZY, 5);
    S(0x55, "EOR", &CPU6502::EOR, &CPU6502::ZPX, 4);
    S(0x56, "LSR", &CPU6502::LSR, &CPU6502::ZPX, 6);
    S(0x58, "CLI", &CPU6502::CLI, &CPU6502::IMP, 2);
    S(0x59, "EOR", &CPU6502::EOR, &CPU6502::ABY, 4);
    S(0x5D, "EOR", &CPU6502::EOR, &CPU6502::ABX, 4);
    S(0x5E, "LSR", &CPU6502::LSR, &CPU6502::ABX, 7);

    // 0x60
    S(0x60, "RTS", &CPU6502::RTS, &CPU6502::IMP, 6);
    S(0x61, "ADC", &CPU6502::ADC, &CPU6502::IZX, 6);
    S(0x65, "ADC", &CPU6502::ADC, &CPU6502::ZP0, 3);
    S(0x66, "ROR", &CPU6502::ROR, &CPU6502::ZP0, 5);
    S(0x68, "PLA", &CPU6502::PLA, &CPU6502::IMP, 4);
    S(0x69, "ADC", &CPU6502::ADC, &CPU6502::IMM, 2);
    S(0x6A, "ROR", &CPU6502::ROR, &CPU6502::ACC, 2);
    S(0x6C, "JMP", &CPU6502::JMP, &CPU6502::IND, 5);
    S(0x6D, "ADC", &CPU6502::ADC, &CPU6502::ABS, 4);
    S(0x6E, "ROR", &CPU6502::ROR, &CPU6502::ABS, 6);

    // 0x70
    S(0x70, "BVS", &CPU6502::BVS, &CPU6502::REL, 2);
    S(0x71, "ADC", &CPU6502::ADC, &CPU6502::IZY, 5);
    S(0x75, "ADC", &CPU6502::ADC, &CPU6502::ZPX, 4);
    S(0x76, "ROR", &CPU6502::ROR, &CPU6502::ZPX, 6);
    S(0x78, "SEI", &CPU6502::SEI, &CPU6502::IMP, 2);
    S(0x79, "ADC", &CPU6502::ADC, &CPU6502::ABY, 4);
    S(0x7D, "ADC", &CPU6502::ADC, &CPU6502::ABX, 4);
    S(0x7E, "ROR", &CPU6502::ROR, &CPU6502::ABX, 7);

    // 0x80
    S(0x81, "STA", &CPU6502::STA, &CPU6502::IZX, 6);
    S(0x84, "STY", &CPU6502::STY, &CPU6502::ZP0, 3);
    S(0x85, "STA", &CPU6502::STA, &CPU6502::ZP0, 3);
    S(0x86, "STX", &CPU6502::STX, &CPU6502::ZP0, 3);
    S(0x88, "DEY", &CPU6502::DEY, &CPU6502::IMP, 2);
    S(0x8A, "TXA", &CPU6502::TXA, &CPU6502::IMP, 2);
    S(0x8C, "STY", &CPU6502::STY, &CPU6502::ABS, 4);
    S(0x8D, "STA", &CPU6502::STA, &CPU6502::ABS, 4);
    S(0x8E, "STX", &CPU6502::STX, &CPU6502::ABS, 4);

    // 0x90
    S(0x90, "BCC", &CPU6502::BCC, &CPU6502::REL, 2);
    S(0x91, "STA", &CPU6502::STA, &CPU6502::IZY, 6);
    S(0x94, "STY", &CPU6502::STY, &CPU6502::ZPX, 4);
    S(0x95, "STA", &CPU6502::STA, &CPU6502::ZPX, 4);
    S(0x96, "STX", &CPU6502::STX, &CPU6502::ZPY, 4);
    S(0x98, "TYA", &CPU6502::TYA, &CPU6502::IMP, 2);
    S(0x99, "STA", &CPU6502::STA, &CPU6502::ABY, 5);
    S(0x9A, "TXS", &CPU6502::TXS, &CPU6502::IMP, 2);
    S(0x9D, "STA", &CPU6502::STA, &CPU6502::ABX, 5);

    // 0xA0
    S(0xA0, "LDY", &CPU6502::LDY, &CPU6502::IMM, 2);
    S(0xA1, "LDA", &CPU6502::LDA, &CPU6502::IZX, 6);
    S(0xA2, "LDX", &CPU6502::LDX, &CPU6502::IMM, 2);
    S(0xA4, "LDY", &CPU6502::LDY, &CPU6502::ZP0, 3);
    S(0xA5, "LDA", &CPU6502::LDA, &CPU6502::ZP0, 3);
    S(0xA6, "LDX", &CPU6502::LDX, &CPU6502::ZP0, 3);
    S(0xA8, "TAY", &CPU6502::TAY, &CPU6502::IMP, 2);
    S(0xA9, "LDA", &CPU6502::LDA, &CPU6502::IMM, 2);
    S(0xAA, "TAX", &CPU6502::TAX, &CPU6502::IMP, 2);
    S(0xAC, "LDY", &CPU6502::LDY, &CPU6502::ABS, 4);
    S(0xAD, "LDA", &CPU6502::LDA, &CPU6502::ABS, 4);
    S(0xAE, "LDX", &CPU6502::LDX, &CPU6502::ABS, 4);

    // 0xB0
    S(0xB0, "BCS", &CPU6502::BCS, &CPU6502::REL, 2);
    S(0xB1, "LDA", &CPU6502::LDA, &CPU6502::IZY, 5);
    S(0xB4, "LDY", &CPU6502::LDY, &CPU6502::ZPX, 4);
    S(0xB5, "LDA", &CPU6502::LDA, &CPU6502::ZPX, 4);
    S(0xB6, "LDX", &CPU6502::LDX, &CPU6502::ZPY, 4);
    S(0xB8, "CLV", &CPU6502::CLV, &CPU6502::IMP, 2);
    S(0xB9, "LDA", &CPU6502::LDA, &CPU6502::ABY, 4);
    S(0xBA, "TSX", &CPU6502::TSX, &CPU6502::IMP, 2);
    S(0xBC, "LDY", &CPU6502::LDY, &CPU6502::ABX, 4);
    S(0xBD, "LDA", &CPU6502::LDA, &CPU6502::ABX, 4);
    S(0xBE, "LDX", &CPU6502::LDX, &CPU6502::ABY, 4);

    // 0xC0
    S(0xC0, "CPY", &CPU6502::CPY, &CPU6502::IMM, 2);
    S(0xC1, "CMP", &CPU6502::CMP, &CPU6502::IZX, 6);
    S(0xC4, "CPY", &CPU6502::CPY, &CPU6502::ZP0, 3);
    S(0xC5, "CMP", &CPU6502::CMP, &CPU6502::ZP0, 3);
    S(0xC6, "DEC", &CPU6502::DEC, &CPU6502::ZP0, 5);
    S(0xC8, "INY", &CPU6502::INY, &CPU6502::IMP, 2);
    S(0xC9, "CMP", &CPU6502::CMP, &CPU6502::IMM, 2);
    S(0xCA, "DEX", &CPU6502::DEX, &CPU6502::IMP, 2);
    S(0xCC, "CPY", &CPU6502::CPY, &CPU6502::ABS, 4);
    S(0xCD, "CMP", &CPU6502::CMP, &CPU6502::ABS, 4);
    S(0xCE, "DEC", &CPU6502::DEC, &CPU6502::ABS, 6);

    // 0xD0
    S(0xD0, "BNE", &CPU6502::BNE, &CPU6502::REL, 2);
    S(0xD1, "CMP", &CPU6502::CMP, &CPU6502::IZY, 5);
    S(0xD5, "CMP", &CPU6502::CMP, &CPU6502::ZPX, 4);
    S(0xD6, "DEC", &CPU6502::DEC, &CPU6502::ZPX, 6);
    S(0xD8, "CLD", &CPU6502::CLD, &CPU6502::IMP, 2);
    S(0xD9, "CMP", &CPU6502::CMP, &CPU6502::ABY, 4);
    S(0xDD, "CMP", &CPU6502::CMP, &CPU6502::ABX, 4);
    S(0xDE, "DEC", &CPU6502::DEC, &CPU6502::ABX, 7);

    // 0xE0
    S(0xE0, "CPX", &CPU6502::CPX, &CPU6502::IMM, 2);
    S(0xE1, "SBC", &CPU6502::SBC, &CPU6502::IZX, 6);
    S(0xE4, "CPX", &CPU6502::CPX, &CPU6502::ZP0, 3);
    S(0xE5, "SBC", &CPU6502::SBC, &CPU6502::ZP0, 3);
    S(0xE6, "INC", &CPU6502::INC, &CPU6502::ZP0, 5);
    S(0xE8, "INX", &CPU6502::INX_OP, &CPU6502::IMP, 2);
    S(0xE9, "SBC", &CPU6502::SBC, &CPU6502::IMM, 2);
    S(0xEA, "NOP", &CPU6502::NOP_OP, &CPU6502::IMP, 2);
    S(0xEC, "CPX", &CPU6502::CPX, &CPU6502::ABS, 4);
    S(0xED, "SBC", &CPU6502::SBC, &CPU6502::ABS, 4);
    S(0xEE, "INC", &CPU6502::INC, &CPU6502::ABS, 6);

    // 0xF0
    S(0xF0, "BEQ", &CPU6502::BEQ, &CPU6502::REL, 2);
    S(0xF1, "SBC", &CPU6502::SBC, &CPU6502::IZY, 5);
    S(0xF5, "SBC", &CPU6502::SBC, &CPU6502::ZPX, 4);
    S(0xF6, "INC", &CPU6502::INC, &CPU6502::ZPX, 6);
    S(0xF8, "SED", &CPU6502::SED, &CPU6502::IMP, 2);
    S(0xF9, "SBC", &CPU6502::SBC, &CPU6502::ABY, 4);
    S(0xFD, "SBC", &CPU6502::SBC, &CPU6502::ABX, 4);
    S(0xFE, "INC", &CPU6502::INC, &CPU6502::ABX, 7);

    // NOTE: Some opcodes above are intentionally absent because they are not valid official 6502 opcodes
    // (e.g. 0x02, 0x12, etc.). These remain as XXX unless mapped in the illegal section below.

    // =========================================================
    // ILLEGAL / UNDOCUMENTED (commonly emulated; many NES titles tolerate them)
    // =========================================================

    // "KIL" / "JAM" opcodes (halt CPU on real 6502). We treat as XXX.
    for (uint8_t op : {0x02,0x12,0x22,0x32,0x42,0x52,0x62,0x72,0x92,0xB2,0xD2,0xF2})
        S(op, "KIL", &CPU6502::XXX, &CPU6502::IMP, 2);

    // NOP variants
    S(0x04, "NOP", &CPU6502::NOP_ILL, &CPU6502::ZP0, 3);
    S(0x0C, "NOP", &CPU6502::NOP_ILL, &CPU6502::ABS, 4);
    S(0x14, "NOP", &CPU6502::NOP_ILL, &CPU6502::ZPX, 4);
    S(0x1A, "NOP", &CPU6502::NOP_OP,  &CPU6502::IMP, 2);
    S(0x1C, "NOP", &CPU6502::NOP_ILL, &CPU6502::ABX, 4);
    S(0x34, "NOP", &CPU6502::NOP_ILL, &CPU6502::ZPX, 4);
    S(0x3A, "NOP", &CPU6502::NOP_OP,  &CPU6502::IMP, 2);
    S(0x3C, "NOP", &CPU6502::NOP_ILL, &CPU6502::ABX, 4);
    S(0x44, "NOP", &CPU6502::NOP_ILL, &CPU6502::ZP0, 3);
    S(0x54, "NOP", &CPU6502::NOP_ILL, &CPU6502::ZPX, 4);
    S(0x5A, "NOP", &CPU6502::NOP_OP,  &CPU6502::IMP, 2);
    S(0x5C, "NOP", &CPU6502::NOP_ILL, &CPU6502::ABX, 4);
    S(0x64, "NOP", &CPU6502::NOP_ILL, &CPU6502::ZP0, 3);
    S(0x74, "NOP", &CPU6502::NOP_ILL, &CPU6502::ZPX, 4);
    S(0x7A, "NOP", &CPU6502::NOP_OP,  &CPU6502::IMP, 2);
    S(0x7C, "NOP", &CPU6502::NOP_ILL, &CPU6502::ABX, 4);
    S(0x80, "NOP", &CPU6502::NOP_ILL, &CPU6502::IMM, 2);
    S(0x82, "NOP", &CPU6502::NOP_ILL, &CPU6502::IMM, 2);
    S(0x89, "NOP", &CPU6502::NOP_ILL, &CPU6502::IMM, 2);
    S(0xC2, "NOP", &CPU6502::NOP_ILL, &CPU6502::IMM, 2);
    S(0xD4, "NOP", &CPU6502::NOP_ILL, &CPU6502::ZPX, 4);
    S(0xDA, "NOP", &CPU6502::NOP_OP,  &CPU6502::IMP, 2);
    S(0xDC, "NOP", &CPU6502::NOP_ILL, &CPU6502::ABX, 4);
    S(0xE2, "NOP", &CPU6502::NOP_ILL, &CPU6502::IMM, 2);
    S(0xF4, "NOP", &CPU6502::NOP_ILL, &CPU6502::ZPX, 4);
    S(0xFA, "NOP", &CPU6502::NOP_OP,  &CPU6502::IMP, 2);
    S(0xFC, "NOP", &CPU6502::NOP_ILL, &CPU6502::ABX, 4);

    // SBC #imm (undocumented, but very common)
    S(0xEB, "SBC", &CPU6502::SBC, &CPU6502::IMM, 2);

    // LAX
    S(0xA3, "LAX", &CPU6502::LAX, &CPU6502::IZX, 6);
    S(0xA7, "LAX", &CPU6502::LAX, &CPU6502::ZP0, 3);
    S(0xAB, "LAX", &CPU6502::LAX, &CPU6502::IMM, 2);
    S(0xAF, "LAX", &CPU6502::LAX, &CPU6502::ABS, 4);
    S(0xB3, "LAX", &CPU6502::LAX, &CPU6502::IZY, 5);
    S(0xB7, "LAX", &CPU6502::LAX, &CPU6502::ZPY, 4);
    S(0xBF, "LAX", &CPU6502::LAX, &CPU6502::ABY, 4);

    // SAX
    S(0x83, "SAX", &CPU6502::SAX, &CPU6502::IZX, 6);
    S(0x87, "SAX", &CPU6502::SAX, &CPU6502::ZP0, 3);
    S(0x8F, "SAX", &CPU6502::SAX, &CPU6502::ABS, 4);
    S(0x97, "SAX", &CPU6502::SAX, &CPU6502::ZPY, 4);

    // DCP
    S(0xC3, "DCP", &CPU6502::DCP, &CPU6502::IZX, 8);
    S(0xC7, "DCP", &CPU6502::DCP, &CPU6502::ZP0, 5);
    S(0xCF, "DCP", &CPU6502::DCP, &CPU6502::ABS, 6);
    S(0xD3, "DCP", &CPU6502::DCP, &CPU6502::IZY, 8);
    S(0xD7, "DCP", &CPU6502::DCP, &CPU6502::ZPX, 6);
    S(0xDB, "DCP", &CPU6502::DCP, &CPU6502::ABY, 7);
    S(0xDF, "DCP", &CPU6502::DCP, &CPU6502::ABX, 7);

    // ISC
    S(0xE3, "ISC", &CPU6502::ISC, &CPU6502::IZX, 8);
    S(0xE7, "ISC", &CPU6502::ISC, &CPU6502::ZP0, 5);
    S(0xEF, "ISC", &CPU6502::ISC, &CPU6502::ABS, 6);
    S(0xF3, "ISC", &CPU6502::ISC, &CPU6502::IZY, 8);
    S(0xF7, "ISC", &CPU6502::ISC, &CPU6502::ZPX, 6);
    S(0xFB, "ISC", &CPU6502::ISC, &CPU6502::ABY, 7);
    S(0xFF, "ISC", &CPU6502::ISC, &CPU6502::ABX, 7);

    // SLO
    S(0x03, "SLO", &CPU6502::SLO, &CPU6502::IZX, 8);
    S(0x07, "SLO", &CPU6502::SLO, &CPU6502::ZP0, 5);
    S(0x0F, "SLO", &CPU6502::SLO, &CPU6502::ABS, 6);
    S(0x13, "SLO", &CPU6502::SLO, &CPU6502::IZY, 8);
    S(0x17, "SLO", &CPU6502::SLO, &CPU6502::ZPX, 6);
    S(0x1B, "SLO", &CPU6502::SLO, &CPU6502::ABY, 7);
    S(0x1F, "SLO", &CPU6502::SLO, &CPU6502::ABX, 7);

    // RLA
    S(0x23, "RLA", &CPU6502::RLA, &CPU6502::IZX, 8);
    S(0x27, "RLA", &CPU6502::RLA, &CPU6502::ZP0, 5);
    S(0x2F, "RLA", &CPU6502::RLA, &CPU6502::ABS, 6);
    S(0x33, "RLA", &CPU6502::RLA, &CPU6502::IZY, 8);
    S(0x37, "RLA", &CPU6502::RLA, &CPU6502::ZPX, 6);
    S(0x3B, "RLA", &CPU6502::RLA, &CPU6502::ABY, 7);
    S(0x3F, "RLA", &CPU6502::RLA, &CPU6502::ABX, 7);

    // SRE
    S(0x43, "SRE", &CPU6502::SRE, &CPU6502::IZX, 8);
    S(0x47, "SRE", &CPU6502::SRE, &CPU6502::ZP0, 5);
    S(0x4F, "SRE", &CPU6502::SRE, &CPU6502::ABS, 6);
    S(0x53, "SRE", &CPU6502::SRE, &CPU6502::IZY, 8);
    S(0x57, "SRE", &CPU6502::SRE, &CPU6502::ZPX, 6);
    S(0x5B, "SRE", &CPU6502::SRE, &CPU6502::ABY, 7);
    S(0x5F, "SRE", &CPU6502::SRE, &CPU6502::ABX, 7);

    // RRA
    S(0x63, "RRA", &CPU6502::RRA, &CPU6502::IZX, 8);
    S(0x67, "RRA", &CPU6502::RRA, &CPU6502::ZP0, 5);
    S(0x6F, "RRA", &CPU6502::RRA, &CPU6502::ABS, 6);
    S(0x73, "RRA", &CPU6502::RRA, &CPU6502::IZY, 8);
    S(0x77, "RRA", &CPU6502::RRA, &CPU6502::ZPX, 6);
    S(0x7B, "RRA", &CPU6502::RRA, &CPU6502::ABY, 7);
    S(0x7F, "RRA", &CPU6502::RRA, &CPU6502::ABX, 7);

    // Immediate oddballs
    S(0x0B, "ANC", &CPU6502::ANC, &CPU6502::IMM, 2);
    S(0x2B, "ANC", &CPU6502::ANC, &CPU6502::IMM, 2);
    S(0x4B, "ALR", &CPU6502::ALR, &CPU6502::IMM, 2);
    S(0x6B, "ARR", &CPU6502::ARR, &CPU6502::IMM, 2);
    S(0x8B, "XAA", &CPU6502::XAA, &CPU6502::IMM, 2);
    S(0xCB, "AXS", &CPU6502::AXS, &CPU6502::IMM, 2);

    // Store/stack oddballs
    S(0x93, "AHX", &CPU6502::AHX, &CPU6502::IZY, 6);
    S(0x9B, "TAS", &CPU6502::TAS, &CPU6502::ABY, 5);
    S(0x9C, "SHY", &CPU6502::SHY, &CPU6502::ABX, 5);
    S(0x9E, "SHX", &CPU6502::SHX, &CPU6502::ABY, 5);
    S(0x9F, "AHX", &CPU6502::AHX, &CPU6502::ABY, 5);
    S(0xBB, "LAS", &CPU6502::LAS, &CPU6502::ABY, 4);

    // Precompute immutable instruction classes once. These flags replace
    // dozens of member-function-pointer comparisons on every CPU bus cycle
    // while preserving the exact same microcycle decisions.
    for (size_t i = 0; i < lookup.size(); ++i) {
        INSTRUCTION& entry = lookup[i];
        const auto op = entry.operate;
        if (op == &CPU6502::ASL || op == &CPU6502::LSR ||
            op == &CPU6502::ROL || op == &CPU6502::ROR ||
            op == &CPU6502::INC || op == &CPU6502::DEC ||
            op == &CPU6502::SLO || op == &CPU6502::RLA ||
            op == &CPU6502::SRE || op == &CPU6502::RRA ||
            op == &CPU6502::DCP || op == &CPU6502::ISC) {
            entry.traits |= TRAIT_RMW;
        }
        if (op == &CPU6502::STA || op == &CPU6502::STX ||
            op == &CPU6502::STY || op == &CPU6502::SAX ||
            op == &CPU6502::TAS || op == &CPU6502::AHX ||
            op == &CPU6502::SHX || op == &CPU6502::SHY) {
            entry.traits |= TRAIT_STORE;
        }
    }
}

// ------------------------------
// Save-state support
// ------------------------------
void CPU6502::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, A);
    write_u8(out, X);
    write_u8(out, Y);
    write_u8(out, SP);
    write_u8(out, P);
    write_u16(out, PC);

    write_u8(out, fetched);
    write_u16(out, addr_abs);
    write_u16(out, addr_rel);
    write_u8(out, opcode);
    write_u8(out, cycles);
    write_u8(out, microCycle);
    write_u8(out, microLo);
    write_u8(out, microHi);
    write_u8(out, microData);
    write_u16(out, microBase);
    write_bool(out, microPageCrossed);
    write_bool(out, microFetchCached);
    write_bool(out, microBranchTaken);
    write_bool(out, unstableStoreIgnoreH);
    write_bool(out, microInterrupt);
    write_u16(out, microVector);

    write_bool(out, irqPending);
    write_bool(out, nmiPending);

    // lookupInitialized is derived; keep it for safety
    write_bool(out, lookupInitialized);
    write_bool(out, interruptArmed);
    write_u16(out, interruptArmedVector);
}

bool CPU6502::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p,end,A)) return false;
    if (!read_u8(p,end,X)) return false;
    if (!read_u8(p,end,Y)) return false;
    if (!read_u8(p,end,SP)) return false;
    if (!read_u8(p,end,P)) return false;
    if (!read_u16(p,end,PC)) return false;

    if (!read_u8(p,end,fetched)) return false;
    if (!read_u16(p,end,addr_abs)) return false;
    if (!read_u16(p,end,addr_rel)) return false;
    if (!read_u8(p,end,opcode)) return false;
    if (!read_u8(p,end,cycles)) return false;
    if (p < end) {
        if (!read_u8(p,end,microCycle)) return false;
        if (!read_u8(p,end,microLo)) return false;
        if (!read_u8(p,end,microHi)) return false;
        if (!read_u8(p,end,microData)) return false;
        if (!read_u16(p,end,microBase)) return false;
        if (!read_bool(p,end,microPageCrossed)) return false;
        if (!read_bool(p,end,microFetchCached)) return false;
        if (!read_bool(p,end,microBranchTaken)) return false;
        if (!read_bool(p,end,unstableStoreIgnoreH)) return false;
        if (p < end) {
            if (!read_bool(p,end,microInterrupt)) return false;
            if (!read_u16(p,end,microVector)) return false;
        } else {
            microInterrupt = false;
            microVector = 0;
        }
    } else {
        microCycle = 0;
        microLo = microHi = microData = 0;
        microBase = 0;
        microPageCrossed = false;
        microFetchCached = false;
        microBranchTaken = false;
        unstableStoreIgnoreH = false;
        microInterrupt = false;
        microVector = 0;
    }

    if (!read_bool(p,end,irqPending)) return false;
    if (!read_bool(p,end,nmiPending)) return false;

    if (!read_bool(p,end,lookupInitialized)) return false;
    if (p < end) {
        if (!read_bool(p,end,interruptArmed)) return false;
        if (!read_u16(p,end,interruptArmedVector)) return false;
    } else {
        interruptArmed = false;
        interruptArmedVector = 0;
    }
    if (!lookupInitialized) {
        initLookup();
        lookupInitialized = true;
    } else if (!lookupInitialized) {
        initLookup();
        lookupInitialized = true;
    }
    return true;
}
