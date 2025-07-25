// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2012-2013 Samsung Electronics Co., Ltd.
 */

#include <linux/string.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include <asm/unaligned.h>

#include "exfat_raw.h"
#include "exfat_fs.h"

/* Upcase table macro */
#define EXFAT_NUM_UPCASE	(2918)
#define UTBL_COUNT		(0x10000)

/*
 * Upcase table in compressed format (7.2.5.1 Recommended Up-case Table
 * in exfat specification, See:
 * https://docs.microsoft.com/en-us/windows/win32/fileio/exfat-specification).
 */
static const unsigned short uni_def_upcase[EXFAT_NUM_UPCASE] = {
	0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
	0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
	0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
	0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f,
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
	0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
	0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
	0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
	0x0060, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
	0x0058, 0x0059, 0x005a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f,
	0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087,
	0x0088, 0x0089, 0x008a, 0x008b, 0x008c, 0x008d, 0x008e, 0x008f,
	0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
	0x0098, 0x0099, 0x009a, 0x009b, 0x009c, 0x009d, 0x009e, 0x009f,
	0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,
	0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af,
	0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,
	0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
	0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7,
	0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf,
	0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,
	0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df,
	0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7,
	0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf,
	0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00f7,
	0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x0178,
	0x0100, 0x0100, 0x0102, 0x0102, 0x0104, 0x0104, 0x0106, 0x0106,
	0x0108, 0x0108, 0x010a, 0x010a, 0x010c, 0x010c, 0x010e, 0x010e,
	0x0110, 0x0110, 0x0112, 0x0112, 0x0114, 0x0114, 0x0116, 0x0116,
	0x0118, 0x0118, 0x011a, 0x011a, 0x011c, 0x011c, 0x011e, 0x011e,
	0x0120, 0x0120, 0x0122, 0x0122, 0x0124, 0x0124, 0x0126, 0x0126,
	0x0128, 0x0128, 0x012a, 0x012a, 0x012c, 0x012c, 0x012e, 0x012e,
	0x0130, 0x0131, 0x0132, 0x0132, 0x0134, 0x0134, 0x0136, 0x0136,
	0x0138, 0x0139, 0x0139, 0x013b, 0x013b, 0x013d, 0x013d, 0x013f,
	0x013f, 0x0141, 0x0141, 0x0143, 0x0143, 0x0145, 0x0145, 0x0147,
	0x0147, 0x0149, 0x014a, 0x014a, 0x014c, 0x014c, 0x014e, 0x014e,
	0x0150, 0x0150, 0x0152, 0x0152, 0x0154, 0x0154, 0x0156, 0x0156,
	0x0158, 0x0158, 0x015a, 0x015a, 0x015c, 0x015c, 0x015e, 0x015e,
	0x0160, 0x0160, 0x0162, 0x0162, 0x0164, 0x0164, 0x0166, 0x0166,
	0x0168, 0x0168, 0x016a, 0x016a, 0x016c, 0x016c, 0x016e, 0x016e,
	0x0170, 0x0170, 0x0172, 0x0172, 0x0174, 0x0174, 0x0176, 0x0176,
	0x0178, 0x0179, 0x0179, 0x017b, 0x017b, 0x017d, 0x017d, 0x017f,
	0x0243, 0x0181, 0x0182, 0x0182, 0x0184, 0x0184, 0x0186, 0x0187,
	0x0187, 0x0189, 0x018a, 0x018b, 0x018b, 0x018d, 0x018e, 0x018f,
	0x0190, 0x0191, 0x0191, 0x0193, 0x0194, 0x01f6, 0x0196, 0x0197,
	0x0198, 0x0198, 0x023d, 0x019b, 0x019c, 0x019d, 0x0220, 0x019f,
	0x01a0, 0x01a0, 0x01a2, 0x01a2, 0x01a4, 0x01a4, 0x01a6, 0x01a7,
	0x01a7, 0x01a9, 0x01aa, 0x01ab, 0x01ac, 0x01ac, 0x01ae, 0x01af,
	0x01af, 0x01b1, 0x01b2, 0x01b3, 0x01b3, 0x01b5, 0x01b5, 0x01b7,
	0x01b8, 0x01b8, 0x01ba, 0x01bb, 0x01bc, 0x01bc, 0x01be, 0x01f7,
	0x01c0, 0x01c1, 0x01c2, 0x01c3, 0x01c4, 0x01c5, 0x01c4, 0x01c7,
	0x01c8, 0x01c7, 0x01ca, 0x01cb, 0x01ca, 0x01cd, 0x01cd, 0x01cf,
	0x01cf, 0x01d1, 0x01d1, 0x01d3, 0x01d3, 0x01d5, 0x01d5, 0x01d7,
	0x01d7, 0x01d9, 0x01d9, 0x01db, 0x01db, 0x018e, 0x01de, 0x01de,
	0x01e0, 0x01e0, 0x01e2, 0x01e2, 0x01e4, 0x01e4, 0x01e6, 0x01e6,
	0x01e8, 0x01e8, 0x01ea, 0x01ea, 0x01ec, 0x01ec, 0x01ee, 0x01ee,
	0x01f0, 0x01f1, 0x01f2, 0x01f1, 0x01f4, 0x01f4, 0x01f6, 0x01f7,
	0x01f8, 0x01f8, 0x01fa, 0x01fa, 0x01fc, 0x01fc, 0x01fe, 0x01fe,
	0x0200, 0x0200, 0x0202, 0x0202, 0x0204, 0x0204, 0x0206, 0x0206,
	0x0208, 0x0208, 0x020a, 0x020a, 0x020c, 0x020c, 0x020e, 0x020e,
	0x0210, 0x0210, 0x0212, 0x0212, 0x0214, 0x0214, 0x0216, 0x0216,
	0x0218, 0x0218, 0x021a, 0x021a, 0x021c, 0x021c, 0x021e, 0x021e,
	0x0220, 0x0221, 0x0222, 0x0222, 0x0224, 0x0224, 0x0226, 0x0226,
	0x0228, 0x0228, 0x022a, 0x022a, 0x022c, 0x022c, 0x022e, 0x022e,
	0x0230, 0x0230, 0x0232, 0x0232, 0x0234, 0x0235, 0x0236, 0x0237,
	0x0238, 0x0239, 0x2c65, 0x023b, 0x023b, 0x023d, 0x2c66, 0x023f,
	0x0240, 0x0241, 0x0241, 0x0243, 0x0244, 0x0245, 0x0246, 0x0246,
	0x0248, 0x0248, 0x024a, 0x024a, 0x024c, 0x024c, 0x024e, 0x024e,
	0x0250, 0x0251, 0x0252, 0x0181, 0x0186, 0x0255, 0x0189, 0x018a,
	0x0258, 0x018f, 0x025a, 0x0190, 0x025c, 0x025d, 0x025e, 0x025f,
	0x0193, 0x0261, 0x0262, 0x0194, 0x0264, 0x0265, 0x0266, 0x0267,
	0x0197, 0x0196, 0x026a, 0x2c62, 0x026c, 0x026d, 0x026e, 0x019c,
	0x0270, 0x0271, 0x019d, 0x0273, 0x0274, 0x019f, 0x0276, 0x0277,
	0x0278, 0x0279, 0x027a, 0x027b, 0x027c, 0x2c64, 0x027e, 0x027f,
	0x01a6, 0x0281, 0x0282, 0x01a9, 0x0284, 0x0285, 0x0286, 0x0287,
	0x01ae, 0x0244, 0x01b1, 0x01b2, 0x0245, 0x028d, 0x028e, 0x028f,
	0x0290, 0x0291, 0x01b7, 0x0293, 0x0294, 0x0295, 0x0296, 0x0297,
	0x0298, 0x0299, 0x029a, 0x029b, 0x029c, 0x029d, 0x029e, 0x029f,
	0x02a0, 0x02a1, 0x02a2, 0x02a3, 0x02a4, 0x02a5, 0x02a6, 0x02a7,
	0x02a8, 0x02a9, 0x02aa, 0x02ab, 0x02ac, 0x02ad, 0x02ae, 0x02af,
	0x02b0, 0x02b1, 0x02b2, 0x02b3, 0x02b4, 0x02b5, 0x02b6, 0x02b7,
	0x02b8, 0x02b9, 0x02ba, 0x02bb, 0x02bc, 0x02bd, 0x02be, 0x02bf,
	0x02c0, 0x02c1, 0x02c2, 0x02c3, 0x02c4, 0x02c5, 0x02c6, 0x02c7,
	0x02c8, 0x02c9, 0x02ca, 0x02cb, 0x02cc, 0x02cd, 0x02ce, 0x02cf,
	0x02d0, 0x02d1, 0x02d2, 0x02d3, 0x02d4, 0x02d5, 0x02d6, 0x02d7,
	0x02d8, 0x02d9, 0x02da, 0x02db, 0x02dc, 0x02dd, 0x02de, 0x02df,
	0x02e0, 0x02e1, 0x02e2, 0x02e3, 0x02e4, 0x02e5, 0x02e6, 0x02e7,
	0x02e8, 0x02e9, 0x02ea, 0x02eb, 0x02ec, 0x02ed, 0x02ee, 0x02ef,
	0x02f0, 0x02f1, 0x02f2, 0x02f3, 0x02f4, 0x02f5, 0x02f6, 0x02f7,
	0x02f8, 0x02f9, 0x02fa, 0x02fb, 0x02fc, 0x02fd, 0x02fe, 0x02ff,
	0x0300, 0x0301, 0x0302, 0x0303, 0x0304, 0x0305, 0x0306, 0x0307,
	0x0308, 0x0309, 0x030a, 0x030b, 0x030c, 0x030d, 0x030e, 0x030f,
	0x0310, 0x0311, 0x0312, 0x0313, 0x0314, 0x0315, 0x0316, 0x0317,
	0x0318, 0x0319, 0x031a, 0x031b, 0x031c, 0x031d, 0x031e, 0x031f,
	0x0320, 0x0321, 0x0322, 0x0323, 0x0324, 0x0325, 0x0326, 0x0327,
	0x0328, 0x0329, 0x032a, 0x032b, 0x032c, 0x032d, 0x032e, 0x032f,
	0x0330, 0x0331, 0x0332, 0x0333, 0x0334, 0x0335, 0x0336, 0x0337,
	0x0338, 0x0339, 0x033a, 0x033b, 0x033c, 0x033d, 0x033e, 0x033f,
	0x0340, 0x0341, 0x0342, 0x0343, 0x0344, 0x0345, 0x0346, 0x0347,
	0x0348, 0x0349, 0x034a, 0x034b, 0x034c, 0x034d, 0x034e, 0x034f,
	0x0350, 0x0351, 0x0352, 0x0353, 0x0354, 0x0355, 0x0356, 0x0357,
	0x0358, 0x0359, 0x035a, 0x035b, 0x035c, 0x035d, 0x035e, 0x035f,
	0x0360, 0x0361, 0x0362, 0x0363, 0x0364, 0x0365, 0x0366, 0x0367,
	0x0368, 0x0369, 0x036a, 0x036b, 0x036c, 0x036d, 0x036e, 0x036f,
	0x0370, 0x0371, 0x0372, 0x0373, 0x0374, 0x0375, 0x0376, 0x0377,
	0x0378, 0x0379, 0x037a, 0x03fd, 0x03fe, 0x03ff, 0x037e, 0x037f,
	0x0380, 0x0381, 0x0382, 0x0383, 0x0384, 0x0385, 0x0386, 0x0387,
	0x0388, 0x0389, 0x038a, 0x038b, 0x038c, 0x038d, 0x038e, 0x038f,
	0x0390, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397,
	0x0398, 0x0399, 0x039a, 0x039b, 0x039c, 0x039d, 0x039e, 0x039f,
	0x03a0, 0x03a1, 0x03a2, 0x03a3, 0x03a4, 0x03a5, 0x03a6, 0x03a7,
	0x03a8, 0x03a9, 0x03aa, 0x03ab, 0x0386, 0x0388, 0x0389, 0x038a,
	0x03b0, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397,
	0x0398, 0x0399, 0x039a, 0x039b, 0x039c, 0x039d, 0x039e, 0x039f,
	0x03a0, 0x03a1, 0x03a3, 0x03a3, 0x03a4, 0x03a5, 0x03a6, 0x03a7,
	0x03a8, 0x03a9, 0x03aa, 0x03ab, 0x038c, 0x038e, 0x038f, 0x03cf,
	0x03d0, 0x03d1, 0x03d2, 0x03d3, 0x03d4, 0x03d5, 0x03d6, 0x03d7,
	0x03d8, 0x03d8, 0x03da, 0x03da, 0x03dc, 0x03dc, 0x03de, 0x03de,
	0x03e0, 0x03e0, 0x03e2, 0x03e2, 0x03e4, 0x03e4, 0x03e6, 0x03e6,
	0x03e8, 0x03e8, 0x03ea, 0x03ea, 0x03ec, 0x03ec, 0x03ee, 0x03ee,
	0x03f0, 0x03f1, 0x03f9, 0x03f3, 0x03f4, 0x03f5, 0x03f6, 0x03f7,
	0x03f7, 0x03f9, 0x03fa, 0x03fa, 0x03fc, 0x03fd, 0x03fe, 0x03ff,
	0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407,
	0x0408, 0x0409, 0x040a, 0x040b, 0x040c, 0x040d, 0x040e, 0x040f,
	0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
	0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f,
	0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
	0x0428, 0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f,
	0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
	0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f,
	0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
	0x0428, 0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f,
	0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407,
	0x0408, 0x0409, 0x040a, 0x040b, 0x040c, 0x040d, 0x040e, 0x040f,
	0x0460, 0x0460, 0x0462, 0x0462, 0x0464, 0x0464, 0x0466, 0x0466,
	0x0468, 0x0468, 0x046a, 0x046a, 0x046c, 0x046c, 0x046e, 0x046e,
	0x0470, 0x0470, 0x0472, 0x0472, 0x0474, 0x0474, 0x0476, 0x0476,
	0x0478, 0x0478, 0x047a, 0x047a, 0x047c, 0x047c, 0x047e, 0x047e,
	0x0480, 0x0480, 0x0482, 0x0483, 0x0484, 0x0485, 0x0486, 0x0487,
	0x0488, 0x0489, 0x048a, 0x048a, 0x048c, 0x048c, 0x048e, 0x048e,
	0x0490, 0x0490, 0x0492, 0x0492, 0x0494, 0x0494, 0x0496, 0x0496,
	0x0498, 0x0498, 0x049a, 0x049a, 0x049c, 0x049c, 0x049e, 0x049e,
	0x04a0, 0x04a0, 0x04a2, 0x04a2, 0x04a4, 0x04a4, 0x04a6, 0x04a6,
	0x04a8, 0x04a8, 0x04aa, 0x04aa, 0x04ac, 0x04ac, 0x04ae, 0x04ae,
	0x04b0, 0x04b0, 0x04b2, 0x04b2, 0x04b4, 0x04b4, 0x04b6, 0x04b6,
	0x04b8, 0x04b8, 0x04ba, 0x04ba, 0x04bc, 0x04bc, 0x04be, 0x04be,
	0x04c0, 0x04c1, 0x04c1, 0x04c3, 0x04c3, 0x04c5, 0x04c5, 0x04c7,
	0x04c7, 0x04c9, 0x04c9, 0x04cb, 0x04cb, 0x04cd, 0x04cd, 0x04c0,
	0x04d0, 0x04d0, 0x04d2, 0x04d2, 0x04d4, 0x04d4, 0x04d6, 0x04d6,
	0x04d8, 0x04d8, 0x04da, 0x04da, 0x04dc, 0x04dc, 0x04de, 0x04de,
	0x04e0, 0x04e0, 0x04e2, 0x04e2, 0x04e4, 0x04e4, 0x04e6, 0x04e6,
	0x04e8, 0x04e8, 0x04ea, 0x04ea, 0x04ec, 0x04ec, 0x04ee, 0x04ee,
	0x04f0, 0x04f0, 0x04f2, 0x04f2, 0x04f4, 0x04f4, 0x04f6, 0x04f6,
	0x04f8, 0x04f8, 0x04fa, 0x04fa, 0x04fc, 0x04fc, 0x04fe, 0x04fe,
	0x0500, 0x0500, 0x0502, 0x0502, 0x0504, 0x0504, 0x0506, 0x0506,
	0x0508, 0x0508, 0x050a, 0x050a, 0x050c, 0x050c, 0x050e, 0x050e,
	0x0510, 0x0510, 0x0512, 0x0512, 0x0514, 0x0515, 0x0516, 0x0517,
	0x0518, 0x0519, 0x051a, 0x051b, 0x051c, 0x051d, 0x051e, 0x051f,
	0x0520, 0x0521, 0x0522, 0x0523, 0x0524, 0x0525, 0x0526, 0x0527,
	0x0528, 0x0529, 0x052a, 0x052b, 0x052c, 0x052d, 0x052e, 0x052f,
	0x0530, 0x0531, 0x0532, 0x0533, 0x0534, 0x0535, 0x0536, 0x0537,
	0x0538, 0x0539, 0x053a, 0x053b, 0x053c, 0x053d, 0x053e, 0x053f,
	0x0540, 0x0541, 0x0542, 0x0543, 0x0544, 0x0545, 0x0546, 0x0547,
	0x0548, 0x0549, 0x054a, 0x054b, 0x054c, 0x054d, 0x054e, 0x054f,
	0x0550, 0x0551, 0x0552, 0x0553, 0x0554, 0x0555, 0x0556, 0x0557,
	0x0558, 0x0559, 0x055a, 0x055b, 0x055c, 0x055d, 0x055e, 0x055f,
	0x0560, 0x0531, 0x0532, 0x0533, 0x0534, 0x0535, 0x0536, 0x0537,
	0x0538, 0x0539, 0x053a, 0x053b, 0x053c, 0x053d, 0x053e, 0x053f,
	0x0540, 0x0541, 0x0542, 0x0543, 0x0544, 0x0545, 0x0546, 0x0547,
	0x0548, 0x0549, 0x054a, 0x054b, 0x054c, 0x054d, 0x054e, 0x054f,
	0x0550, 0x0551, 0x0552, 0x0553, 0x0554, 0x0555, 0x0556, 0xffff,
	0x17f6, 0x2c63, 0x1d7e, 0x1d7f, 0x1d80, 0x1d81, 0x1d82, 0x1d83,
	0x1d84, 0x1d85, 0x1d86, 0x1d87, 0x1d88, 0x1d89, 0x1d8a, 0x1d8b,
	0x1d8c, 0x1d8d, 0x1d8e, 0x1d8f, 0x1d90, 0x1d91, 0x1d92, 0x1d93,
	0x1d94, 0x1d95, 0x1d96, 0x1d97, 0x1d98, 0x1d99, 0x1d9a, 0x1d9b,
	0x1d9c, 0x1d9d, 0x1d9e, 0x1d9f, 0x1da0, 0x1da1, 0x1da2, 0x1da3,
	0x1da4, 0x1da5, 0x1da6, 0x1da7, 0x1da8, 0x1da9, 0x1daa, 0x1dab,
	0x1dac, 0x1dad, 0x1dae, 0x1daf, 0x1db0, 0x1db1, 0x1db2, 0x1db3,
	0x1db4, 0x1db5, 0x1db6, 0x1db7, 0x1db8, 0x1db9, 0x1dba, 0x1dbb,
	0x1dbc, 0x1dbd, 0x1dbe, 0x1dbf, 0x1dc0, 0x1dc1, 0x1dc2, 0x1dc3,
	0x1dc4, 0x1dc5, 0x1dc6, 0x1dc7, 0x1dc8, 0x1dc9, 0x1dca, 0x1dcb,
	0x1dcc, 0x1dcd, 0x1dce, 0x1dcf, 0x1dd0, 0x1dd1, 0x1dd2, 0x1dd3,
	0x1dd4, 0x1dd5, 0x1dd6, 0x1dd7, 0x1dd8, 0x1dd9, 0x1dda, 0x1ddb,
	0x1ddc, 0x1ddd, 0x1dde, 0x1ddf, 0x1de0, 0x1de1, 0x1de2, 0x1de3,
	0x1de4, 0x1de5, 0x1de6, 0x1de7, 0x1de8, 0x1de9, 0x1dea, 0x1deb,
	0x1dec, 0x1ded, 0x1dee, 0x1def, 0x1df0, 0x1df1, 0x1df2, 0x1df3,
	0x1df4, 0x1df5, 0x1df6, 0x1df7, 0x1df8, 0x1df9, 0x1dfa, 0x1dfb,
	0x1dfc, 0x1dfd, 0x1dfe, 0x1dff, 0x1e00, 0x1e00, 0x1e02, 0x1e02,
	0x1e04, 0x1e04, 0x1e06, 0x1e06, 0x1e08, 0x1e08, 0x1e0a, 0x1e0a,
	0x1e0c, 0x1e0c, 0x1e0e, 0x1e0e, 0x1e10, 0x1e10, 0x1e12, 0x1e12,
	0x1e14, 0x1e14, 0x1e16, 0x1e16, 0x1e18, 0x1e18, 0x1e1a, 0x1e1a,
	0x1e1c, 0x1e1c, 0x1e1e, 0x1e1e, 0x1e20, 0x1e20, 0x1e22, 0x1e22,
	0x1e24, 0x1e24, 0x1e26, 0x1e26, 0x1e28, 0x1e28, 0x1e2a, 0x1e2a,
	0x1e2c, 0x1e2c, 0x1e2e, 0x1e2e, 0x1e30, 0x1e30, 0x1e32, 0x1e32,
	0x1e34, 0x1e34, 0x1e36, 0x1e36, 0x1e38, 0x1e38, 0x1e3a, 0x1e3a,
	0x1e3c, 0x1e3c, 0x1e3e, 0x1e3e, 0x1e40, 0x1e40, 0x1e42, 0x1e42,
	0x1e44, 0x1e44, 0x1e46, 0x1e46, 0x1e48, 0x1e48, 0x1e4a, 0x1e4a,
	0x1e4c, 0x1e4c, 0x1e4e, 0x1e4e, 0x1e50, 0x1e50, 0x1e52, 0x1e52,
	0x1e54, 0x1e54, 0x1e56, 0x1e56, 0x1e58, 0x1e58, 0x1e5a, 0x1e5a,
	0x1e5c, 0x1e5c, 0x1e5e, 0x1e5e, 0x1e60, 0x1e60, 0x1e62, 0x1e62,
	0x1e64, 0x1e64, 0x1e66, 0x1e66, 0x1e68, 0x1e68, 0x1e6a, 0x1e6a,
	0x1e6c, 0x1e6c, 0x1e6e, 0x1e6e, 0x1e70, 0x1e70, 0x1e72, 0x1e72,
	0x1e74, 0x1e74, 0x1e76, 0x1e76, 0x1e78, 0x1e78, 0x1e7a, 0x1e7a,
	0x1e7c, 0x1e7c, 0x1e7e, 0x1e7e, 0x1e80, 0x1e80, 0x1e82, 0x1e82,
	0x1e84, 0x1e84, 0x1e86, 0x1e86, 0x1e88, 0x1e88, 0x1e8a, 0x1e8a,
	0x1e8c, 0x1e8c, 0x1e8e, 0x1e8e, 0x1e90, 0x1e90, 0x1e92, 0x1e92,
	0x1e94, 0x1e94, 0x1e96, 0x1e97, 0x1e98, 0x1e99, 0x1e9a, 0x1e9b,
	0x1e9c, 0x1e9d, 0x1e9e, 0x1e9f, 0x1ea0, 0x1ea0, 0x1ea2, 0x1ea2,
	0x1ea4, 0x1ea4, 0x1ea6, 0x1ea6, 0x1ea8, 0x1ea8, 0x1eaa, 0x1eaa,
	0x1eac, 0x1eac, 0x1eae, 0x1eae, 0x1eb0, 0x1eb0, 0x1eb2, 0x1eb2,
	0x1eb4, 0x1eb4, 0x1eb6, 0x1eb6, 0x1eb8, 0x1eb8, 0x1eba, 0x1eba,
	0x1ebc, 0x1ebc, 0x1ebe, 0x1ebe, 0x1ec0, 0x1ec0, 0x1ec2, 0x1ec2,
	0x1ec4, 0x1ec4, 0x1ec6, 0x1ec6, 0x1ec8, 0x1ec8, 0x1eca, 0x1eca,
	0x1ecc, 0x1ecc, 0x1ece, 0x1ece, 0x1ed0, 0x1ed0, 0x1ed2, 0x1ed2,
	0x1ed4, 0x1ed4, 0x1ed6, 0x1ed6, 0x1ed8, 0x1ed8, 0x1eda, 0x1eda,
	0x1edc, 0x1edc, 0x1ede, 0x1ede, 0x1ee0, 0x1ee0, 0x1ee2, 0x1ee2,
	0x1ee4, 0x1ee4, 0x1ee6, 0x1ee6, 0x1ee8, 0x1ee8, 0x1eea, 0x1eea,
	0x1eec, 0x1eec, 0x1eee, 0x1eee, 0x1ef0, 0x1ef0, 0x1ef2, 0x1ef2,
	0x1ef4, 0x1ef4, 0x1ef6, 0x1ef6, 0x1ef8, 0x1ef8, 0x1efa, 0x1efb,
	0x1efc, 0x1efd, 0x1efe, 0x1eff, 0x1f08, 0x1f09, 0x1f0a, 0x1f0b,
	0x1f0c, 0x1f0d, 0x1f0e, 0x1f0f, 0x1f08, 0x1f09, 0x1f0a, 0x1f0b,
	0x1f0c, 0x1f0d, 0x1f0e, 0x1f0f, 0x1f18, 0x1f19, 0x1f1a, 0x1f1b,
	0x1f1c, 0x1f1d, 0x1f16, 0x1f17, 0x1f18, 0x1f19, 0x1f1a, 0x1f1b,
	0x1f1c, 0x1f1d, 0x1f1e, 0x1f1f, 0x1f28, 0x1f29, 0x1f2a, 0x1f2b,
	0x1f2c, 0x1f2d, 0x1f2e, 0x1f2f, 0x1f28, 0x1f29, 0x1f2a, 0x1f2b,
	0x1f2c, 0x1f2d, 0x1f2e, 0x1f2f, 0x1f38, 0x1f39, 0x1f3a, 0x1f3b,
	0x1f3c, 0x1f3d, 0x1f3e, 0x1f3f, 0x1f38, 0x1f39, 0x1f3a, 0x1f3b,
	0x1f3c, 0x1f3d, 0x1f3e, 0x1f3f, 0x1f48, 0x1f49, 0x1f4a, 0x1f4b,
	0x1f4c, 0x1f4d, 0x1f46, 0x1f47, 0x1f48, 0x1f49, 0x1f4a, 0x1f4b,
	0x1f4c, 0x1f4d, 0x1f4e, 0x1f4f, 0x1f50, 0x1f59, 0x1f52, 0x1f5b,
	0x1f54, 0x1f5d, 0x1f56, 0x1f5f, 0x1f58, 0x1f59, 0x1f5a, 0x1f5b,
	0x1f5c, 0x1f5d, 0x1f5e, 0x1f5f, 0x1f68, 0x1f69, 0x1f6a, 0x1f6b,
	0x1f6c, 0x1f6d, 0x1f6e, 0x1f6f, 0x1f68, 0x1f69, 0x1f6a, 0x1f6b,
	0x1f6c, 0x1f6d, 0x1f6e, 0x1f6f, 0x1fba, 0x1fbb, 0x1fc8, 0x1fc9,
	0x1fca, 0x1fcb, 0x1fda, 0x1fdb, 0x1ff8, 0x1ff9, 0x1fea, 0x1feb,
	0x1ffa, 0x1ffb, 0x1f7e, 0x1f7f, 0x1f88, 0x1f89, 0x1f8a, 0x1f8b,
	0x1f8c, 0x1f8d, 0x1f8e, 0x1f8f, 0x1f88, 0x1f89, 0x1f8a, 0x1f8b,
	0x1f8c, 0x1f8d, 0x1f8e, 0x1f8f, 0x1f98, 0x1f99, 0x1f9a, 0x1f9b,
	0x1f9c, 0x1f9d, 0x1f9e, 0x1f9f, 0x1f98, 0x1f99, 0x1f9a, 0x1f9b,
	0x1f9c, 0x1f9d, 0x1f9e, 0x1f9f, 0x1fa8, 0x1fa9, 0x1faa, 0x1fab,
	0x1fac, 0x1fad, 0x1fae, 0x1faf, 0x1fa8, 0x1fa9, 0x1faa, 0x1fab,
	0x1fac, 0x1fad, 0x1fae, 0x1faf, 0x1fb8, 0x1fb9, 0x1fb2, 0x1fbc,
	0x1fb4, 0x1fb5, 0x1fb6, 0x1fb7, 0x1fb8, 0x1fb9, 0x1fba, 0x1fbb,
	0x1fbc, 0x1fbd, 0x1fbe, 0x1fbf, 0x1fc0, 0x1fc1, 0x1fc2, 0x1fc3,
	0x1fc4, 0x1fc5, 0x1fc6, 0x1fc7, 0x1fc8, 0x1fc9, 0x1fca, 0x1fcb,
	0x1fc3, 0x1fcd, 0x1fce, 0x1fcf, 0x1fd8, 0x1fd9, 0x1fd2, 0x1fd3,
	0x1fd4, 0x1fd5, 0x1fd6, 0x1fd7, 0x1fd8, 0x1fd9, 0x1fda, 0x1fdb,
	0x1fdc, 0x1fdd, 0x1fde, 0x1fdf, 0x1fe8, 0x1fe9, 0x1fe2, 0x1fe3,
	0x1fe4, 0x1fec, 0x1fe6, 0x1fe7, 0x1fe8, 0x1fe9, 0x1fea, 0x1feb,
	0x1fec, 0x1fed, 0x1fee, 0x1fef, 0x1ff0, 0x1ff1, 0x1ff2, 0x1ff3,
	0x1ff4, 0x1ff5, 0x1ff6, 0x1ff7, 0x1ff8, 0x1ff9, 0x1ffa, 0x1ffb,
	0x1ff3, 0x1ffd, 0x1ffe, 0x1fff, 0x2000, 0x2001, 0x2002, 0x2003,
	0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200a, 0x200b,
	0x200c, 0x200d, 0x200e, 0x200f, 0x2010, 0x2011, 0x2012, 0x2013,
	0x2014, 0x2015, 0x2016, 0x2017, 0x2018, 0x2019, 0x201a, 0x201b,
	0x201c, 0x201d, 0x201e, 0x201f, 0x2020, 0x2021, 0x2022, 0x2023,
	0x2024, 0x2025, 0x2026, 0x2027, 0x2028, 0x2029, 0x202a, 0x202b,
	0x202c, 0x202d, 0x202e, 0x202f, 0x2030, 0x2031, 0x2032, 0x2033,
	0x2034, 0x2035, 0x2036, 0x2037, 0x2038, 0x2039, 0x203a, 0x203b,
	0x203c, 0x203d, 0x203e, 0x203f, 0x2040, 0x2041, 0x2042, 0x2043,
	0x2044, 0x2045, 0x2046, 0x2047, 0x2048, 0x2049, 0x204a, 0x204b,
	0x204c, 0x204d, 0x204e, 0x204f, 0x2050, 0x2051, 0x2052, 0x2053,
	0x2054, 0x2055, 0x2056, 0x2057, 0x2058, 0x2059, 0x205a, 0x205b,
	0x205c, 0x205d, 0x205e, 0x205f, 0x2060, 0x2061, 0x2062, 0x2063,
	0x2064, 0x2065, 0x2066, 0x2067, 0x2068, 0x2069, 0x206a, 0x206b,
	0x206c, 0x206d, 0x206e, 0x206f, 0x2070, 0x2071, 0x2072, 0x2073,
	0x2074, 0x2075, 0x2076, 0x2077, 0x2078, 0x2079, 0x207a, 0x207b,
	0x207c, 0x207d, 0x207e, 0x207f, 0x2080, 0x2081, 0x2082, 0x2083,
	0x2084, 0x2085, 0x2086, 0x2087, 0x2088, 0x2089, 0x208a, 0x208b,
	0x208c, 0x208d, 0x208e, 0x208f, 0x2090, 0x2091, 0x2092, 0x2093,
	0x2094, 0x2095, 0x2096, 0x2097, 0x2098, 0x2099, 0x209a, 0x209b,
	0x209c, 0x209d, 0x209e, 0x209f, 0x20a0, 0x20a1, 0x20a2, 0x20a3,
	0x20a4, 0x20a5, 0x20a6, 0x20a7, 0x20a8, 0x20a9, 0x20aa, 0x20ab,
	0x20ac, 0x20ad, 0x20ae, 0x20af, 0x20b0, 0x20b1, 0x20b2, 0x20b3,
	0x20b4, 0x20b5, 0x20b6, 0x20b7, 0x20b8, 0x20b9, 0x20ba, 0x20bb,
	0x20bc, 0x20bd, 0x20be, 0x20bf, 0x20c0, 0x20c1, 0x20c2, 0x20c3,
	0x20c4, 0x20c5, 0x20c6, 0x20c7, 0x20c8, 0x20c9, 0x20ca, 0x20cb,
	0x20cc, 0x20cd, 0x20ce, 0x20cf, 0x20d0, 0x20d1, 0x20d2, 0x20d3,
	0x20d4, 0x20d5, 0x20d6, 0x20d7, 0x20d8, 0x20d9, 0x20da, 0x20db,
	0x20dc, 0x20dd, 0x20de, 0x20df, 0x20e0, 0x20e1, 0x20e2, 0x20e3,
	0x20e4, 0x20e5, 0x20e6, 0x20e7, 0x20e8, 0x20e9, 0x20ea, 0x20eb,
	0x20ec, 0x20ed, 0x20ee, 0x20ef, 0x20f0, 0x20f1, 0x20f2, 0x20f3,
	0x20f4, 0x20f5, 0x20f6, 0x20f7, 0x20f8, 0x20f9, 0x20fa, 0x20fb,
	0x20fc, 0x20fd, 0x20fe, 0x20ff, 0x2100, 0x2101, 0x2102, 0x2103,
	0x2104, 0x2105, 0x2106, 0x2107, 0x2108, 0x2109, 0x210a, 0x210b,
	0x210c, 0x210d, 0x210e, 0x210f, 0x2110, 0x2111, 0x2112, 0x2113,
	0x2114, 0x2115, 0x2116, 0x2117, 0x2118, 0x2119, 0x211a, 0x211b,
	0x211c, 0x211d, 0x211e, 0x211f, 0x2120, 0x2121, 0x2122, 0x2123,
	0x2124, 0x2125, 0x2126, 0x2127, 0x2128, 0x2129, 0x212a, 0x212b,
	0x212c, 0x212d, 0x212e, 0x212f, 0x2130, 0x2131, 0x2132, 0x2133,
	0x2134, 0x2135, 0x2136, 0x2137, 0x2138, 0x2139, 0x213a, 0x213b,
	0x213c, 0x213d, 0x213e, 0x213f, 0x2140, 0x2141, 0x2142, 0x2143,
	0x2144, 0x2145, 0x2146, 0x2147, 0x2148, 0x2149, 0x214a, 0x214b,
	0x214c, 0x214d, 0x2132, 0x214f, 0x2150, 0x2151, 0x2152, 0x2153,
	0x2154, 0x2155, 0x2156, 0x2157, 0x2158, 0x2159, 0x215a, 0x215b,
	0x215c, 0x215d, 0x215e, 0x215f, 0x2160, 0x2161, 0x2162, 0x2163,
	0x2164, 0x2165, 0x2166, 0x2167, 0x2168, 0x2169, 0x216a, 0x216b,
	0x216c, 0x216d, 0x216e, 0x216f, 0x2160, 0x2161, 0x2162, 0x2163,
	0x2164, 0x2165, 0x2166, 0x2167, 0x2168, 0x2169, 0x216a, 0x216b,
	0x216c, 0x216d, 0x216e, 0x216f, 0x2180, 0x2181, 0x2182, 0x2183,
	0x2183, 0xffff, 0x034b, 0x24b6, 0x24b7, 0x24b8, 0x24b9, 0x24ba,
	0x24bb, 0x24bc, 0x24bd, 0x24be, 0x24bf, 0x24c0, 0x24c1, 0x24c2,
	0x24c3, 0x24c4, 0x24c5, 0x24c6, 0x24c7, 0x24c8, 0x24c9, 0x24ca,
	0x24cb, 0x24cc, 0x24cd, 0x24ce, 0x24cf, 0xffff, 0x0746, 0x2c00,
	0x2c01, 0x2c02, 0x2c03, 0x2c04, 0x2c05, 0x2c06, 0x2c07, 0x2c08,
	0x2c09, 0x2c0a, 0x2c0b, 0x2c0c, 0x2c0d, 0x2c0e, 0x2c0f, 0x2c10,
	0x2c11, 0x2c12, 0x2c13, 0x2c14, 0x2c15, 0x2c16, 0x2c17, 0x2c18,
	0x2c19, 0x2c1a, 0x2c1b, 0x2c1c, 0x2c1d, 0x2c1e, 0x2c1f, 0x2c20,
	0x2c21, 0x2c22, 0x2c23, 0x2c24, 0x2c25, 0x2c26, 0x2c27, 0x2c28,
	0x2c29, 0x2c2a, 0x2c2b, 0x2c2c, 0x2c2d, 0x2c2e, 0x2c5f, 0x2c60,
	0x2c60, 0x2c62, 0x2c63, 0x2c64, 0x2c65, 0x2c66, 0x2c67, 0x2c67,
	0x2c69, 0x2c69, 0x2c6b, 0x2c6b, 0x2c6d, 0x2c6e, 0x2c6f, 0x2c70,
	0x2c71, 0x2c72, 0x2c73, 0x2c74, 0x2c75, 0x2c75, 0x2c77, 0x2c78,
	0x2c79, 0x2c7a, 0x2c7b, 0x2c7c, 0x2c7d, 0x2c7e, 0x2c7f, 0x2c80,
	0x2c80, 0x2c82, 0x2c82, 0x2c84, 0x2c84, 0x2c86, 0x2c86, 0x2c88,
	0x2c88, 0x2c8a, 0x2c8a, 0x2c8c, 0x2c8c, 0x2c8e, 0x2c8e, 0x2c90,
	0x2c90, 0x2c92, 0x2c92, 0x2c94, 0x2c94, 0x2c96, 0x2c96, 0x2c98,
	0x2c98, 0x2c9a, 0x2c9a, 0x2c9c, 0x2c9c, 0x2c9e, 0x2c9e, 0x2ca0,
	0x2ca0, 0x2ca2, 0x2ca2, 0x2ca4, 0x2ca4, 0x2ca6, 0x2ca6, 0x2ca8,
	0x2ca8, 0x2caa, 0x2caa, 0x2cac, 0x2cac, 0x2cae, 0x2cae, 0x2cb0,
	0x2cb0, 0x2cb2, 0x2cb2, 0x2cb4, 0x2cb4, 0x2cb6, 0x2cb6, 0x2cb8,
	0x2cb8, 0x2cba, 0x2cba, 0x2cbc, 0x2cbc, 0x2cbe, 0x2cbe, 0x2cc0,
	0x2cc0, 0x2cc2, 0x2cc2, 0x2cc4, 0x2cc4, 0x2cc6, 0x2cc6, 0x2cc8,
	0x2cc8, 0x2cca, 0x2cca, 0x2ccc, 0x2ccc, 0x2cce, 0x2cce, 0x2cd0,
	0x2cd0, 0x2cd2, 0x2cd2, 0x2cd4, 0x2cd4, 0x2cd6, 0x2cd6, 0x2cd8,
	0x2cd8, 0x2cda, 0x2cda, 0x2cdc, 0x2cdc, 0x2cde, 0x2cde, 0x2ce0,
	0x2ce0, 0x2ce2, 0x2ce2, 0x2ce4, 0x2ce5, 0x2ce6, 0x2ce7, 0x2ce8,
	0x2ce9, 0x2cea, 0x2ceb, 0x2cec, 0x2ced, 0x2cee, 0x2cef, 0x2cf0,
	0x2cf1, 0x2cf2, 0x2cf3, 0x2cf4, 0x2cf5, 0x2cf6, 0x2cf7, 0x2cf8,
	0x2cf9, 0x2cfa, 0x2cfb, 0x2cfc, 0x2cfd, 0x2cfe, 0x2cff, 0x10a0,
	0x10a1, 0x10a2, 0x10a3, 0x10a4, 0x10a5, 0x10a6, 0x10a7, 0x10a8,
	0x10a9, 0x10aa, 0x10ab, 0x10ac, 0x10ad, 0x10ae, 0x10af, 0x10b0,
	0x10b1, 0x10b2, 0x10b3, 0x10b4, 0x10b5, 0x10b6, 0x10b7, 0x10b8,
	0x10b9, 0x10ba, 0x10bb, 0x10bc, 0x10bd, 0x10be, 0x10bf, 0x10c0,
	0x10c1, 0x10c2, 0x10c3, 0x10c4, 0x10c5, 0xffff, 0xd21b, 0xff21,
	0xff22, 0xff23, 0xff24, 0xff25, 0xff26, 0xff27, 0xff28, 0xff29,
	0xff2a, 0xff2b, 0xff2c, 0xff2d, 0xff2e, 0xff2f, 0xff30, 0xff31,
	0xff32, 0xff33, 0xff34, 0xff35, 0xff36, 0xff37, 0xff38, 0xff39,
	0xff3a, 0xff5b, 0xff5c, 0xff5d, 0xff5e, 0xff5f, 0xff60, 0xff61,
	0xff62, 0xff63, 0xff64, 0xff65, 0xff66, 0xff67, 0xff68, 0xff69,
	0xff6a, 0xff6b, 0xff6c, 0xff6d, 0xff6e, 0xff6f, 0xff70, 0xff71,
	0xff72, 0xff73, 0xff74, 0xff75, 0xff76, 0xff77, 0xff78, 0xff79,
	0xff7a, 0xff7b, 0xff7c, 0xff7d, 0xff7e, 0xff7f, 0xff80, 0xff81,
	0xff82, 0xff83, 0xff84, 0xff85, 0xff86, 0xff87, 0xff88, 0xff89,
	0xff8a, 0xff8b, 0xff8c, 0xff8d, 0xff8e, 0xff8f, 0xff90, 0xff91,
	0xff92, 0xff93, 0xff94, 0xff95, 0xff96, 0xff97, 0xff98, 0xff99,
	0xff9a, 0xff9b, 0xff9c, 0xff9d, 0xff9e, 0xff9f, 0xffa0, 0xffa1,
	0xffa2, 0xffa3, 0xffa4, 0xffa5, 0xffa6, 0xffa7, 0xffa8, 0xffa9,
	0xffaa, 0xffab, 0xffac, 0xffad, 0xffae, 0xffaf, 0xffb0, 0xffb1,
	0xffb2, 0xffb3, 0xffb4, 0xffb5, 0xffb6, 0xffb7, 0xffb8, 0xffb9,
	0xffba, 0xffbb, 0xffbc, 0xffbd, 0xffbe, 0xffbf, 0xffc0, 0xffc1,
	0xffc2, 0xffc3, 0xffc4, 0xffc5, 0xffc6, 0xffc7, 0xffc8, 0xffc9,
	0xffca, 0xffcb, 0xffcc, 0xffcd, 0xffce, 0xffcf, 0xffd0, 0xffd1,
	0xffd2, 0xffd3, 0xffd4, 0xffd5, 0xffd6, 0xffd7, 0xffd8, 0xffd9,
	0xffda, 0xffdb, 0xffdc, 0xffdd, 0xffde, 0xffdf, 0xffe0, 0xffe1,
	0xffe2, 0xffe3, 0xffe4, 0xffe5, 0xffe6, 0xffe7, 0xffe8, 0xffe9,
	0xffea, 0xffeb, 0xffec, 0xffed, 0xffee, 0xffef, 0xfff0, 0xfff1,
	0xfff2, 0xfff3, 0xfff4, 0xfff5, 0xfff6, 0xfff7, 0xfff8, 0xfff9,
	0xfffa, 0xfffb, 0xfffc, 0xfffd, 0xfffe, 0xffff,
};

/*
 * Allow full-width illegal characters :
 * "MS windows 7" supports full-width-invalid-name-characters.
 * So we should check half-width-invalid-name-characters(ASCII) only
 * for compatibility.
 *
 * " * / : < > ? \ |
 */
static unsigned short bad_uni_chars[] = {
	0x0022,         0x002A, 0x002F, 0x003A,
	0x003C, 0x003E, 0x003F, 0x005C, 0x007C,
	0
};

static int exfat_convert_char_to_ucs2(struct nls_table *nls,
		const unsigned char *ch, int ch_len, unsigned short *ucs2,
		int *lossy)
{
	int len;

	*ucs2 = 0x0;

	if (ch[0] < 0x80) {
		*ucs2 = ch[0];
		return 1;
	}

	len = nls->char2uni(ch, ch_len, ucs2);
	if (len < 0) {
		/* conversion failed */
		if (lossy != NULL)
			*lossy |= NLS_NAME_LOSSY;
		*ucs2 = '_';
		return 1;
	}
	return len;
}

static int exfat_convert_ucs2_to_char(struct nls_table *nls,
		unsigned short ucs2, unsigned char *ch, int *lossy)
{
	int len;

	ch[0] = 0x0;

	if (ucs2 < 0x0080) {
		ch[0] = ucs2;
		return 1;
	}

	len = nls->uni2char(ucs2, ch, MAX_CHARSET_SIZE);
	if (len < 0) {
		/* conversion failed */
		if (lossy != NULL)
			*lossy |= NLS_NAME_LOSSY;
		ch[0] = '_';
		return 1;
	}
	return len;
}

unsigned short exfat_toupper(struct super_block *sb, unsigned short a)
{
	struct exfat_sb_info *sbi = EXFAT_SB(sb);

	return sbi->vol_utbl[a] ? sbi->vol_utbl[a] : a;
}

static unsigned short *exfat_wstrchr(unsigned short *str, unsigned short wchar)
{
	while (*str) {
		if (*(str++) == wchar)
			return str;
	}
	return NULL;
}

int exfat_uniname_ncmp(struct super_block *sb, unsigned short *a,
		unsigned short *b, unsigned int len)
{
	int i;

	for (i = 0; i < len; i++, a++, b++)
		if (exfat_toupper(sb, *a) != exfat_toupper(sb, *b))
			return 1;
	return 0;
}

static int exfat_utf16_to_utf8(struct super_block *sb,
		struct exfat_uni_name *p_uniname, unsigned char *p_cstring,
		int buflen)
{
	int len;
	const unsigned short *uniname = p_uniname->name;

	/* always len >= 0 */
	len = utf16s_to_utf8s(uniname, MAX_NAME_LENGTH, UTF16_HOST_ENDIAN,
		p_cstring, buflen);
	p_cstring[len] = '\0';
	return len;
}

static int exfat_utf8_to_utf16(struct super_block *sb,
		const unsigned char *p_cstring, const int len,
		struct exfat_uni_name *p_uniname, int *p_lossy)
{
	int i, unilen, lossy = NLS_NAME_NO_LOSSY;
	__le16 upname[MAX_NAME_LENGTH + 1];
	unsigned short *uniname = p_uniname->name;

	WARN_ON(!len);

	unilen = utf8s_to_utf16s(p_cstring, len, UTF16_HOST_ENDIAN,
			(wchar_t *)uniname, MAX_NAME_LENGTH + 2);
	if (unilen < 0) {
		exfat_err(sb, "failed to %s (err : %d) nls len : %d",
			  __func__, unilen, len);
		return unilen;
	}

	if (unilen > MAX_NAME_LENGTH) {
		exfat_err(sb, "failed to %s (estr:ENAMETOOLONG) nls len : %d, unilen : %d > %d",
			  __func__, len, unilen, MAX_NAME_LENGTH);
		return -ENAMETOOLONG;
	}

	for (i = 0; i < unilen; i++) {
		if (*uniname < 0x0020 ||
		    exfat_wstrchr(bad_uni_chars, *uniname))
			lossy |= NLS_NAME_LOSSY;

		upname[i] = cpu_to_le16(exfat_toupper(sb, *uniname));
		uniname++;
	}

	*uniname = '\0';
	p_uniname->name_len = unilen;
	p_uniname->name_hash = exfat_calc_chksum16(upname, unilen << 1, 0,
			CS_DEFAULT);

	if (p_lossy)
		*p_lossy = lossy;
	return unilen;
}

#define SURROGATE_MASK	0xfffff800
#define SURROGATE_PAIR	0x0000d800
#define SURROGATE_LOW	0x00000400

static int __exfat_utf16_to_nls(struct super_block *sb,
		struct exfat_uni_name *p_uniname, unsigned char *p_cstring,
		int buflen)
{
	int i, j, len, out_len = 0;
	unsigned char buf[MAX_CHARSET_SIZE];
	const unsigned short *uniname = p_uniname->name;
	struct nls_table *nls = EXFAT_SB(sb)->nls_io;

	i = 0;
	while (i < MAX_NAME_LENGTH && out_len < (buflen - 1)) {
		if (*uniname == '\0')
			break;
		if ((*uniname & SURROGATE_MASK) != SURROGATE_PAIR) {
			len = exfat_convert_ucs2_to_char(nls, *uniname, buf,
				NULL);
		} else {
			/* Process UTF-16 surrogate pair as one character */
			if (!(*uniname & SURROGATE_LOW) &&
			    i+1 < MAX_NAME_LENGTH &&
			    (*(uniname+1) & SURROGATE_MASK) == SURROGATE_PAIR &&
			    (*(uniname+1) & SURROGATE_LOW)) {
				uniname++;
				i++;
			}

			/*
			 * UTF-16 surrogate pair encodes code points above
			 * U+FFFF. Code points above U+FFFF are not supported
			 * by kernel NLS framework therefore use replacement
			 * character
			 */
			len = 1;
			buf[0] = '_';
		}

		if (out_len + len >= buflen)
			len = buflen - 1 - out_len;
		out_len += len;

		if (len > 1) {
			for (j = 0; j < len; j++)
				*p_cstring++ = buf[j];
		} else { /* len == 1 */
			*p_cstring++ = *buf;
		}

		uniname++;
		i++;
	}

	*p_cstring = '\0';
	return out_len;
}

static int exfat_nls_to_ucs2(struct super_block *sb,
		const unsigned char *p_cstring, const int len,
		struct exfat_uni_name *p_uniname, int *p_lossy)
{
	int i = 0, unilen = 0, lossy = NLS_NAME_NO_LOSSY;
	__le16 upname[MAX_NAME_LENGTH + 1];
	unsigned short *uniname = p_uniname->name;
	struct nls_table *nls = EXFAT_SB(sb)->nls_io;

	WARN_ON(!len);

	while (unilen < MAX_NAME_LENGTH && i < len) {
		i += exfat_convert_char_to_ucs2(nls, p_cstring + i, len - i,
				uniname, &lossy);

		if (*uniname < 0x0020 ||
		    exfat_wstrchr(bad_uni_chars, *uniname))
			lossy |= NLS_NAME_LOSSY;

		upname[unilen] = cpu_to_le16(exfat_toupper(sb, *uniname));
		uniname++;
		unilen++;
	}

	if (p_cstring[i] != '\0')
		lossy |= NLS_NAME_OVERLEN;

	*uniname = '\0';
	p_uniname->name_len = unilen;
	p_uniname->name_hash = exfat_calc_chksum16(upname, unilen << 1, 0,
			CS_DEFAULT);

	if (p_lossy)
		*p_lossy = lossy;
	return unilen;
}

int exfat_utf16_to_nls(struct super_block *sb, struct exfat_uni_name *uniname,
		unsigned char *p_cstring, int buflen)
{
	if (EXFAT_SB(sb)->options.utf8)
		return exfat_utf16_to_utf8(sb, uniname, p_cstring,
				buflen);
	return __exfat_utf16_to_nls(sb, uniname, p_cstring, buflen);
}

int exfat_nls_to_utf16(struct super_block *sb, const unsigned char *p_cstring,
		const int len, struct exfat_uni_name *uniname, int *p_lossy)
{
	if (EXFAT_SB(sb)->options.utf8)
		return exfat_utf8_to_utf16(sb, p_cstring, len,
				uniname, p_lossy);
	return exfat_nls_to_ucs2(sb, p_cstring, len, uniname, p_lossy);
}

static int exfat_load_upcase_table(struct super_block *sb,
		sector_t sector, unsigned long long num_sectors,
		unsigned int utbl_checksum)
{
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	unsigned int sect_size = sb->s_blocksize;
	unsigned int i, index = 0;
	u32 chksum = 0;
	int ret;
	unsigned char skip = false;
	unsigned short *upcase_table;

	upcase_table = kvcalloc(UTBL_COUNT, sizeof(unsigned short), GFP_KERNEL);
	if (!upcase_table)
		return -ENOMEM;

	sbi->vol_utbl = upcase_table;
	num_sectors += sector;

	while (sector < num_sectors) {
		struct buffer_head *bh;

		bh = sb_bread(sb, sector);
		if (!bh) {
			exfat_err(sb, "failed to read sector(0x%llx)\n",
				  (unsigned long long)sector);
			ret = -EIO;
			goto free_table;
		}
		sector++;
		for (i = 0; i < sect_size && index <= 0xFFFF; i += 2) {
			unsigned short uni = get_unaligned_le16(bh->b_data + i);

			if (skip) {
				index += uni;
				skip = false;
			} else if (uni == index) {
				index++;
			} else if (uni == 0xFFFF) {
				skip = true;
			} else { /* uni != index , uni != 0xFFFF */
				upcase_table[index] = uni;
				index++;
			}
		}
		chksum = exfat_calc_chksum32(bh->b_data, i, chksum, CS_DEFAULT);
		brelse(bh);
	}

	if (index >= 0xFFFF && utbl_checksum == chksum)
		return 0;

	exfat_err(sb, "failed to load upcase table (idx : 0x%08x, chksum : 0x%08x, utbl_chksum : 0x%08x)",
		  index, chksum, utbl_checksum);
	ret = -EINVAL;
free_table:
	exfat_free_upcase_table(sbi);
	return ret;
}

static int exfat_load_default_upcase_table(struct super_block *sb)
{
	int i, ret = -EIO;
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	unsigned char skip = false;
	unsigned short uni = 0, *upcase_table;
	unsigned int index = 0;

	upcase_table = kvcalloc(UTBL_COUNT, sizeof(unsigned short), GFP_KERNEL);
	if (!upcase_table)
		return -ENOMEM;

	sbi->vol_utbl = upcase_table;

	for (i = 0; index <= 0xFFFF && i < EXFAT_NUM_UPCASE; i++) {
		uni = uni_def_upcase[i];
		if (skip) {
			index += uni;
			skip = false;
		} else if (uni == index) {
			index++;
		} else if (uni == 0xFFFF) {
			skip = true;
		} else {
			upcase_table[index] = uni;
			index++;
		}
	}

	if (index >= 0xFFFF)
		return 0;

	/* FATAL error: default upcase table has error */
	exfat_free_upcase_table(sbi);
	return ret;
}

int exfat_create_upcase_table(struct super_block *sb)
{
	int i, ret;
	unsigned int tbl_clu, type;
	sector_t sector;
	unsigned long long tbl_size, num_sectors;
	unsigned char blksize_bits = sb->s_blocksize_bits;
	struct exfat_chain clu;
	struct exfat_dentry *ep;
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	struct buffer_head *bh;

	clu.dir = sbi->root_dir;
	clu.flags = ALLOC_FAT_CHAIN;

	while (clu.dir != EXFAT_EOF_CLUSTER) {
		for (i = 0; i < sbi->dentries_per_clu; i++) {
			ep = exfat_get_dentry(sb, &clu, i, &bh, NULL);
			if (!ep)
				return -EIO;

			type = exfat_get_entry_type(ep);
			if (type == TYPE_UNUSED) {
				brelse(bh);
				break;
			}

			if (type != TYPE_UPCASE) {
				brelse(bh);
				continue;
			}

			tbl_clu  = le32_to_cpu(ep->dentry.upcase.start_clu);
			tbl_size = le64_to_cpu(ep->dentry.upcase.size);

			sector = exfat_cluster_to_sector(sbi, tbl_clu);
			num_sectors = ((tbl_size - 1) >> blksize_bits) + 1;
			ret = exfat_load_upcase_table(sb, sector, num_sectors,
				le32_to_cpu(ep->dentry.upcase.checksum));

			brelse(bh);
			if (ret && ret != -EIO)
				goto load_default;

			/* load successfully */
			return ret;
		}

		if (exfat_get_next_cluster(sb, &(clu.dir)))
			return -EIO;
	}

load_default:
	/* load default upcase table */
	return exfat_load_default_upcase_table(sb);
}

void exfat_free_upcase_table(struct exfat_sb_info *sbi)
{
	kvfree(sbi->vol_utbl);
	sbi->vol_utbl = NULL;
}
