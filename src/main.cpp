#include "df_font.h"
#include "df_window.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

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

struct Fnt {

//    struct FontDirEntry {
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
        _Glyph glyph_table[1];           // Num entries is last_char - first_char + 2.
//         char device_name[];
//         char face_name[];
//    };
};
#pragma pack(pop)


struct BinaryFileReader {
    char *data;
    int size;
    int offset;

    bool ReadEntireFile(char const *path) {
        FILE *f = fopen(path, "rb");
        if (!f) return false;
        if (fseek(f, 0, SEEK_END) != 0) goto err;
        int size = ftell(f);
        if (fseek(f, 0, SEEK_SET) != 0) goto err;
        data = new char[size];
        if (fread(data, 1, size, f) != size) goto err;
        fclose(f);

        offset = 0;
        return true;

    err:
        fclose(f);
        return false;
    }

    char *Read(int size) {
        char *rv = data + offset;
        offset += size;
        return rv;
    }
};

// FNT resource format explanation https://jeffpar.github.io/kbarchive/kb/065/Q65123/
void ReadFntResourceItem(BinaryFileReader &f, int block_size) {
    //printf("Resource table item offset 0x%x\n", f.offset);
    ResourceTableItem *rt_item = (ResourceTableItem *)f.Read(sizeof(ResourceTableItem));
    assert(rt_item->resource_id & 0x8000);
    //printf("Item id %i\n", rt_item->resource_id);

    int next_item_offset = f.offset;

    int fnt_data_offset = block_size * rt_item->data_offset;
    //printf("FNT data offset is 0x%x\n", fnt_data_offset);

    f.offset = fnt_data_offset;
    Fnt *fnt = (Fnt *)f.Read(sizeof(Fnt));

    ReleaseAssert(fnt->version == 0x200, "Version 0x%x found. Only version 0x200 supported.",
        fnt->version);
    //printf("Name: %s\n", (char *)fnt + fnt->name_offset);
    //printf("%s\n", fnt->copyright);
    //printf("Bitmap offset is 0x%x\n", fnt->bitmap_offset + fnt_data_offset);

    int num_glyphs = fnt->last_char - fnt->first_char + 1;
    int bytes_per_row = ((fnt->max_width + 8) / 8) * num_glyphs - 1;
    int bmp_num_bytes = bytes_per_row * fnt->pix_height;
    //printf("Bitmap num bytes is 0x%x\n", bmp_num_bytes);

    f.offset = fnt->bitmap_offset + fnt_data_offset;
    u8 *bmp = (u8 *)f.Read(bmp_num_bytes);

    int num_chars = fnt->last_char - fnt->first_char + 1;
    int num_columns = (fnt->max_width + 7) / 8;
    int num_bytes_per_glyph = num_columns * fnt->pix_height;
    for (int i = 0; i < num_chars; i++) {
        for (int column = 0; column < num_columns; column++) {
            int x0 = (i % 16) * fnt->max_width + column * 8;
            int y0 = (i / 16) * fnt->pix_height;
            int bmp_offset = i * num_bytes_per_glyph + column * fnt->pix_height;
            int column_pixel_width = 8;// fnt->pix_width - column * 8;
            u8 *glyph = bmp + bmp_offset;
            for (int y = 0; y < fnt->pix_height; y++) {
                for (int x = 0; x < column_pixel_width; x++) {
                    int the_byte = glyph[y];
                    int bit_mask = 0x80 >> (x % 8);
                    if (the_byte & bit_mask) {
                        PutPix(g_window->bmp, x0 + x, y0 + y, g_colourWhite);
                    }
                }
            }
        }
    }

    f.offset = next_item_offset;
}


int main() {
    CreateWin(800, 600, WT_WINDOWED, ".FON Converter");
    BitmapClear(g_window->bmp, g_colourBlack);

    BinaryFileReader f;
    char const *path = "h:/arcs/fonts/myfonts/trowel_variable2.fon";
    //char const *path = "h:/arcs/fonts/coure.fon";
    if (!f.ReadEntireFile(path)) {
        return -1;
    }

    OldExeHeader *old_hdr = (OldExeHeader *)f.Read(sizeof(OldExeHeader));
    //printf("Read old header of size %i\n", sizeof(OldExeHeader));
    
    int new_hdr_offset = old_hdr->num_paragraphs_in_header * 16 + sizeof(OldExeHeader);
    //printf("New header spans 0x%x to 0x%x.\n", new_hdr_offset, new_hdr_offset + sizeof(NewExeHeader) - 1);

    f.offset = new_hdr_offset;
    NewExeHeader *new_hdr = (NewExeHeader *)f.Read(sizeof(NewExeHeader));
    //printf("New header ID is %c%c\n", new_hdr->id[0], new_hdr->id[1]);
    
    int seg_table_offset = new_hdr_offset + new_hdr->seg_table_offset;
    //printf("Seg table spans 0x%x to 0x%x\n", seg_table_offset,
//        seg_table_offset + new_hdr->seg_table_num_items * 8 - 1);

    int res_table_offset = new_hdr_offset + new_hdr->res_table_offset;
    int res_table_num_blocks = new_hdr->num_resource_entries / sizeof(ResourceTableBlock);
    //printf("Resource table num blocks is %i\n", res_table_num_blocks);
    //printf("Resource table spans 0x%x to 0x%x\n", res_table_offset, new_hdr->num_resource_entries * sizeof(ResourceTableBlock));

    //printf("Resident name table offset is 0x%x\n", new_hdr->resi_name_table_offset + new_hdr_offset);
    //printf("Module ref table offset is 0x%x\n", new_hdr->module_ref_table_offset + new_hdr_offset);
    //printf("Imported name table offset is 0x%x\n", new_hdr->imported_names_table_offset + new_hdr_offset);
    //printf("Non resi name table offset 0x%x. Num items %i\n", new_hdr->non_resi_name_table_offset,
//        new_hdr->non_resi_name_table_offset + new_hdr->non_resi_name_table_num_bytes);
    
    f.offset = res_table_offset;
    char *resource_table = f.data + f.offset;
    u16 *alignment_shift_amount = (u16*)f.Read(sizeof(u16));
    int block_size = 1 << *alignment_shift_amount;

    while (1) {
        //printf("\nResource block offset 0x%x. ", f.offset);
        ResourceTableBlock *rtblock = (ResourceTableBlock *)f.Read(sizeof(ResourceTableBlock));
        //printf("type id 0x%x. Num of this type %i\n", rtblock->type_id, rtblock->num_of_this_type);

        if (rtblock->type_id == 0) {
            break;
        }
        else if (rtblock->type_id == 0x8008) {
            int next_block_offset = f.offset + sizeof(ResourceTableItem)* rtblock->num_of_this_type;

            for (int i = 0; i < 1; i++) {
                BitmapClear(g_window->bmp, g_colourBlack);
//            for (int i = 0; i < rtblock->num_of_this_type; i++) {
                ReadFntResourceItem(f, block_size);
            }

            break;
        }
        else {
            int next_block_offset = f.offset + sizeof(ResourceTableItem) * rtblock->num_of_this_type;
            
            ResourceTableItem *rt_item = (ResourceTableItem *)f.Read(sizeof(ResourceTableItem));
            if (rtblock->type_id == 0x8007) {
                assert(!(rt_item->resource_id & 0x8000));
                //printf("Font dir name %s\n", resource_table + rt_item->resource_id);
            }
    
            f.offset = next_block_offset;
        }
    }

    while (!g_window->windowClosed && !g_input.keyDowns[KEY_ESC])
    {
        InputPoll();
         
        UpdateWin();
        WaitVsync();
    }

    return 0;
}