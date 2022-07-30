#include "windows_fnt.h"

#include "df_font.h"
#include "df_window.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


DfColour g_colourRed = Colour(244, 0, 0);


struct FullFnt {
    FntHeader hdr;
    _Glyph *glyph_table;           // Num entries is hdr.last_char - hdr.first_char + 2.
    char name[128];
    DfBitmap *bmp;
};


struct MemBuf {
    int max_len;
    u8 *data;
    int len;
    int hi_nibble_next;
    int output_byte;

    MemBuf() {
        max_len = 1000 * 1000;
        data = new u8[max_len];
        memset(data, 0, max_len);
        len = 0;
        hi_nibble_next = 0;
        output_byte = 0;
    }

    void PushByte(int val) {
        ReleaseAssert(hi_nibble_next == 0, "Attempt to write a byte when a nibble was buffered");
        ReleaseAssert(len < max_len, "MemBuf full");
        data[len] = val;
        len++;
    }

    void PushNibble(int val) {
        if (hi_nibble_next) {
            output_byte |= val << 4;
            hi_nibble_next = 0;
            PushByte(output_byte);
            output_byte = 0;
        }
        else {
            hi_nibble_next = 1;
            output_byte |= val;
        }
    }

    void WriteToBinFile(FILE *f) {
        if (hi_nibble_next) {
            PushNibble(0);
        }

        ReleaseAssert(hi_nibble_next == 0, "Attempt to write a byte when a nibble was buffered 2");
        int bytes_written = fwrite(data, 1, len, f);
        ReleaseAssert(bytes_written == len, "Couldn't write to output file");
    }

    void WriteToCFile(FILE *f, const char *font_name, int font_width, int font_height) {
        if (hi_nibble_next) {
            PushNibble(0);
        }

        ReleaseAssert(hi_nibble_next == 0, "Attempt to write a byte when a nibble was buffered 2");
        
        fprintf(f, "NO_UNUSED_WARNING static unsigned %s_%ix%i[] = {\n    ", font_name, font_width, font_height);
        u32 *data32 = (u32*)data;
        int num_words = (len + 3) / 4;
        for (int i = 0; i < num_words; i++) {
            fprintf(f, "0x%08x, ", data32[i]);
            if (i % 8 == 7) {
                fprintf(f, "\n    ");
            }
        }
        fprintf(f, "\n};\n\n");
    }
};


// FNT resource format explanation https://jeffpar.github.io/kbarchive/kb/065/Q65123/
FullFnt *ReadFntResourceItem(FILE *f, int block_size) {  
    ResourceTableItem rt_item; fread(&rt_item, 1, sizeof(ResourceTableItem), f);
    ReleaseAssert(rt_item.resource_id & 0x8000, "Bad resource id");
    
    int next_item_offset = ftell(f);
    int fnt_data_offset = block_size * rt_item.data_offset;
    fseek(f, fnt_data_offset, SEEK_SET);
    
    FullFnt *full_fnt = new FullFnt;
    FntHeader *fnt = &full_fnt->hdr;
    fread(fnt, 1, sizeof(FntHeader), f);

    ReleaseAssert(fnt->version == 0x200, "Version 0x%x found. Only version 0x200 supported.",
        fnt->version);
    
    int num_glyphs = fnt->last_char - fnt->first_char + 1;
    int bytes_per_row = ((fnt->max_width + 8) / 8) * num_glyphs;
    int bmp_num_bytes = bytes_per_row * fnt->pix_height;

    // Read the glyph table.
    const int glyph_table_size = fnt->last_char - fnt->first_char + 2;
    full_fnt->glyph_table = new _Glyph[fnt->last_char - fnt->first_char + 2];
    fread(full_fnt->glyph_table, 1, glyph_table_size * sizeof(_Glyph), f);

    // Read the name.
    fseek(f, fnt_data_offset + fnt->name_offset, SEEK_SET);
    fread(full_fnt->name, 1, sizeof(full_fnt->name), f);

    // Read the bitmap.
    fseek(f, fnt->bitmap_offset + fnt_data_offset, SEEK_SET);
    u8 *bmp = new u8[bmp_num_bytes];
    fread(bmp, 1, bmp_num_bytes, f);

    // Create the DF bitmap.
    full_fnt->bmp = BitmapCreate(16 * fnt->max_width, 14 * fnt->pix_height);
    BitmapClear(full_fnt->bmp, g_colourBlack);
    int num_chars = fnt->last_char - fnt->first_char + 1;
    for (int i = 0; i < num_chars; i++) {
        int num_columns = (full_fnt->glyph_table[i].pix_width + 7) / 8;
        for (int column = 0; column < num_columns; column++) {
            int x0 = (i % 16) * fnt->max_width + column * 8;
            int y0 = (i / 16) * fnt->pix_height;
            int bmp_offset = full_fnt->glyph_table[i].bitmap_offset + fnt->pix_height * column;
            u8 *glyph = bmp + bmp_offset - 1018;
            for (int y = 0; y < fnt->pix_height; y++) {
                for (int x = 0; x < 8; x++) {
                    int the_byte = glyph[y];
                    int bit_mask = 0x80 >> (x % 8);
                    if (the_byte & bit_mask) {
                        PutPix(full_fnt->bmp, x0 + x, y0 + y, g_colourWhite);
                    }
                }
            }
        }

        int glyph_width = full_fnt->glyph_table[i].pix_width;
        for (int j = glyph_width; j < full_fnt->hdr.max_width; j++) {
            int x = (i % 16) * fnt->max_width + j;
            int y = (i / 16) * fnt->pix_height;
            VLine(full_fnt->bmp, x, y, fnt->pix_height, g_colourRed);
        }
    }

    fseek(f, next_item_offset, SEEK_SET);

    return full_fnt;
}


void RemoveRed(DfBitmap *bmp) {
    for (int y = 0; y < bmp->height; y++) {
        for (int x = 0; x < bmp->width; x++) {
            DfColour c = GetPix(bmp, x, y);
            if (c.c == g_colourRed.c) {
                PutPix(bmp, x, y, g_colourBlack);
            }
        }
    }
}

void DoUpPrediction(DfBitmap *bmp) {
    for (int y = bmp->height - 1; y > 0; y--) {
        for (int x = 0; x < bmp->width; x++) {
            DfColour prevColour = GetPix(bmp, x, y - 1);
            DfColour colour = GetPix(bmp, x, y);
            if (prevColour.c == colour.c) {
                PutPix(bmp, x, y, g_colourBlack);
            }
            else {
                PutPix(bmp, x, y, g_colourWhite);
            }

            prevColour = colour;
        }
    }
}


void WriteDfbfToMemBuf(MemBuf *buf, FullFnt *fnt) {
    buf->PushByte(fnt->hdr.max_width);
    buf->PushByte(fnt->hdr.pix_height);

    int flags = 0;
    if (fnt->hdr.pix_width == 0) {
        flags |= 1;
    }
    buf->PushByte(flags);

    // If fnt is variable width, write the glyph widths table.
    if (fnt->hdr.pix_width == 0) {
        for (int c = 0; c < 224; c++) {
            buf->PushByte(fnt->glyph_table[c].pix_width);
        }
    }

    RemoveRed(fnt->bmp);
    DoUpPrediction(fnt->bmp);

    // Write the RLE encoded bitmap
    int run_len = 0;
    DfColour prev_pix = g_colourBlack;
    for (int y = 0; y < fnt->bmp->height; y++) {
        for (int x = 0; x < fnt->bmp->width; x++) {
            DfColour pix = GetPix(fnt->bmp, x, y);
            if (pix.c != prev_pix.c) {
                while (run_len > 0) {
                    if (run_len >= 16 || run_len == 0) {
                        int run_len_to_write = run_len;
                        if (run_len_to_write > 255) {
                            run_len_to_write = 255;
                        }
                        buf->PushNibble(0); // 0 is the escape char, meaning an 8-bit length follows.
                        buf->PushNibble(run_len_to_write & 0xf);
                        buf->PushNibble(run_len_to_write >> 4);
                        if (run_len > 255) {
							// If run_len is > 255, then there is no way to encode it directly.
							// Instead we will have to ouput multiple runs that add up to the
							// required total. But adjacent runs must be of opposite "values".
							// The solution to that is to create zero length runs between them.
							// We can only encode zero length runs using an escape too. So,
							// output a zero lengthed run now.
                            buf->PushNibble(0);
                            buf->PushNibble(0);
                            buf->PushNibble(0);
                        }
                        run_len -= run_len_to_write;
                    }
                    else if (run_len < 16) {
                        buf->PushNibble(run_len);
                        run_len = 0;
                    }
                }
            }

            run_len++;
            prev_pix = pix;
        }
    }
}


char *GetNameFromPath(const char *path) {
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    ReleaseAssert(dot && slash && slash < dot, "Couldn't extract font name from input path of '%s'", path);
    size_t name_len = dot - slash - 1;
    char *rv = new char[name_len + 1];
    memcpy(rv, slash + 1, name_len);
    rv[name_len] = '\0';
    return rv;
}


int main(int argc, char *argv[]) {
    char const *path = NULL;
    //path = "c:/coding/deadfrog-lib/src/fonts/df_prop.fon";

    if (!path) {
        if (argc != 2) {
            printf("Usage: %s <your.fon>\n", argv[0]);
            return 0;
        }
        else {
            path = argv[1];
        }
    }

    g_window = CreateWin(1400, 800, WT_WINDOWED_FIXED, ".FON Converter");
    BitmapClear(g_window->bmp, g_colourBlack);

    FILE *f = fopen(path, "rb");
    ReleaseAssert(f, "Couldn't open file '%s'", path);

    OldExeHeader old_hdr; fread(&old_hdr, 1, sizeof(OldExeHeader), f);
    //printf("Read old header of size %i\n", sizeof(OldExeHeader));
    
    int new_hdr_offset = old_hdr.num_paragraphs_in_header * 16 + sizeof(OldExeHeader);
    //printf("New header spans 0x%x to 0x%x.\n", new_hdr_offset, new_hdr_offset + sizeof(NewExeHeader) - 1);

    fseek(f, new_hdr_offset, SEEK_SET);
    NewExeHeader new_hdr; fread(&new_hdr, 1, sizeof(NewExeHeader), f);
    //printf("New header ID is %c%c\n", new_hdr.id[0], new_hdr.id[1]);
    
    int seg_table_offset = new_hdr_offset + new_hdr.seg_table_offset;
    //printf("Seg table spans 0x%x to 0x%x\n", seg_table_offset,
//        seg_table_offset + new_hdr.seg_table_num_items * 8 - 1);

    int res_table_offset = new_hdr_offset + new_hdr.res_table_offset;
    int res_table_num_blocks = new_hdr.num_resource_entries / sizeof(ResourceTableBlock);
    //printf("Resource table num blocks is %i\n", res_table_num_blocks);
    //printf("Resource table spans 0x%x to 0x%x\n", res_table_offset, new_hdr.num_resource_entries * sizeof(ResourceTableBlock));

    //printf("Resident name table offset is 0x%x\n", new_hdr.resi_name_table_offset + new_hdr_offset);
    //printf("Module ref table offset is 0x%x\n", new_hdr.module_ref_table_offset + new_hdr_offset);
    //printf("Imported name table offset is 0x%x\n", new_hdr.imported_names_table_offset + new_hdr_offset);
    //printf("Non resi name table offset 0x%x. Num items %i\n", new_hdr.non_resi_name_table_offset,
//        new_hdr.non_resi_name_table_offset + new_hdr.non_resi_name_table_num_bytes);
    
    fseek(f, res_table_offset, SEEK_SET);
//    char *resource_table = f.data + f.offset;
    u16 alignment_shift_amount; fread(&alignment_shift_amount, 1, sizeof(u16), f);
    int block_size = 1 << alignment_shift_amount;

    FullFnt *all_fnts[16] = { NULL };
    int num_fnts = 0;
    while (1) {
        //printf("\nResource block offset 0x%x. ", f.offset);
        ResourceTableBlock rtblock; fread(&rtblock, 1, sizeof(ResourceTableBlock), f);
        //printf("type id 0x%x. Num of this type %i\n", rtblock.type_id, rtblock.num_of_this_type);

        if (rtblock.type_id == 0) {
            break;
        }
        else if (rtblock.type_id == 0x8008) {
            int next_block_offset = ftell(f) + sizeof(ResourceTableItem)* rtblock.num_of_this_type;
    
            int x = 0;
            for (int i = 0; i < rtblock.num_of_this_type; i++) {
                all_fnts[i] = ReadFntResourceItem(f, block_size);
                num_fnts++;

//                DoUpPrediction(all_fnts[i]->bmp);
                ScaleUpBlit(g_window->bmp, x, 0, 2, all_fnts[i]->bmp);
                x += (all_fnts[i]->bmp->width + 10);
            }

            break;
        }
        else {
            int next_block_offset = ftell(f) + sizeof(ResourceTableItem) * rtblock.num_of_this_type;
            
            ResourceTableItem rt_item; fread(&rt_item, 1, sizeof(ResourceTableItem), f);
            if (rtblock.type_id == 0x8007) {
                ReleaseAssert(!(rt_item.resource_id & 0x8000), "Bad resource id 2");
                //printf("Font dir name %s\n", resource_table + rt_item->resource_id);
            }
    
            fseek(f, next_block_offset, SEEK_SET);
        }
    }
    fclose(f);

    {
        char *fnt_name = GetNameFromPath(path);
        char *c_file_name = new char[strlen(path) + 2];
        strcpy(c_file_name, fnt_name);
        strcat(c_file_name, ".h");
        char *bin_file_name = new char[strlen(path) + 6];
        strcpy(bin_file_name, fnt_name);
        strcat(bin_file_name, ".dfbf");

        FILE *c_file = fopen(c_file_name, "w");
        ReleaseAssert(c_file, "Couldn't create output file '%s'", c_file_name);
        FILE *bin_file = fopen(bin_file_name, "wb");
        ReleaseAssert(bin_file, "Couldn't create output file '%s'", bin_file_name);

        // Write header into C file.
        fputs("#ifdef __GNUC__\n"
              "#define NO_UNUSED_WARNING __attribute__ ((unused))\n"
              "#else\n"
              "#define NO_UNUSED_WARNING\n"
              "#endif\n"
              "\n", c_file);

        // Write header into bin file.
        char version = 0;
        fprintf(bin_file, "dfbf%c%c", version, num_fnts);
        for (int i = 0; i < num_fnts; i++) {
            u32 dummy_offset = 0;
            fwrite(&dummy_offset, 1, 4, bin_file);
        }

        for (int i = 0; i < num_fnts; i++) {
            MemBuf buf;
            FullFnt *fnt = all_fnts[i];
            WriteDfbfToMemBuf(&buf, fnt);
            buf.WriteToCFile(c_file, fnt_name, fnt->hdr.max_width, fnt->hdr.pix_height);
            
            u32 pos = ftell(bin_file);
            fseek(bin_file, 6 + i * 4, SEEK_SET);
            fwrite(&pos, 1, 4, bin_file);
            fseek(bin_file, pos, SEEK_SET);
            buf.WriteToBinFile(bin_file);
        }
    }

    while (!g_window->windowClosed && !g_window->input.keyDowns[KEY_ESC]) {
        InputPoll(g_window);
         
        UpdateWin(g_window);
        WaitVsync();
    }

    return 0;
}