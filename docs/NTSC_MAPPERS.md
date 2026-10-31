# CipherNES 1.0.0 NTSC Mapper Compatibility

> **Compatibility scope:** Mapper support refers to implemented mapper families and documented test coverage; individual game compatibility may vary with board variants and peripheral requirements. See [NTSC compatibility](NTSC_COMPATIBILITY.md).


This page lists the mapper hardware supported by CipherNES 1.0.0.

## Implementation

Mapper support is implemented in the production core under `CipherNES_Core/Mappers/` and selected by the cartridge factory in `CipherNES_Core/cartridge.cpp`.



The table comes directly from the mapper cases in `CipherNES_Core/cartridge.cpp` and the implementations under `CipherNES_Core/Mappers/`.

## Supported mapper chart

CipherNES 1.0.0 recognizes and implements **151 mapper / disk-system IDs** in the release core, including Famicom Disk System Mapper 20.

Common board names are used where they are well established. Obscure clone and multicart assignments keep the mapper number when a single public name would be misleading.

| Mapper ID | Common mapper / board name | CipherNES implementation family | Notes |
| ---: | --- | --- | --- |
| 0 | NROM | Dedicated mapper implementation |  |
| 1 | MMC1 / SxROM | Dedicated mapper implementation |  |
| 2 | UxROM | Dedicated mapper implementation |  |
| 3 | CNROM | Dedicated mapper implementation |  |
| 4 | MMC3 / MMC6 / TxROM | Dedicated mapper implementation |  |
| 5 | MMC5 / ExROM | Dedicated mapper implementation |  |
| 6 | Front Fareast FFE F4xxx | Front Fareast FFE |  |
| 7 | AxROM | Dedicated mapper implementation |  |
| 8 | Front Fareast FFE F3xxx | Front Fareast FFE |  |
| 9 | MMC2 / PxROM | Dedicated mapper implementation |  |
| 10 | MMC4 / FxROM | Dedicated mapper implementation |  |
| 11 | Color Dreams | Dedicated mapper implementation |  |
| 12 | MMC3-compatible variant (Mapper 12) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 13 | CPROM | Dedicated mapper implementation |  |
| 15 | 100-in-1 Contra Function 16 | Dedicated mapper implementation | Multicart mapper designation used by NESdev. |
| 16 | Bandai FCG / LZ93D50 family | Dedicated mapper implementation |  |
| 17 | Front Fareast Super Magic Card | Dedicated mapper implementation |  |
| 18 | Jaleco SS8806 | Dedicated mapper implementation |  |
| 19 | Namco 163 / N163 | Dedicated mapper implementation | Namco-family mapper with expansion-audio support in the release core. |
| 20 | Famicom Disk System | FDS disk-system | FDS; requires a user-supplied 8192-byte `disksys.rom` BIOS. |
| 21 | Konami VRC4 family | Konami VRC2 / VRC4 |  |
| 22 | Konami VRC2a | Dedicated mapper implementation |  |
| 23 | Konami VRC2 / VRC4 family | Konami VRC2 / VRC4 |  |
| 24 | Konami VRC6a | Konami VRC6 | Expansion-audio capable hardware family. |
| 25 | Konami VRC2 / VRC4 family | Konami VRC2 / VRC4 |  |
| 26 | Konami VRC6b | Konami VRC6 | Expansion-audio capable hardware family. |
| 28 | Action 53 | Dedicated mapper implementation |  |
| 32 | Irem G-101 | Dedicated mapper implementation |  |
| 33 | Taito TC0190FMC / TC0350FMR | Dedicated mapper implementation |  |
| 34 | BNROM / NINA-001 family | Dedicated mapper implementation |  |
| 41 | Discrete board (Mapper 41) | Discrete / register-latch board | Mapper-specific equations in the discrete/register-latch implementation. |
| 42 | Board-specific mapper 42 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 43 | Board-specific mapper 43 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 44 | MMC3-compatible variant (Mapper 44) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 45 | MMC3-compatible variant (Mapper 45) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 46 | Board-specific mapper 46 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 47 | MMC3-compatible variant (Mapper 47) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 49 | MMC3-compatible variant (Mapper 49) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 50 | Board-specific mapper 50 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 51 | Board-specific mapper 51 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 52 | MMC3-compatible variant (Mapper 52) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 53 | Mapper 53 | Dedicated mapper implementation |  |
| 56 | Kaiser KS202 | Kaiser KS202 |  |
| 57 | Discrete board (Mapper 57) | Discrete / register-latch board | Mapper-specific equations in the discrete/register-latch implementation. |
| 58 | Board-specific mapper 58 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 60 | Board-specific mapper 60 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 61 | Board-specific mapper 61 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 62 | Board-specific mapper 62 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 64 | Tengen RAMBO-1 | Dedicated mapper implementation |  |
| 65 | Irem H-3001 | Dedicated mapper implementation |  |
| 66 | GxROM | Dedicated mapper implementation |  |
| 67 | Sunsoft 3 | Dedicated mapper implementation |  |
| 68 | Sunsoft 4 / After Burner | Dedicated mapper implementation | ROM-based nametable family. |
| 69 | Sunsoft FME-7 / Sunsoft 5B | Dedicated mapper implementation |  |
| 70 | Bandai 74161/7432 | Dedicated mapper implementation |  |
| 71 | Camerica / Codemasters family | Dedicated mapper implementation |  |
| 72 | Jaleco JF-17 / JF-19 | Jaleco discrete board |  |
| 73 | Konami VRC3 | Dedicated mapper implementation |  |
| 74 | MMC3-compatible variant (Mapper 74) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 75 | Konami VRC1 | Dedicated mapper implementation |  |
| 76 | Namco 109 variant | Namco 108 family | Namco 108-family hardware variant. |
| 77 | Irem LROG017 | Dedicated mapper implementation |  |
| 78 | Irem 74HC161 / 74HC32-based | Dedicated mapper implementation | Discrete Irem mapper family. |
| 79 | NINA-03 / NINA-06 | Dedicated mapper implementation |  |
| 80 | Taito X1-005 | Dedicated mapper implementation |  |
| 82 | Taito X1-017 | Dedicated mapper implementation |  |
| 83 | Cony / Yoko family | Dedicated mapper implementation |  |
| 85 | Konami VRC7 | Dedicated mapper implementation | Expansion-audio capable hardware family. |
| 86 | Jaleco JF-13 | Jaleco discrete board |  |
| 87 | Mapper 87 | Dedicated mapper implementation |  |
| 88 | Namco 108 family | Namco 108 family |  |
| 89 | Discrete board (Mapper 89) | Discrete / register-latch board | Mapper-specific equations in the discrete/register-latch implementation. |
| 90 | J.Y. Company ASIC | J.Y. Company ASIC |  |
| 91 | HK-SF3 | Dedicated mapper implementation | Commonly documented as a pirate HK-SF3 mapper. |
| 92 | Jaleco JF-17 / JF-19 | Jaleco discrete board |  |
| 93 | Board-specific mapper 93 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 94 | Senjou no Ookami | Discrete / small-ASIC board | Established iNES mapper designation. |
| 95 | Namco 108 family | Namco 108 family |  |
| 96 | Board-specific mapper 96 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 97 | Irem TAM-S1 | Dedicated mapper implementation |  |
| 99 | Vs. System family | Dedicated mapper implementation |  |
| 105 | NES-EVENT / Nintendo World Championships | Dedicated mapper implementation |  |
| 107 | Board-specific mapper 107 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 111 | GTROM | Dedicated mapper implementation |  |
| 112 | Discrete board (Mapper 112) | Discrete / register-latch board | Mapper-specific equations in the discrete/register-latch implementation. |
| 113 | HES NTD-8 | Dedicated mapper implementation | Multicart-oriented mapper family. |
| 114 | MMC3-compatible variant (Mapper 114) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 115 | MMC3-compatible variant (Mapper 115) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 117 | Board-specific mapper 117 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 118 | MMC3 / TxSROM-family variant | MMC3-compatible variant | MMC3-family wiring with extended nametable and/or mixed CHR behavior. |
| 119 | TQROM / MMC3 mixed CHR ROM-RAM | MMC3-compatible variant | MMC3-family wiring with extended nametable and/or mixed CHR behavior. |
| 120 | Board-specific mapper 120 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 123 | MMC3-compatible variant (Mapper 123) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 128 | MMC3-compatible variant (Mapper 128) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 129 | Board-specific mapper 129 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 130 | Mapper 130 | Dedicated mapper implementation |  |
| 133 | Board-specific mapper 133 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 140 | Jaleco JF-11 / JF-14 | Jaleco discrete board |  |
| 142 | Kaiser KS202 | Kaiser KS202 |  |
| 151 | VRC1-family variant | VRC1-family |  |
| 160 | Mapper 160 | Dedicated mapper implementation |  |
| 161 | MMC1-family variant | MMC1-family |  |
| 162 | Board-specific mapper 162 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 164 | Board-specific mapper 164 | Discrete / small-ASIC board | Mapper-specific equations in the discrete/small-ASIC implementation. |
| 165 | MMC3-compatible variant (Mapper 165) | MMC3-compatible variant | MMC3-family wiring with extended nametable and/or mixed CHR behavior. |
| 166 | SUBOR | Discrete / small-ASIC board | SUBOR-family mapper designation. |
| 180 | Crazy Climber / UNROM-180 | Dedicated mapper implementation | UNROM-style variant with fixed first bank behavior. |
| 182 | MMC3-compatible variant (Mapper 182) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 184 | Sunsoft-1 | Dedicated mapper implementation |  |
| 185 | CNROM with protection diodes | Dedicated mapper implementation | CNROM-family protection variant. |
| 187 | MMC3-compatible variant (Mapper 187) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 188 | Mapper 188 | Dedicated mapper implementation |  |
| 189 | MMC3-compatible variant (Mapper 189) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 193 | NTDEC TC-112 | Dedicated mapper implementation |  |
| 195 | MMC3-compatible variant (Mapper 195) | MMC3-compatible variant | MMC3-family wiring with extended nametable and/or mixed CHR behavior. |
| 196 | MMC3-compatible variant (Mapper 196) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 199 | MMC3-compatible variant (Mapper 199) | MMC3-compatible variant | MMC3-family wiring with extended nametable and/or mixed CHR behavior. |
| 200 | Discrete board (Mapper 200) | Discrete / register-latch board | Mapper-specific equations in the discrete/register-latch implementation. |
| 201 | Discrete board (Mapper 201) | Discrete / register-latch board | Mapper-specific equations in the discrete/register-latch implementation. |
| 202 | Discrete board (Mapper 202) | Discrete / register-latch board | Mapper-specific equations in the discrete/register-latch implementation. |
| 203 | Discrete board (Mapper 203) | Discrete / register-latch board | Mapper-specific equations in the discrete/register-latch implementation. |
| 204 | Discrete board (Mapper 204) | Discrete / register-latch board | Mapper-specific equations in the discrete/register-latch implementation. |
| 205 | MMC3-compatible variant (Mapper 205) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 210 | Namco 175 / 340 family | Dedicated mapper implementation |  |
| 211 | J.Y. Company ASIC | J.Y. Company ASIC |  |
| 222 | VRC2 / VRC4-compatible family | Dedicated mapper implementation |  |
| 224 | MMC3-compatible variant (Mapper 224) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 225 | Multicart board (Mapper 225) | Discrete multicart | Mapper-specific PRG/CHR latch equations in the multicart implementation. |
| 226 | Multicart board (Mapper 226) | Discrete multicart | Mapper-specific PRG/CHR latch equations in the multicart implementation. |
| 227 | Multicart board (Mapper 227) | Discrete multicart | Mapper-specific PRG/CHR latch equations in the multicart implementation. |
| 228 | Action 52 | Discrete multicart | Action 52 multicart mapper. |
| 229 | Multicart board (Mapper 229) | Discrete multicart | Mapper-specific PRG/CHR latch equations in the multicart implementation. |
| 230 | Multicart board (Mapper 230) | Discrete multicart | Mapper-specific PRG/CHR latch equations in the multicart implementation. |
| 231 | Multicart board (Mapper 231) | Discrete multicart | Mapper-specific PRG/CHR latch equations in the multicart implementation. |
| 232 | Camerica Quattro family | Dedicated mapper implementation |  |
| 233 | Multicart board (Mapper 233) | Discrete multicart | Mapper-specific PRG/CHR latch equations in the multicart implementation. |
| 234 | Mapper 234 | Dedicated mapper implementation |  |
| 240 | Multicart board (Mapper 240) | Discrete multicart | Mapper-specific PRG/CHR latch equations in the multicart implementation. |
| 241 | Multicart board (Mapper 241) | Discrete multicart | Mapper-specific PRG/CHR latch equations in the multicart implementation. |
| 242 | Discrete board (Mapper 242) | Discrete / register-latch board | Mapper-specific equations in the discrete/register-latch implementation. |
| 243 | Discrete board (Mapper 243) | Discrete / register-latch board | Mapper-specific equations in the discrete/register-latch implementation. |
| 244 | Multicart board (Mapper 244) | Discrete multicart | Mapper-specific PRG/CHR latch equations in the multicart implementation. |
| 245 | MMC3-compatible variant (Mapper 245) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 246 | Discrete board (Mapper 246) | Discrete / register-latch board | Mapper-specific equations in the discrete/register-latch implementation. |
| 248 | MMC3-compatible variant (Mapper 248) | MMC3-compatible variant | Obsolete duplicate assignment; CipherNES maps it to mapper 115 hardware. |
| 249 | MMC3-compatible variant (Mapper 249) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 250 | MMC3-compatible variant (Mapper 250) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 251 | MMC3-compatible variant (Mapper 251) | MMC3-compatible variant | Obsolete iNES assignment; CipherNES maps it to mapper 45 board wiring. |
| 252 | Waixing mapper 252 | Dedicated mapper implementation |  |
| 254 | MMC3-compatible variant (Mapper 254) | MMC3-compatible variant | MMC3-compatible board with mapper-specific wiring/profile. |
| 255 | Multicart board (Mapper 255) | Discrete multicart | Mapper-specific PRG/CHR latch equations in the multicart implementation. |

## Table notes

- **Common mapper / board name** uses the usual hardware or family name when there is a clear one.
- **Board-specific mapper N** means the mapper is implemented, but no single common board name is used here.
- **MMC3-compatible variant** means the mapper uses MMC3-compatible IRQ/protection behavior with mapper-specific bank-line, outer-bank, CHR-RAM, nametable, or protection wiring.
- **Discrete / register-latch**, **discrete / small-ASIC**, and **multicart** entries are not generic fallbacks. The mapper number selects mapper-specific hardware equations inside the corresponding shared implementation.

## Shared implementation families

The release core includes dedicated shared implementations for:

- MMC3-compatible board variants;
- Konami VRC6 variants;
- Namco 108 variants;
- J.Y. Company ASIC boards;
- Kaiser KS202 boards;
- Jaleco discrete boards;
- discrete multicart boards;
- common discrete/register-latch boards;
- additional discrete/small-ASIC boards.

Several related boards share code, but the mapper ID still selects the board-specific behavior.

## Famicom Disk System

FDS is supported through Mapper 20 and accepts `.fds` and `.qd` disk images. A legally obtained **8192-byte `disksys.rom` BIOS** must be supplied by the user; the BIOS is not included with CipherNES.

## What “supported” means

A mapper being listed means CipherNES recognizes the ID and has an implementation for it. It does not mean every dump, multicart variation, peripheral combination, malformed header, or future NES 2.0 submapper has been individually tested.

For accuracy-test certification and mapper-profile test results, see [NTSC Accuracy Certification](NTSC_ACCURACY.md).

## Naming reference

Common mapper names use established NES community conventions where available, with [NESdev's mapper list](https://www.nesdev.org/wiki/List_of_mappers) used as the primary public naming reference. Mapper numbers remain authoritative when historical naming is ambiguous or multiple boards share an iNES assignment.
