# CipherNES 1.0.0 NTSC Accuracy and Validation

> **Evidence scope:** This page records historical NTSC test outcomes and hardware-profile notes. These records must not be interpreted as a fresh test run of the current GitHub Actions binaries. See [validation methodology](VALIDATION.md).


**Current AccuracyCoin result:** October 21, 2026 (updated ROM, trimmed Ubuntu 1.0 core)  
**Earlier NTSC corpus certification:** September 4, 2026  
**Timing profile:** NTSC  
**Release:** CipherNES 1.0.0

> The updated AccuracyCoin result was measured on the repaired Ubuntu core. The other NTSC group totals below are historical records unless explicitly described as rerun; the full suite has not been independently run on every platform.

## Updated AccuracyCoin — 146 / 146 (100%), October 21, 2026


The supplied 146-test AccuracyCoin ROM completed **146 / 146 (100%)** again on the trimmed release **Ubuntu** core. This is a new ROM result, not a retroactive rerun of the earlier corpus. Before the CipherNES rename, native Windows, Ubuntu, macOS Intel, and macOS Apple Silicon builds passed CI and ROM-free PRG-RAM regression checks. The renamed targets still require fresh CI, and complete AccuracyCoin runs on each desktop platform have not been recorded.

AccuracyCoin is highlighted first because it is the clearest single-suite accuracy result in the current documentation. The score was also reproduced using the trimmed Ubuntu 1.0 core. The released repository does not include screenshots or test ROMs.

## Validation results

| Validation group | Certified result |
| --- | ---: |
| **AccuracyCoin (updated ROM, 2026-10-21 Ubuntu run)** | **146 / 146 (100%)** |
| CPU release validation | **14 / 14** |
| APU validation | **30 / 30** |
| DMA / DPCM / OAM validation | **14 / 14** |
| MMC3 validation | **18 / 18** |
| MMC1 / MMC5 deterministic visual states | **12 / 12** |
| Additional current formal tests | **34 / 34** |
| Focused PPU formal aggregate | **42 / 42** |
| **Current uploaded NTSC archive: 600-frame execution/state audit** | **289 / 289 valid iNES images** (internal result not asserted) |
| Historical whole-tree NTSC smoke audit | 270 / 270 valid iNES images |
| Pre/post visual regression comparison | **30 / 30 byte-identical framebuffer captures** |

## Named test-ROM results

The names below come from the archived NTSC test-ROM tree used during development. The ROM files themselves are not redistributed.

Where the original suite membership is clear, each ROM is listed under the certified suite result.

### AccuracyCoin

| Test ROM | ROM result | Certified suite score | Notes |
| --- | --- | ---: | --- |
| `AccuracyCoin.nes` (updated 146-test ROM) | **PASS** | **146 / 146 (100%)** | Recorded on repaired Ubuntu core, October 21, 2026; not yet independently measured on other desktop platforms. |

### APU validation — 30 / 30

| Test ROM | ROM result | Certified suite score | Notes |
| --- | --- | ---: | --- |
| `apu_test/rom_singles/3-irq_flag.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_test/rom_singles/1-len_ctr.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_test/rom_singles/5-len_timing.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_test/rom_singles/2-len_table.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_test/rom_singles/8-dmc_rates.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_test/rom_singles/7-dmc_basics.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_test/rom_singles/6-irq_flag_timing.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_test/rom_singles/4-jitter.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_test/apu_test.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `blargg_apu_2005.07.30/05.len_timing_mode0.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `blargg_apu_2005.07.30/02.len_table.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `blargg_apu_2005.07.30/08.irq_timing.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `blargg_apu_2005.07.30/03.irq_flag.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `blargg_apu_2005.07.30/11.len_reload_timing.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `blargg_apu_2005.07.30/06.len_timing_mode1.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `blargg_apu_2005.07.30/09.reset_timing.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `blargg_apu_2005.07.30/10.len_halt_timing.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `blargg_apu_2005.07.30/07.irq_flag_timing.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `blargg_apu_2005.07.30/01.len_ctr.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `blargg_apu_2005.07.30/04.clock_jitter.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_reset/4017_timing.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_reset/irq_flag_cleared.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_reset/4015_cleared.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_reset/works_immediately.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_reset/4017_written.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `apu_reset/len_ctrs_enabled.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `dmc_tests/latency.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `dmc_tests/status.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `dmc_tests/buffer_retained.nes` | **PASS** | **30 / 30** | APU release corpus. |
| `dmc_tests/status_irq.nes` | **PASS** | **30 / 30** | APU release corpus. |

### MMC3 / MMC6 validation — 18 / 18

| Test ROM | ROM result | Certified suite score | Notes |
| --- | --- | ---: | --- |
| `mmc3_irq_tests/3.A12_clocking.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_irq_tests/1.Clocking.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_irq_tests/2.Details.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_irq_tests/4.Scanline_timing.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_irq_tests/6.MMC3_rev_B.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_irq_tests/5.MMC3_rev_A.nes` | **PASS** | **18 / 18** | Passed with the alternate MMC3/MMC6 silicon profile it is designed to test. |
| `mmc3_test_2/rom_singles/1-clocking.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_test_2/rom_singles/2-details.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_test_2/rom_singles/6-MMC3_alt.nes` | **PASS** | **18 / 18** | Passed with the alternate MMC3/MMC6 silicon profile it is designed to test. |
| `mmc3_test_2/rom_singles/5-MMC3.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_test_2/rom_singles/3-A12_clocking.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_test_2/rom_singles/4-scanline_timing.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_test/1-clocking.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_test/2-details.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_test/6-MMC6.nes` | **PASS** | **18 / 18** | Passed with the alternate MMC3/MMC6 silicon profile it is designed to test. |
| `mmc3_test/5-MMC3.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_test/3-A12_clocking.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |
| `mmc3_test/4-scanline_timing.nes` | **PASS** | **18 / 18** | MMC3 release corpus. |

### Additional current formal tests — 34 / 34

| Test ROM | ROM result | Certified suite score | Notes |
| --- | --- | ---: | --- |
| `cpu_exec_space/test_cpu_exec_space_apu.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `cpu_exec_space/test_cpu_exec_space_ppuio.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `cpu_dummy_writes/cpu_dummy_writes_oam.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `cpu_dummy_writes/cpu_dummy_writes_ppumem.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `ppu_read_buffer/test_ppu_read_buffer.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `nes_instr_test/rom_singles/04-zp_xy.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `nes_instr_test/rom_singles/09-branches.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `nes_instr_test/rom_singles/03-zero_page.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `nes_instr_test/rom_singles/11-special.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `nes_instr_test/rom_singles/10-stack.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `nes_instr_test/rom_singles/01-implied.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `nes_instr_test/rom_singles/05-absolute.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `nes_instr_test/rom_singles/07-ind_x.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `nes_instr_test/rom_singles/02-immediate.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `nes_instr_test/rom_singles/08-ind_y.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `nes_instr_test/rom_singles/06-abs_xy.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/06-absolute.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/04-zero_page.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/15-brk.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/14-rti.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/08-ind_x.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/03-immediate.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/07-abs_xy.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/16-special.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/05-zp_xy.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/01-basics.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/09-ind_y.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/12-jmp_jsr.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/10-branches.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/02-implied.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/13-rts.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/rom_singles/11-stack.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/all_instrs.nes` | **PASS** | **34 / 34** | Current formal release corpus. |
| `instr_test-v5/official_only.nes` | **PASS** | **34 / 34** | Current formal release corpus. |

### Focused PPU formal aggregate — 42 / 42

| Test ROM | ROM result | Certified suite score | Notes |
| --- | --- | ---: | --- |
| `sprite_hit_tests_2005.10.05/09.timing_basics.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_hit_tests_2005.10.05/11.edge_timing.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_hit_tests_2005.10.05/05.left_clip.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_hit_tests_2005.10.05/10.timing_order.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_hit_tests_2005.10.05/02.alignment.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_hit_tests_2005.10.05/03.corners.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_hit_tests_2005.10.05/01.basics.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_hit_tests_2005.10.05/04.flip.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_hit_tests_2005.10.05/08.double_height.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_hit_tests_2005.10.05/07.screen_bottom.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_hit_tests_2005.10.05/06.right_edge.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_overflow_tests/5.Emulator.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_overflow_tests/2.Details.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_overflow_tests/4.Obscure.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_overflow_tests/3.Timing.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `sprite_overflow_tests/1.Basics.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `vbl_nmi_timing/2.vbl_timing.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `vbl_nmi_timing/1.frame_basics.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `vbl_nmi_timing/4.vbl_clear_timing.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `vbl_nmi_timing/5.nmi_suppression.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `vbl_nmi_timing/3.even_odd_frames.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `vbl_nmi_timing/6.nmi_disable.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `vbl_nmi_timing/7.nmi_timing.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `oam_stress/oam_stress.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `ppu_open_bus/ppu_open_bus.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `ppu_vbl_nmi/rom_singles/03-vbl_clear_time.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `ppu_vbl_nmi/rom_singles/05-nmi_timing.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `ppu_vbl_nmi/rom_singles/09-even_odd_frames.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `ppu_vbl_nmi/rom_singles/06-suppression.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `ppu_vbl_nmi/rom_singles/07-nmi_on_timing.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `ppu_vbl_nmi/rom_singles/01-vbl_basics.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `ppu_vbl_nmi/rom_singles/10-even_odd_timing.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `ppu_vbl_nmi/rom_singles/02-vbl_set_time.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `ppu_vbl_nmi/rom_singles/08-nmi_off_timing.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `ppu_vbl_nmi/rom_singles/04-nmi_control.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `ppu_vbl_nmi/ppu_vbl_nmi.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `oam_read/oam_read.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `blargg_ppu_tests_2005.09.15b/palette_ram.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `blargg_ppu_tests_2005.09.15b/vbl_clear_time.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `blargg_ppu_tests_2005.09.15b/vram_access.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `blargg_ppu_tests_2005.09.15b/sprite_ram.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |
| `blargg_ppu_tests_2005.09.15b/power_up_palette.nes` | **PASS** | **42 / 42** | Focused PPU release corpus. |

## Aggregate-only results

The original audit kept the perfect aggregate scores below, but not an authoritative filename-by-filename membership for every subcheck. They stay as aggregate results here rather than assigning per-ROM scores after the fact.

| Validation group | Certified result |
| --- | ---: |
| CPU release validation | **14 / 14** |
| DMA / DPCM / OAM validation | **14 / 14** |
| MMC1 / MMC5 deterministic visual states | **12 / 12** |

The goal is simple: keep the historical record accurate and do not invent detail that was not retained.

## Other named verified cases

| Test ROM | Historical result |
| --- | --- |
| `read_joy3/thorough_test.nes` | **PASS** |
| `read_joy3/test_buttons.nes` | **PASS** with the correct CipherNES controller mask |
| `other/read2004.nes` | **PASS** |
| `scanline/scanline.nes` | No error stars visible |
| `scanline-a1/scanline.nes` | No error stars visible |

## Whole-tree NTSC smoke audit

The development test tree contained **271 NTSC-oriented `.nes` filenames** after PAL-only paths/files were excluded.

- **270 / 270 valid iNES images loaded and executed**
- `other/LINUSMUS.NES` was excluded from the ROM count because its header is not valid iNES data

A smoke-audit result only means the ROM loaded and ran. It is not treated as a formal accuracy pass unless it belongs to one of the certified groups above.

## Historical / interpretive diagnostics

### NEStress

NEStress is documented separately rather than being misrepresented as a modern binary accuracy failure:

```text
CPU: 48 / 48
PPU: 12 / 14
```

The same 12/14 PPU result was observed before and after the final AccuracyCoin fixes, so it was not introduced by the release corrections.

### Controller/DMC counters

`read_joy3/count_errors.nes` and `read_joy3/count_errors_fast.nes` intentionally count/report DMC/controller conflicts. They are diagnostic counters, not binary pass/fail tests.

### Manual / visual / peripheral ROMs

240p, palette, raster, display, sound, volume, paddle/Vaus, and similar utilities require human interpretation or interaction. They are intentionally not given artificial PASS scores merely because they launch.

## Pre/post visual regression check

Thirty representative NTSC visual/manual ROMs were rendered before and after the final AccuracyCoin fixes at matching frame points.

```text
30 / 30 byte-identical framebuffer captures
```

This confirmed that the final secondary-OAM and DMC corrections did not trade the new AccuracyCoin passes for visible regressions elsewhere in the representative set.

## Short summary

A short version of the result is:

> **Current AccuracyCoin:** 146/146 on the repaired Ubuntu core (October 21, 2026). **Earlier NTSC corpus:** additional release certification totals are retained as historical records, not silently recertified for the repaired build or other operating systems.

That statement is intentionally scoped to the certified release corpus and does not claim that no future test or previously unseen software can expose a new defect.
