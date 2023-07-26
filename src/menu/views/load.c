#include <libdragon.h>

#include "../rom_database.h"
#include "boot/boot.h"
#include "flashcart/flashcart.h"
#include "fragments/fragments.h"
#include "views.h"


static bool load_pending;
static rom_header_t rom_header;


static char *format_rom_endian (uint32_t endian) {
    switch (endian) {
        case ROM_BIG_ENDIAN:
        case IPL_BIG_ENDIAN:
            return "Big (native)";
        case ROM_LITTLE_ENDIAN:
            return "Little (unsupported)";
        case ROM_MID_BIG_ENDIAN:
            return "Byte swapped";
        default:
            return "Unknown";
    }
}

static char *format_rom_media_type (uint8_t media_type) {
    switch (media_type) {
        case N64_CART:
            return "Cartridge";
        case N64_DISK:
            return "Disk";
        case N64_CART_EXPANDABLE:
            return "Cartridge (Expandable)";
        case N64_DISK_EXPANDABLE:
            return "Disk (Expandable)";
        case N64_ALECK64:
            return "Aleck64";
        default:
            return "Unknown";
    }
}

static char *format_rom_destination_market (uint8_t market_type) {
    // TODO: These are all assumptions and should be corrected if required.
    switch (market_type) {
        case MARKET_ALL:
            return "All";
        case MARKET_BRAZIL:
            return "Brazil (MPAL)";
        case MARKET_CHINA:
            return "China";
        case MARKET_GERMANY:
            return "Germany (PAL)";
        case MARKET_USA:
            return "USA (NTSC)";
        case MARKET_FRANCE:
            return "France (PAL)";
        case MARKET_NETHERLANDS:
            return "Netherlands (PAL)";
        case MARKET_ITALY:
            return "Italy (PAL)";
        case MARKET_JAPAN:
            return "Japan (NTSC)";
        case MARKET_KOREA:
            return "Korea";
        case MARKET_CANADA:
            return "Canada";
        case MARKET_SPAIN:
            return "Spain (PAL)";
        case MARKET_AUSTRAILA:
            return "Austraila (PAL)";
        case MARKET_SCANDINAVAIA:
            return "Scandinavaia";
        case MARKET_GATEWAY64_NTSC:
            return "Gateway (NTSC)";
        case MARKET_GATEWAY64_PAL:
            return "Gateway (PAL)";
        case MARKET_PAL_GENERIC:
            return "Generic (PAL)";
        case MARKET_PAL_X: //FIXME: some AUS ROM's use this so not only EUR
        case MARKET_PAL_Y:
        case MARKET_PAL_Z:
            return "Unknown (PAL)";

        default:
            return "Unknown";
    }
}

static char *format_rom_save_type (uint8_t save_type) {
    switch (save_type) {
        case DB_SAVE_TYPE_NONE:
            return "None";
        case DB_SAVE_TYPE_EEPROM_4K:
            return "EEPROM 4K";
        case DB_SAVE_TYPE_EEPROM_16K:
            return "EEPROM 16K";
        case DB_SAVE_TYPE_SRAM:
            return "SRAM";
        case DB_SAVE_TYPE_SRAM_BANKED:
            return "SRAM Banked";
        case DB_SAVE_TYPE_SRAM_128K:
            return "SRAM 128K";
        case DB_SAVE_TYPE_FLASHRAM:
            return "FlashRAM";
        case DB_SAVE_TYPE_CPAK:
            return "Controller PAK";
        default:
            return "Unknown";
    }
}

static char *format_rom_memory_type (uint8_t memory_type) {
    switch (memory_type)
    {
    case DB_MEMORY_EXPANSION_REQUIRED:
        return "Required";
        break;
    case DB_MEMORY_EXPANSION_RECOMMENDED:
        return "Recommended";
        break;
    case DB_MEMORY_EXPANSION_SUGGESTED:
        return "Suggested";
        break;
    case DB_MEMORY_EXPANSION_FAULTY:
        return "May require ROM patch";
        break;
    default:
        return "Not required";
        break;
    }
}

static flashcart_save_type_t convert_save_type (uint8_t save_type) {
    switch (save_type) {
        case DB_SAVE_TYPE_EEPROM_4K:
            return FLASHCART_SAVE_TYPE_EEPROM_4K;
        case DB_SAVE_TYPE_EEPROM_16K:
            return FLASHCART_SAVE_TYPE_EEPROM_16K;
        case DB_SAVE_TYPE_SRAM:
            return FLASHCART_SAVE_TYPE_SRAM;
        case DB_SAVE_TYPE_SRAM_BANKED:
            return FLASHCART_SAVE_TYPE_SRAM_BANKED;
        case DB_SAVE_TYPE_SRAM_128K:
            return FLASHCART_SAVE_TYPE_SRAM_128K;
        case DB_SAVE_TYPE_FLASHRAM:
            return FLASHCART_SAVE_TYPE_FLASHRAM;
        default:
            return FLASHCART_SAVE_TYPE_NONE;
    }
}


static void load (menu_t *menu) {
    menu->next_mode = MENU_MODE_BOOT;

    path_t *path = path_clone(menu->browser.directory);
    path_push(path, menu->browser.list[menu->browser.selected].name);

    bool byte_swap = (rom_header.endian == ROM_MID_BIG_ENDIAN);
    menu->flashcart_error = flashcart_load_rom(path_get(path), byte_swap);
    if (menu->flashcart_error != FLASHCART_OK) {
        menu->next_mode = MENU_MODE_FAULT;
        path_free(path);
        return;
    }

    uint8_t save_type = rom_db_match_save_type(rom_header);

    path_ext_replace(path, "sav");
    menu->flashcart_error = flashcart_load_save(path_get(path), convert_save_type(save_type));
    if (menu->flashcart_error != FLASHCART_OK) {
        menu->next_mode = MENU_MODE_FAULT;
        path_free(path);
        return;
    }

    path_free(path);

    menu->boot_params->device_type = BOOT_DEVICE_TYPE_ROM;
    menu->boot_params->reset_type = BOOT_RESET_TYPE_COLD;
    menu->boot_params->tv_type = BOOT_TV_TYPE_PASSTHROUGH;
    menu->boot_params->detect_cic_seed = true;
}


static void process (menu_t *menu) {
    if (menu->actions.enter) {
        load_pending = true;
    } else if (menu->actions.back) {
        menu->next_mode = MENU_MODE_BROWSER;
    }
}

static void draw (menu_t *menu, surface_t *d) {
    layout_t *layout = layout_get();

    const int text_x = layout->offset_x + layout->offset_text_x;
    int text_y = layout->offset_y + layout->offset_text_y;

    const color_t bg_color = RGBA32(0x00, 0x00, 0x00, 0xFF);
    const color_t text_color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF);

    rdpq_attach(d, NULL);
    rdpq_clear(bg_color);

    if (load_pending) {
        fragment_loader(d);
    } else {
        // Layout
        fragment_borders(d);

        // Text start
        fragment_text_start(text_color);

        // Main screen
        text_y += fragment_textf(text_x, text_y, "Title: %.20s", rom_header.title);
        text_y += fragment_textf(text_x, text_y, "Save type: %s", format_rom_save_type(rom_db_match_save_type(rom_header)));
        text_y += fragment_textf(text_x, text_y, "Media type: %c - %s", rom_header.metadata.media_type, format_rom_media_type(rom_header.metadata.media_type));
        text_y += fragment_textf(text_x, text_y, "Unique ID: %.2s", (char*)&(rom_header.metadata.unique_identifier));
        text_y += fragment_textf(text_x, text_y, "Destination market: %c - %s", rom_header.metadata.destination_market, format_rom_destination_market(rom_header.metadata.destination_market));
        text_y += fragment_textf(text_x, text_y, "Version: %hhu", rom_header.version);
        text_y += fragment_textf(text_x, text_y, "Checksum: 0x%016llX", rom_header.checksum);
        text_y += fragment_textf(text_x, text_y, "ROM endian: %s", format_rom_endian(rom_header.endian));
        text_y += fragment_textf(text_x, text_y, "Expansion PAK: %s", format_rom_memory_type(rom_db_match_expansion_pak(rom_header)));

        // Actions bar
        text_y = layout->actions_y + layout->offset_text_y;
        text_y += fragment_textf(text_x, text_y, "A: Load and run ROM");
        text_y += fragment_textf(text_x, text_y, "B: Exit");
    }

    rdpq_detach_show();
}


void view_load_init (menu_t *menu) {
    load_pending = false;

    path_t *path = path_clone(menu->browser.directory);
    path_push(path, menu->browser.list[menu->browser.selected].name);

    rom_header = file_read_rom_header(path_get(path));

    path_free(path);
}

void view_load_display (menu_t *menu, surface_t *display) {
    process(menu);
    draw(menu, display);
    if (load_pending) {
        load_pending = false;
        load(menu);
    }
}