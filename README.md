# ARMv8-Simulator

A simulator for a simplified AArch64 (ARMv8) instruction set, written in C++. A script is parsed into instructions once, then run through a fetch-execute loop over a register file and byte-addressable memory.

## How it works

- **Registers**: named, 64-bit values (e.g. `X0`, `X1`, `SP`) — any name works, there's no fixed X0-X30 range enforced yet.
- **Memory**: a byte-addressable array, read/written 8 bytes at a time.
- **Program counter**: advances one instruction at a time; branches jump by resolving labels to instruction indices at parse time.
- **Flags**: `CMP` sets zero/carry flags, which conditional branches (`B.LT`, `B.GT`) check.
- **Syntax**: operands must be comma-separated like real ARMv8 assembly (e.g. `MOV X0, X1`, not `MOV X0 X1`) — this is enforced, not just cosmetic.

## Supported instructions

| Instruction | Example | Description |
|---|---|---|
| `MOV` | `MOV X0, X1` / `MOV X0, #5` | Copy a register or immediate into a register |
| `MOVZ` | `MOVZ X0, #4386` | Set a register to an immediate |
| `MOVK` | `MOVK X0, #13124, LSL #16` | Set one 16-bit lane of a register, leaving the rest untouched |
| `ADD` / `SUB` | `ADD X0, X1, X2` / `ADD X0, X1, LSL #3` | Add/subtract two registers or an immediate, with an optional shift |
| `LDR` / `STR` | `LDR X0, [SP, #4]` | Load/store a register to memory, with register or immediate addressing |
| `CMP` | `CMP X0, X1` | Compare two values and set flags |
| `B` / `B.cond` | `B start` / `B.LT loop` | Unconditional or conditional branch to a label |
| `CBZ` | `CBZ X0, end` | Branch to a label if a register is zero |
| `PRINT` | `PRINT X0` | Print a register's value |
| `DUMP` | `DUMP X0` | Print 8 bytes of memory starting at an address |
| `HLT` | `HLT` | Stop execution |

`PRINT` and `DUMP` aren't real ARMv8 instructions — they're debug utilities added so a script can print register/memory state while testing, without needing an external debugger.

## Build & run

```
g++ -std=c++17 ARMv8Simulator.cpp -o ARMv8Simulator
./ARMv8Simulator
```

The program currently loads and runs `script2.txt` from the working directory.

## Example script

```
MOVZ X0, #10
loop:
    CBZ X0, end
    PRINT X0
    SUB X0, X0, #1
    B loop
end:
    HLT
```

Counts down from 10 to 1, printing each value.

## Some constraints

- Registers aren't limited to a real X0-X30 range — any name is accepted, so a typo'd register name just silently creates a new one instead of erroring.
- Memory is only 64 bytes, and reads/writes are fixed at 8 bytes — there's no W-register equivalent, or narrower loads/stores like `LDRB`/`LDRH`. Out-of-bounds accesses fail with a clean error rather than corrupting memory.
- Only `B.LT` and `B.GT` exist as conditional branches; any other `B.cond` suffix (e.g. `B.EQ`) currently falls back to an unconditional branch instead of checking flags.
- Arithmetic doesn't track overflow — `ADD`/`SUB` just wrap on overflow like normal unsigned integers, with no flag set.
- The script file is hardcoded in `main()` (currently `script2.txt`) rather than taken as a command-line argument.
- A missing or malformed operand now fails with a clean error instead of crashing, but parsing is still minimal overall — not every malformed line is caught.
- `MOVK`'s second operand is currently allowed to be a register (e.g. `MOVK X0, X1`), but real ARMv8 `MOVK` only ever takes an immediate — this should be restricted to immediates only.