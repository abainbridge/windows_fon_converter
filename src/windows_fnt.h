#pragma once

#include <stdint.h>


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;


// First 0x40 bytes of file.
struct OldExeHeader {
    u8 id[2];                       // "MZ"
    u16 num_bytes_in_last_page;     // 0x10d
    u16 num_pages;                  // 1
    u16 num_reloc_entries;          // 0
    u16 num_paragraphs_in_header;   // 4 - Paragraph is 16 bytes, so 0x40 bytes total. Therefore NewExeHeader is at 0x40 + 0x40.
    u16 min_para;                   // 0
    u16 max_para;                   // 0xffff
    u16 initial_ss;                 // 0

    u16 initial_sp;                 // 0xb8
    u16 checksum;                   // 0
    u32 cs_ip;                      // 0
    u16 reloc_table_offset;         // 0x40 - Points to DOS stub
    u16 overlay_num;
    u8 padding[32];
    u32 new_exe_header_offset;
};


struct DosStub {
    u8 stuff[14];// = {
    //    0x0e, 0x1f, 0xba, 0x0e, 0x00, 0xb4, 0x09,
    //    0xcd, 0x21, 0xb8, 0x01, 0x4c, 0xcd, 0x21 
    //};
    u8 more_stuff[44];// = "This Program cannot be run in DOS mode\r\n$";
};


// Starts at 0x80
struct NewExeHeader {
    u8 id[2];                       // "NE"
    u16 linker_ver;                 // 0x105
    u16 entry_table_offset;         // 0x88 - Relative to start of this header.
    u16 entry_table_num_bytes;      // 0
    u32 crc;                        // 0
    u16 flags;                      // 0x8300 - "Library mode".
    u16 automatic_data_seg_num;     // 0

    u8 padding[10];                 // 0

    u16 seg_table_num_items;
    u16 module_ref_table_num_items;
    u16 non_resi_name_table_num_bytes;  // 0x2f
    u16 seg_table_offset;           // 0x40 - Relative to start of this header.
    u16 res_table_offset;           // 0x40 - Relative to start of this header.
    u16 resi_name_table_offset;     // 0x7a - Relative to start of this header.
    u16 module_ref_table_offset;    // 0x88 - Relative to start of this header.
    u16 imported_names_table_offset; // 0x88
    u32 non_resi_name_table_offset; // 0x10a - Relative to the beginning of the file.

    u16 num_movables_in_entry_table; // 0
    u16 logical_section_alignment_shift_count; // 4
    u16 num_resource_entries;       // 0
    u8 exe_type;                    // 4
    u8 expected_windows_version[2]; // 0
};


struct ResourceTableItem {
    u16 data_offset;                // 0x14 - Multiply by 1<<4 = 0x14 * 16 = 0x140. From start of file.
    u16 num_bytes;                  // 9
    u16 flags;                      // 0x50 - Preloaded and movable.
    u16 resource_id;                // 0x8001 - Integer id if the high-order it is set, otherwise it is the offset to the resource string, the offset is relative to the beginning of the resource table.
    u32 padding2;

    // Second one is the font.
    // data_offset = 0x1d * 16 = 0x1d0
    // num_bytes = 0xf8
    // flags = 0x1030
    // resource_name = 0x8050
};


// First one at offset 0xc2
struct ResourceTableBlock {
    u16 type_id;                    // 0x8007 - Int if top bit set. 7=Font dir. 8=Font.
    u16 num_of_this_type;           // 1
    u32 padding;
    //    ResourceTableItem items[1];
};


struct ResourceTableString {
    u8 num_bytes;                   // Zero means end of string table.
    u8 text[1];
};


// Starts at 0xc0
struct ResorceTable {
    u16 alignment_shift_amount;     // 4
    //    ResourceTableBlock blocks[1];
};


// Starts at 0x140
#pragma pack(push, 1)
struct _Glyph {
    u16 pix_width;
    u16 bitmap_offset;
};

struct FntHeader {
    u16 version;                    // 0x200
    u16 size[2];                    // 0xf76
    char copyright[60];
    u16 type;                       // 0 - Bit 0 is zero, is therefore raster not vector.
    u16 point_size;                 // 0xa
    u16 vert_dpi;                   // 0x60
    u16 hori_dpi;                   // 0x60
    u16 ascent;                     // 0xb - Num pixels from top of glyph to baseline. Useful for aligning baseline of fonts of different heights.
    u16 internal_leading;           // 0
    u16 external_leading;           // 0
    u8 italic;                      // 0
    u8 underline;                   // 0
    u8 strikeout;                   // 0
    u16 weight;                     // 0x190 - 100=light, 200=regular, 300=semi-bold
    u8 char_set;                    // 0
    u16 pix_width;                  // 7 - Pixel width of glyph. 0=variable.
    u16 pix_height;                 // 13
    u8 pitch_and_family;            // 0x30 - b0=0=fixed width, 
    u16 avg_width;                  // 7
    u16 max_width;                  // 7
    u8 first_char;                  // 0x20
    u8 last_char;                   // 0xff
    u8 default_char;                // 0xdf
    u8 break_char;                  // 0
    u16 width_bytes;                // 0x627 - Number of bytes in each row of the bitmap. Must be even.
    u32 device;                     // 0
    u32 name_offset;                // 0xf6b - Offset from start of this struct for font name.
    u32 pad0;                       // 0
    u32 bitmap_offset;              // 0x3fe
    u8 pad1;
    //        u32 flags;                      // 1=fixed, 2=proportional, 16=1 colour
    //        u16 abc_spaces[3];
    //        u32 colour_offset;
    //        u8 pad2[2];
};
#pragma pack(pop)
