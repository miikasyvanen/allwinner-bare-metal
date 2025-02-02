// SPDX-License-Identifier: MIT
// Copyright (c) 2021 Ulrich Hecht

#include <stdint.h>

#define TVE_BASE 0x01e00000

#include "ccu.h"
#include "display.h"
#include "interrupts.h"
#include "system.h"
#include "tve.h"
#include "util.h"

extern struct virt_mode_t dsp;

// Allwinner TV encoder register symbols taken from u-boot source code,
// which in turn takes them from the A10 User Manual.
#define REG(a) (*(volatile uint32_t *)(a))

#define TVE_GCTRL             REG(TVE_BASE + 0x000)
#define TVE_CFG0              REG(TVE_BASE + 0x004)
#define TVE_DAC_CFG0          REG(TVE_BASE + 0x008)
#define TVE_FILTER            REG(TVE_BASE + 0x00C)
#define TVE_CHROMA_FREQ       REG(TVE_BASE + 0x010)
#define TVE_PORCH_NUM         REG(TVE_BASE + 0x014)
#define TVE_UNKNOWN0          REG(TVE_BASE + 0x018)
#define TVE_LINE_NUM          REG(TVE_BASE + 0x01C)
#define TVE_BLANK_BLACK_LEVEL REG(TVE_BASE + 0x020)
#define TVE_UNKNOWN1          REG(TVE_BASE + 0x024)
// reserved			   REG(TVE_BASE + 0x028)
#define TVE_AUTO_DETECT_EN         REG(TVE_BASE + 0x030)
#define TVE_AUTO_DETECT_INT_STATUS REG(TVE_BASE + 0x034)
#define TVE_AUTO_DETECT_STATUS     REG(TVE_BASE + 0x038)
#define TVE_AUTO_DETECT_DEBOUNCE   REG(TVE_BASE + 0x03C)
#define TVE_CSC_REG0               REG(TVE_BASE + 0x040)
#define TVE_CSC_REG1               REG(TVE_BASE + 0x044)
#define TVE_CSC_REG2               REG(TVE_BASE + 0x048)
#define TVE_CSC_REG3               REG(TVE_BASE + 0x04C)
// reserved			   REG(TVE_BASE + 0x050)
#define TVE_COLOR_BURST    REG(TVE_BASE + 0x100)
#define TVE_VSYNC_NUM      REG(TVE_BASE + 0x104)
#define TVE_NOTCH_FREQ     REG(TVE_BASE + 0x108)
#define TVE_CBR_LEVEL      REG(TVE_BASE + 0x10C)
#define TVE_BURST_PHASE    REG(TVE_BASE + 0x110)
#define TVE_BURST_WIDTH    REG(TVE_BASE + 0x114)
#define TVE_UNKNOWN2       REG(TVE_BASE + 0x118)
#define TVE_SYNC_VBI_LEVEL REG(TVE_BASE + 0x11C)
#define TVE_WHITE_LEVEL    REG(TVE_BASE + 0x120)
#define TVE_ACTIVE_NUM     REG(TVE_BASE + 0x124)
#define TVE_CHROMA_BW_GAIN REG(TVE_BASE + 0x128)
#define TVE_NOTCH_WIDTH    REG(TVE_BASE + 0x12C)
#define TVE_RESYNC_NUM     REG(TVE_BASE + 0x130)
#define TVE_SLAVE_PARA     REG(TVE_BASE + 0x134)
#define TVE_CFG1           REG(TVE_BASE + 0x138)
#define TVE_CFG2           REG(TVE_BASE + 0x13C)

/*
 * Select input 0 to disable dac, 1 - 4 to feed dac from tve0, 5 - 8 to feed
 * dac from tve1. When using tve1 the mux value must be written to both tve0's
 * and tve1's gctrl reg.
 */
#define TVE_GCTRL_DAC_INPUT_MASK(dac) (0xf << (((dac) + 1) * 4))
#define TVE_GCTRL_DAC_INPUT(dac, sel) ((sel) << (((dac) + 1) * 4))
#define TVE_CFG0_VGA                  0x20000000
#define TVE_CFG0_PAL                  0x07030001
#define TVE_CFG0_NTSC                 0x07030000
#define TVE_DAC_CFG0_VGA              0x403e1ac7
#ifdef CONFIG_MACH_SUN5I
#define TVE_DAC_CFG0_COMPOSITE 0x433f0009
#else
#define TVE_DAC_CFG0_COMPOSITE 0x403f0008
#endif
#define TVE_FILTER_COMPOSITE                0x00000120
#define TVE_CHROMA_FREQ_PAL_M               0x21e6efe3
#define TVE_CHROMA_FREQ_PAL_NC              0x21f69446
#define TVE_PORCH_NUM_PAL                   0x008a0018
#define TVE_PORCH_NUM_NTSC                  0x00760020
#define TVE_LINE_NUM_PAL                    0x00160271
#define TVE_LINE_NUM_NTSC                   0x0016020d
#define TVE_BLANK_BLACK_LEVEL_PAL           0x00fc00fc
#define TVE_BLANK_BLACK_LEVEL_NTSC          0x00f0011a
#define TVE_UNKNOWN1_VGA                    0x00000000
#define TVE_UNKNOWN1_COMPOSITE              0x18181818
#define TVE_AUTO_DETECT_EN_DET_EN(dac)      (1 << ((dac) + 0))
#define TVE_AUTO_DETECT_EN_INT_EN(dac)      (1 << ((dac) + 16))
#define TVE_AUTO_DETECT_INT_STATUS_BIT(dac) (1 << ((dac) + 0))
#define TVE_AUTO_DETECT_STATUS_SHIFT(dac)   ((dac)*8)
#define TVE_AUTO_DETECT_STATUS_MASK(dac)    (3 << ((dac)*8))
#define TVE_AUTO_DETECT_STATUS_NONE         0
#define TVE_AUTO_DETECT_STATUS_CONNECTED    1
#define TVE_AUTO_DETECT_STATUS_SHORT_GND    3
#define TVE_AUTO_DETECT_DEBOUNCE_SHIFT(d)   ((d)*8)
#define TVE_AUTO_DETECT_DEBOUNCE_MASK(d)    (0xf << ((d)*8))
#define TVE_CSC_REG0_ENABLE                 (1 << 31)
#define TVE_CSC_REG0_BITS                   0x08440832
#define TVE_CSC_REG1_BITS                   0x3b6dace1
#define TVE_CSC_REG2_BITS                   0x0e1d13dc
#define TVE_CSC_REG3_BITS                   0x00108080
#define TVE_COLOR_BURST_PAL_M               0x00000000
#define TVE_CBR_LEVEL_PAL                   0x00002828
#define TVE_CBR_LEVEL_NTSC                  0x0000004f
#define TVE_BURST_PHASE_NTSC                0x00000000
#define TVE_BURST_WIDTH_COMPOSITE           0x0016447e
#define TVE_UNKNOWN2_PAL                    0x0000e0e0
#define TVE_UNKNOWN2_NTSC                   0x0000a0a0
#define TVE_SYNC_VBI_LEVEL_NTSC             0x001000f0
#define TVE_ACTIVE_NUM_COMPOSITE            0x000005a0
#define TVE_CHROMA_BW_GAIN_COMP             0x00000002
#define TVE_NOTCH_WIDTH_COMPOSITE           0x00000101
#define TVE_RESYNC_NUM_PAL                  0x800d000c
#define TVE_RESYNC_NUM_NTSC                 0x000e000c
#define TVE_SLAVE_PARA_COMPOSITE            0x00000000

// Many operations on registers in the DE mixer will only take effect after doing
// this:
void tve_update_buffer(void)
{
  DE_MIXER1_GLB_DBUFFER = 1;
}

int display_is_pal;

void tve_init(int pal)
{
  display_is_pal = pal;

  // initialize clocks
  DE_CLK           = 0x81000001;
  TCON0_CLK        = 0x00000000;
  PLL_VIDEO_CTRL   = 0x93006207;
  PLL_PERIPH0_CTRL = 0x90041811;
  PLL_PERIPH1_CTRL = 0x90041811;
  PLL_DE_CTRL      = 0x91002300;

  udelay(10);

  TVE_CLK = 0x80000003;
  BUS_CLK_GATING1 |= BIT(12) | BIT(9) | BIT(4);
  BUS_SOFT_RST1 |= BIT(12) | BIT(9) | BIT(4);

  // TVE
  for (int i = TVE_BASE; i <= TVE_BASE + 0x140; i += 4)
    REG(i) = 0;

  // All initialization values are derived from a register dump of a Linux
  // vendor kernel with TV out support.

  // The H3 TV encoder is not documented in the Allwinner datasheet.
  // Comments from u-boot source code (which does, as of now, not support
  // TV out on the H3) suggest that the TV encoder is largely the same as
  // the one documented in the A10 datasheet. A lot of the values from the
  // Linux register dump do not make sense when checked against that
  // documentation, however, suggesting that the differences are larger
  // than assumed. The magic numbers from the register dump have therefore
  // only been symbolized where it is clear that they actually correspond
  // 1:1 to the documented variety.

  TVE_GCTRL             = 0x00000301;
  TVE_CFG0              = pal ? 0x07070001 : 0x07070000;
  TVE_DAC_CFG0          = 0x433E12B1;
  TVE_FILTER            = 0x30001400;
  TVE_CHROMA_FREQ       = pal ? 0x2A098ACB : 0x21F07C1F;
  TVE_PORCH_NUM         = pal ? TVE_PORCH_NUM_PAL : TVE_PORCH_NUM_NTSC;
  TVE_UNKNOWN0          = 0x00000016;
  TVE_LINE_NUM          = pal ? TVE_LINE_NUM_PAL : TVE_LINE_NUM_NTSC;
  TVE_BLANK_BLACK_LEVEL = pal ? TVE_BLANK_BLACK_LEVEL_PAL : TVE_BLANK_BLACK_LEVEL_NTSC;

  TVE_AUTO_DETECT_EN         = 0x00000001;
  TVE_AUTO_DETECT_INT_STATUS = 0x00000000;
  TVE_AUTO_DETECT_STATUS     = 0x00000001;
  TVE_AUTO_DETECT_DEBOUNCE   = 0x00000009;

  REG(TVE_BASE + 0x00f8) = 0x00000280;
  REG(TVE_BASE + 0x00fc) = 0x028F00FF;

  TVE_COLOR_BURST    = pal ? 0x00000000 : 0x00000001;
  TVE_VSYNC_NUM      = pal ? 0x00000001 : 0x00000000;
  TVE_NOTCH_FREQ     = pal ? 0x00000005 : 0x00000002;
  TVE_CBR_LEVEL      = pal ? 0x00002929 : TVE_CBR_LEVEL_NTSC;
  TVE_BURST_PHASE    = 0x00000000;
  TVE_BURST_WIDTH    = TVE_BURST_WIDTH_COMPOSITE;
  TVE_UNKNOWN2       = pal ? 0x0000A8A8 : 0x0000A0A0;
  TVE_SYNC_VBI_LEVEL = pal ? 0x001000FC : TVE_SYNC_VBI_LEVEL_NTSC;
  TVE_WHITE_LEVEL    = 0x01E80320;
  TVE_ACTIVE_NUM     = TVE_ACTIVE_NUM_COMPOSITE;
  TVE_CHROMA_BW_GAIN = pal ? 0x00010000 : 0;
  TVE_NOTCH_WIDTH    = 0x00000101;
  TVE_RESYNC_NUM     = pal ? 0x3005000A : 0x30050368;

  // DE display system
  DE_SCLK_GATE   = 0x00000002;
  DE_HCLK_GATE   = 0x00000002;
  DE_AHB_RESET   = 0x00000004;
  DE_SCLK_DIV    = 0x00000000;
  DE_DE2TCON_MUX = 0x00000000;
  DE_CMD_CTL     = 0x00000000;

  // mixer registers contain random data after reset
  for (uint32_t i = DE_MIXER1; i < DE_MIXER1 + 0xc000; i += 4)
    REG(i) = 0;

  // DE mixer1 glb
  DE_MIXER1_GLB_CTL  = 0x00001001;
  DE_MIXER1_GLB_SIZE = DE_SIZE_PHYS;

  // DE mixer1 bld
  DE_MIXER1_BLD_FILL_COLOR_CTL = 0x00000101;
  DE_MIXER1_BLD_FILL_COLOR(0)  = 0xFF000000;  // opaque black
  DE_MIXER1_BLD_CH_ISIZE(0)    = DE_SIZE_PHYS;

  DE_MIXER1_BLD_FILL_COLOR(1) = 0xFF000000;

  DE_MIXER1_BLD_CH_RTCTL   = 0x00000001;
  DE_MIXER1_BLD_PREMUL_CTL = 0x00000000;
  DE_MIXER1_BLD_BK_COLOR   = 0xFF000000;
  DE_MIXER1_BLD_SIZE       = DE_SIZE_PHYS;
  DE_MIXER1_BLD_CTL(0)     = 0x03010301;

  // DE mixer1 ovl_ui ch1
  DE_MIXER1_OVL_UI_ATTR_CTL(0) = 0xFF000003;
  DE_MIXER1_OVL_UI_MBSIZE(0)   = DE_SIZE_PHYS;

  DE_MIXER1_OVL_UI_PITCH(0)    = 0x00000B40;
  DE_MIXER1_OVL_UI_TOP_LADD(0) = 0x40000000;  // show text section on screen :)

  DE_MIXER1_OVL_UI_SIZE = DE_SIZE_PHYS;

  // DE mixer1 post_proc2
  // This does the colorspace conversion. Seems to be undocumented. Mainline
  // Linux appears to use the (barely) documented CSC submodule instead.
  REG(0x012b0000) = 0x00000001;
  REG(0x012b0004) = 0x00000001;
  REG(0x012b0008) = 0x00000001;
  REG(0x012b000c) = 0x00000001;
  REG(0x012b0010) = 0x00000107;
  REG(0x012b0014) = 0x00000204;
  REG(0x012b0018) = 0x00000064;
  REG(0x012b001c) = 0x00004200;
  REG(0x012b0020) = 0x00001F68;
  REG(0x012b0024) = 0x00001ED6;
  REG(0x012b0028) = 0x000001C2;
  REG(0x012b002c) = 0x00020200;
  REG(0x012b0030) = 0x000001C2;
  REG(0x012b0034) = 0x00001E87;
  REG(0x012b0038) = 0x00001FB7;
  REG(0x012b003c) = 0x00020200;
  REG(0x012b0040) = 0x00020200;

  // TCON LCD1
  for (uint32_t i = 0x01c0d000; i < 0x01c0e000; i += 4)
    REG(i) = 0;

  LCD1_GCTL = 0x80000000;

  LCD1_TCON1_CTL = 0x800001F0;
  // The register layout (0xYYYYXXXX) is reversed here (0xXXXXYYYY), so we
  // cannot use DE_SIZE_PHYS.
  LCD1_TCON1_BASIC0 = DE_SIZE(DISPLAY_PHYS_RES_Y, DISPLAY_PHYS_RES_X);
  LCD1_TCON1_BASIC1 = DE_SIZE(DISPLAY_PHYS_RES_Y, DISPLAY_PHYS_RES_X);
  LCD1_TCON1_BASIC2 = DE_SIZE(DISPLAY_PHYS_RES_Y, DISPLAY_PHYS_RES_X);
  LCD1_TCON1_BASIC3 = pal ? 0x035F0083 : 0x03590079;
  LCD1_TCON1_BASIC4 = pal ? 0x04E2002B : 0x041A0023;
  LCD1_TCON1_BASIC5 = pal ? 0x003F0004 : 0x003D0005;

  LCD1_TCON1_PS_SYNC = 0x00010001;

  LCD1_TCON1_IO_POL = 0x04000000;
  LCD1_TCON1_IO_TRI = 0xFFFFFFFF;

  LCD1_TCON_CEU_CTL = 0x80000000;

  LCD1_TCON_CEU_COEF_MUL(0)  = 0x00000100;
  LCD1_TCON_CEU_COEF_MUL(5)  = 0x00000100;
  LCD1_TCON_CEU_COEF_MUL(10) = 0x00000100;

  LCD1_TCON_CEU_COEF_RANG(0) = 0x001000EB;
  LCD1_TCON_CEU_COEF_RANG(1) = 0x001000F0;
  LCD1_TCON_CEU_COEF_RANG(2) = 0x001000F0;

  // clang-format off
  static const uint32_t gamma_table_pal[] = {
    0x004E0A12, 0x00DC55E4, 0x0067BF86, 0x0004E7E6, 0x0023A82C, 0x00B7BA92, 0x0058D0C8, 0x00941BCE,
    0x00109A9E, 0x00172FDF, 0x0078F22D, 0x00C9D83E, 0x007A44B0, 0x001BC2AE, 0x00470C8F, 0x0049D62B,
    0x005DEB5B, 0x0000C3EE, 0x007F087E, 0x00446F42, 0x00C56806, 0x004B0E83, 0x00A61160, 0x00B82427,
    0x007827B0, 0x0007BF07, 0x006585BF, 0x006549C1, 0x00DAF450, 0x00163DA2, 0x00BAB2AA, 0x00AF30FC,
    0x004DDCB1, 0x00C5919C, 0x00345D7C, 0x009C76E3, 0x00F900E4, 0x006DB3BB, 0x004E5B0F, 0x002C1925,
    0x00578BF1, 0x00386BB1, 0x00CCE432, 0x004D4D85, 0x00FB9550, 0x00AAC83F, 0x009E18E2, 0x008A0B85,
    0x00AFE791, 0x009F679F, 0x00D89F58, 0x00E07BD8, 0x00F1A78E, 0x00A58C70, 0x0056D062, 0x0075EB96,
    0x00E36021, 0x00CE2E07, 0x000206F3, 0x00716FDB, 0x003E182C, 0x0024CCAE, 0x00D1EB41, 0x00681D20,
    0x00347074, 0x00DE0330, 0x00A1B90C, 0x00FEB55C, 0x00F2E598, 0x000BB02E, 0x00D63A3A, 0x00EA47CB,
    0x0023A18A, 0x00F1B1EE, 0x009DEFDA, 0x0086DE98, 0x003080E4, 0x00BC48C0, 0x0019ADB1, 0x00232D0A,
    0x001F56D9, 0x0002836D, 0x00F9E43B, 0x000A8D15, 0x00C5B1E8, 0x00DFCED6, 0x00185C7B, 0x007B884C,
    0x0015556A, 0x002EAB65, 0x0058FBFB, 0x006BE6D2, 0x002AB274, 0x004A81D5, 0x00AF37A2, 0x00D80086,
    0x004349B3, 0x008754F8, 0x005ABBDC, 0x00492046, 0x003158B8, 0x00C4040B, 0x00808E44, 0x006BB6B9,
    0x006E91EE, 0x000B3F4E, 0x00960C50, 0x00EE01CF, 0x006DB494, 0x00DA3BD1, 0x00F4683A, 0x00229714,
    0x002AA18B, 0x00C71E07, 0x008E8CAC, 0x00EBDDB7, 0x00156434, 0x000D0D40, 0x008594D8, 0x006CF215,
    0x006EE0AB, 0x0021244E, 0x0095E9BE, 0x00B14E1F, 0x00D23239, 0x000A4EAF, 0x00B200B8, 0x0000F340,
    0x008AD4EB, 0x00E336A9, 0x00A06590, 0x00E80846, 0x00247EE1, 0x00850869, 0x006C5AA3, 0x00730624,
    0x00D60A89, 0x0039871E, 0x00F46CA8, 0x00B76E7F, 0x00025151, 0x0044034F, 0x00227DFC, 0x004726CB,
    0x0099A84C, 0x006D677D, 0x0092253B, 0x000D0FA0, 0x00FA1331, 0x0044D2AA, 0x0012B2B2, 0x0042210A,
    0x00F8315C, 0x000F9097, 0x00BCFA41, 0x00DA3768, 0x00D77705, 0x00C68EEA, 0x0044D238, 0x00142103,
    0x00ED597C, 0x000199A7, 0x0045C11C, 0x008A01BE, 0x00AC2072, 0x00BF2BCA, 0x0031B260, 0x00ACD66C,
    0x00791058, 0x008EC01C, 0x00542658, 0x007E4833, 0x00A18814, 0x00AF9DEF, 0x004B0D3E, 0x006D9432,
    0x00DE85F3, 0x008B07A7, 0x0027D1DB, 0x0083AB20, 0x000EB9E3, 0x006BC30E, 0x00EC895E, 0x00CDEB04,
    0x00A7A9E4, 0x001BA87F, 0x0052CFE8, 0x007D8753, 0x0031D260, 0x00011AF8, 0x0092BB43, 0x00108905,
    0x007ADC80, 0x004E08EA, 0x00ACF77C, 0x006C4345, 0x0060E59C, 0x0084ACEE, 0x007830E1, 0x00674A14,
    0x0038C920, 0x00976ABF, 0x00D991F8, 0x005DFC88, 0x00B01873, 0x00E72A64, 0x001D6770, 0x00FFE539,
    0x00D470DC, 0x00D68DFE, 0x001F349D, 0x008D4DFC, 0x00DD567B, 0x0007CED6, 0x0069C799, 0x00F8AA2A,
    0x0019F69B, 0x004259CF, 0x00F044F7, 0x00368C70, 0x00894813, 0x0003BB07, 0x00E46987, 0x00821F0D,
    0x00CAFB7C, 0x00835768, 0x00E5F5AA, 0x00CF3300, 0x00E7AAB1, 0x008AF58C, 0x0045E5DA, 0x001A7D93,
    0x005BEC14, 0x00888ED1, 0x0044E10A, 0x0008CD1F, 0x0000D35E, 0x00E346BD, 0x008422C4, 0x00DCB12C,
    0x004A2B7E, 0x006605ED, 0x00787F35, 0x002A053B, 0x00808A30, 0x00C4017E, 0x003A0E29, 0x000AC883,
    0x003ADDE8, 0x00EB018D, 0x007EE978, 0x00ED6A8B, 0x006E50A4, 0x00D9D8D5, 0x007844CE, 0x00E0D57F
  };

  static const uint32_t gamma_table_ntsc[] = {
    0x00464AB2, 0x004E15E4, 0x006FBF86, 0x0080E7C2, 0x0023A82C, 0x00B5AA92, 0x0058D0CA, 0x00F85BCE,
    0x0010DB9F, 0x00972FBE, 0x0078622D, 0x00C9D81E, 0x00FB44B0, 0x0019C2AE, 0x00460C8F, 0x004DD72B,
    0x0045E359, 0x002043EA, 0x00BF087C, 0x00C42F42, 0x00C56A26, 0x004A0E83, 0x00A61160, 0x00B83D27,
    0x0078A6B0, 0x0007BF03, 0x006505BF, 0x006749C9, 0x00DAF450, 0x00167DA6, 0x00BA92AB, 0x00BF70FC,
    0x006FDCB1, 0x0085919C, 0x0034557C, 0x008C7663, 0x00DB00E4, 0x006CB3BD, 0x001E590F, 0x002C1925,
    0x00478BF1, 0x00B87BE1, 0x00CCE032, 0x006D4D81, 0x00DB9552, 0x00AB883F, 0x009E19E2, 0x000A0E81,
    0x002FE793, 0x00BF369F, 0x00DA9B58, 0x00E05AD8, 0x00F1A78E, 0x00A58D78, 0x0016D062, 0x0075EB96,
    0x00636021, 0x00CE2E07, 0x000306F3, 0x003565C3, 0x003E1834, 0x00248CAE, 0x0081EB41, 0x00689D20,
    0x00307474, 0x00DF0320, 0x00A9A908, 0x00FEB55C, 0x0072F598, 0x000BB020, 0x00D73A3A, 0x00EA45CF,
    0x0023818A, 0x00F9B1EF, 0x009DAFDA, 0x0084C69E, 0x00B2A8E4, 0x00BC4AC8, 0x0019ADF1, 0x00022D28,
    0x001F54DB, 0x00008365, 0x00FBF413, 0x000AAD2D, 0x00C5B1E8, 0x00DFCE4E, 0x00185FFB, 0x007B884C,
    0x0015556C, 0x002EBB65, 0x0098FBFB, 0x002BE6D2, 0x000AB27C, 0x004A81F9, 0x00AF77A2, 0x00C93006,
    0x004349B3, 0x00A654B8, 0x004AFBDC, 0x00C92046, 0x00B158BA, 0x00E4040B, 0x00808EC4, 0x006AB6B9,
    0x00DE91CE, 0x00033F4E, 0x00D40850, 0x00CE01CF, 0x004DB495, 0x00DA7BD1, 0x00F4685A, 0x00A2173E,
    0x002A01AF, 0x00559E07, 0x008EACAC, 0x00EBDDBF, 0x0055643D, 0x00494D4A, 0x0085D0D8, 0x006CF217,
    0x006FF02B, 0x0021654E, 0x00D4F99E, 0x00B04E3F, 0x00C2323B, 0x000A0EAF, 0x00B200BA, 0x0004F248,
    0x0082D4EB, 0x00E336A9, 0x00B06591, 0x00A40842, 0x00247FE5, 0x00970C69, 0x00EC7AA3, 0x00330624,
    0x00D60A09, 0x0079C71E, 0x00F47CA8, 0x00B74E6F, 0x00001151, 0x0064234D, 0x00665CFD, 0x004726EB,
    0x0099A04C, 0x006F276D, 0x0092253B, 0x000D0FA0, 0x00F23331, 0x0044DAAA, 0x0016B2B2, 0x0042210A,
    0x00702B5C, 0x000D9097, 0x00BDFA41, 0x00DA3668, 0x00C75305, 0x00C68EEB, 0x004CD338, 0x0034210B,
    0x006D514C, 0x002199A7, 0x0045811C, 0x00AA00BE, 0x00BC3260, 0x00ADAB8A, 0x00B1B068, 0x00ACD44C,
    0x00790058, 0x009EC09C, 0x00142748, 0x003E8823, 0x00A98816, 0x00AF9CFF, 0x0049553E, 0x00ED952A,
    0x00DE85F6, 0x008B07A7, 0x0027D1CB, 0x0083BB60, 0x000EB1A1, 0x006BC70E, 0x00EC8956, 0x00D9FB04,
    0x00A78DE4, 0x000BA87F, 0x0052CFF8, 0x007D0743, 0x0031D360, 0x00111CBC, 0x0092BF41, 0x00109915,
    0x007ADCC8, 0x004E08EB, 0x00B8F77C, 0x00666367, 0x0060E5FC, 0x0084ACEE, 0x00F860E1, 0x00674A15,
    0x003CC9A4, 0x00877ABF, 0x00D991F8, 0x005DFC88, 0x00B08DD3, 0x00F72A64, 0x000D6770, 0x007FE539,
    0x00D474F8, 0x00548DBE, 0x003F3495, 0x009D4D7C, 0x00DD467B, 0x0007CED6, 0x0069C719, 0x00F8A82A,
    0x0019F613, 0x005279CF, 0x00F064F7, 0x00B68C70, 0x00894813, 0x0003BB07, 0x00E22987, 0x00861F4D,
    0x00CAF93C, 0x0083576C, 0x00E5F5A2, 0x00CF3300, 0x00E7AA91, 0x008AF189, 0x0075E0DA, 0x001879D3,
    0x004AEC7C, 0x000C8E52, 0x0044E50A, 0x0088CD5F, 0x0010F37C, 0x00E2569D, 0x008422C0, 0x00DCB12C,
    0x004A2F7E, 0x002605CD, 0x00FA7F37, 0x002A042F, 0x00888AF0, 0x0084017E, 0x00384E29, 0x0082D80B,
    0x00BA5DE8, 0x00F9018D, 0x007EE978, 0x00EC6A8B, 0x006E50A4, 0x00D9D8D5, 0x0078444E, 0x00E0D55F
  };
  // clang-format on

  for (uint32_t i = 0; i < 0x80; ++i)
    LCD1_TCON1_GAMMA_TABLE(i) = pal ? gamma_table_pal[i] : gamma_table_ntsc[i];

  tve_update_buffer();

  // XXX: This should be a vblank interrupt, not a line interrupt.
  LCD1_GINT1 = 1;
  LCD1_GINT0 = BIT(30);
  irq_enable(119);
}

void tve_de2_init(void)
{
  DE_MIXER1_GLB_SIZE        = DE_SIZE_PHYS;
  DE_MIXER1_BLD_SIZE        = DE_SIZE_PHYS;
  DE_MIXER1_BLD_CH_ISIZE(0) = DE_SIZE_PHYS;

  // The output takes a dsp.x*dsp.y area from a total (dsp.x+dsp.ovx)*(dsp.y+dsp.ovy) buffer
  DE_MIXER1_OVL_UI_MBSIZE(0) = DE_SIZE(dsp.x, dsp.y);
  DE_MIXER1_OVL_UI_COOR(0)   = 0;
  DE_MIXER1_OVL_UI_PITCH(0)  = dsp.fb_width * 4;  // Scan line in bytes including overscan
  DE_MIXER1_OVL_UI_SIZE      = DE_SIZE(dsp.x, dsp.y);

  DE_MIXER1_UIS_CTRL(0)     = 0;  // off

  DE_MIXER1_UIS_OUT_SIZE(0) = DE_SIZE_PHYS;
  DE_MIXER1_UIS_IN_SIZE(0)  = DE_SIZE(dsp.x, dsp.y);

  double scale_x            = (double)dsp.x * (double)0x100000 / (double)DISPLAY_PHYS_RES_X;
  DE_MIXER1_UIS_HSTEP(0)    = (uint32_t)scale_x;

  double scale_y            = (double)dsp.y * (double)0x100000 / (double)DISPLAY_PHYS_RES_Y;
  DE_MIXER1_UIS_VSTEP(0)    = (uint32_t)scale_y;

  for (int n = 0; n < 16; ++n) {
    DE_MIXER1_UIS_HCOEF(0, n) = 0x40000000;
  }

  DE_MIXER1_UIS_CTRL(0) = 0x11;
  tve_update_buffer();
}

void tve_set_visible_buffer(volatile uint32_t *buf)
{
  DE_MIXER1_OVL_UI_TOP_LADD(0) = (uint32_t)buf;
  DE_MIXER1_OVL_UI_BOT_LADD(0) = (uint32_t)buf;  // XXX: necessary?
}
