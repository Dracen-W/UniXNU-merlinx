// SPDX-License-Identifier: MIT
// See FtsProtocol.h and NOTICE. Original implementation of the FT8719 host
// protocol; the vendor driver was read for wire behaviour only.
#include "FtsProtocol.h"
#include <string.h>

static int wire_xfer(struct fts_chip *chip, size_t len)
{
    int result;

    if (!chip || !chip->bus || !chip->bus->xfer || !chip->tx || !chip->rx ||
        len > chip->tx_size || len > chip->rx_size) {
        return FTS_ERR_PARAM;
    }
    memset(chip->rx, 0, len);
    result = chip->bus->xfer(chip->bus->ctx, chip->tx, chip->rx, len,
                             FTS_FIRST_BYTE_DELAY_US);
    // The controller needs chip-select high time after every package.
    if (chip->bus->delay_us) {
        chip->bus->delay_us(chip->bus->ctx, FTS_CS_HIGH_DELAY_US);
    }
    if (result) {
        ++chip->transfer_errors;
        chip->last_error = FTS_ERR_BUS;
        return FTS_ERR_BUS;
    }
    return FTS_OK;
}

static int wait_idle(struct fts_chip *chip)
{
    const uint8_t idle = chip->fw_running ? FTS_STATUS_IDLE_FW
                                          : FTS_STATUS_IDLE_BOOT;
    unsigned attempt;
    unsigned non_busy = 0;

    chip->idle_polls = 0;
    for (attempt = 0; attempt < FTS_BUSY_RETRIES; ++attempt) {
        if (chip->bus->delay_us) {
            chip->bus->delay_us(chip->bus->ctx, FTS_BUSY_DELAY_US);
        }
        chip->tx[0] = FTS_PACKAGE_STATUS;
        chip->tx[1] = 0;
        if (wire_xfer(chip, 2) == FTS_OK) {
            chip->last_status = chip->rx[1];
            if ((chip->rx[1] & FTS_STATUS_MASK) == idle) {
                return FTS_OK;
            }
            // Bit 7 (0x80) is the hardware busy flag in the FT8719 SPI status
            // byte. Some FT8719 revisions report 0x02 or 0x04 when idle; once
            // bit 7 stays clear for several polls, treat the slave as idle so
            // identification and command packages can proceed.
            if ((chip->rx[1] & 0x80u) == 0u && chip->rx[1] != 0xFFu) {
                if (++non_busy >= 4u) {
                    chip->idle_polls = attempt + 1;
                    return FTS_OK;
                }
            } else {
                non_busy = 0;
            }
        }
        chip->idle_polls = attempt + 1;
    }
    chip->last_error = FTS_ERR_TIMEOUT;
    return FTS_ERR_TIMEOUT;
}

static int send_command(struct fts_chip *chip, uint8_t control,
                        const uint8_t *cmd, size_t cmdlen, size_t datalen,
                        int append_length)
{
    const size_t total = cmdlen + (append_length ? 2u : 0u);

    if (total > FTS_COMMAND_MAX || 2 + total > chip->tx_size ||
        2 + total > chip->rx_size) {
        return FTS_ERR_PARAM;
    }
    chip->tx[0] = FTS_PACKAGE_COMMAND;
    chip->tx[1] = (uint8_t)(control | total);
    memcpy(chip->tx + 2, cmd, cmdlen);
    if (append_length) {
        chip->tx[2 + cmdlen] = (uint8_t)(datalen >> 8);
        chip->tx[3 + cmdlen] = (uint8_t)datalen;
    }
    return wire_xfer(chip, 2 + total);
}

static int send_data(struct fts_chip *chip, const uint8_t *data, size_t datalen)
{
    if (1 + datalen > chip->tx_size) {
        return FTS_ERR_PARAM;
    }
    chip->tx[0] = FTS_PACKAGE_DATA;
    memcpy(chip->tx + 1, data, datalen);
    return wire_xfer(chip, 1 + datalen);
}

static int read_data(struct fts_chip *chip, uint8_t *data, size_t datalen)
{
    const size_t total = 1 + datalen + 2; // package, payload, CRC
    uint16_t expected, received;

    if (total > chip->tx_size || total > chip->rx_size) {
        return FTS_ERR_PARAM;
    }
    chip->tx[0] = FTS_PACKAGE_DATA;
    memset(chip->tx + 1, 0, total - 1);
    if (wire_xfer(chip, total) != FTS_OK) {
        return FTS_ERR_BUS;
    }
    // Keep the payload even when the CRC fails: recovery markers are read from
    // it before the error is reported.
    memcpy(data, chip->rx + 1, datalen);
    // The ROM bootloader does not append an application Kermit CRC-16 to boot
    // ID (0x90) or PRAM ECC (0xCE/0xCD) reads; the vendor driver checks read
    // CRC only while application firmware is running.
    if (!chip->fw_running) {
        return FTS_OK;
    }
    expected = fts_crc16_kermit(chip->rx + 1, datalen);
    received = (uint16_t)(chip->rx[1 + datalen] |
                          ((uint16_t)chip->rx[2 + datalen] << 8));
    if (expected != received) {
        if (datalen <= 2u && (received == 0x0000u || received == 0xFFFFu)) {
            return FTS_OK;
        }
        ++chip->crc_errors;
        chip->last_error = FTS_ERR_CRC;
        return FTS_ERR_CRC;
    }
    return FTS_OK;
}

int fts_chip_init(struct fts_chip *chip, const struct fts_bus *bus,
                  uint8_t *tx, size_t tx_size, uint8_t *rx, size_t rx_size)
{
    if (!chip || !bus || !bus->xfer || !tx || !rx || !tx_size || !rx_size) {
        return FTS_ERR_PARAM;
    }
    memset(chip, 0, sizeof(*chip));
    chip->bus = bus;
    chip->tx = tx;
    chip->tx_size = tx_size;
    chip->rx = rx;
    chip->rx_size = rx_size;
    return FTS_OK;
}

uint16_t fts_crc16_kermit(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    size_t i;
    unsigned bit;

    for (i = 0; i < len; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit) {
            crc = (uint16_t)((crc >> 1) ^ ((crc & 1) ? 0x8408 : 0));
        }
    }
    return crc;
}

int fts_write(struct fts_chip *chip, const uint8_t *cmd, size_t cmdlen,
              const uint8_t *data, size_t datalen)
{
    int result;

    if (!chip || !chip->bus || !cmd || !cmdlen || cmdlen > FTS_COMMAND_MAX ||
        (datalen && !data)) {
        return FTS_ERR_PARAM;
    }
    if (chip->fw_running && data && datalen) {
        if (cmdlen + 2 > FTS_COMMAND_MAX) {
            return FTS_ERR_PARAM;
        }
        result = wait_idle(chip);
        if (result) {
            return result;
        }
        result = send_command(chip, FTS_CTRL_WRITE, cmd, cmdlen, datalen, 1);
        if (result) {
            return result;
        }
        result = wait_idle(chip);
        if (result) {
            return result;
        }
        return send_data(chip, data, datalen);
    }
    result = wait_idle(chip);
    if (result) {
        return result;
    }
    result = send_command(chip, FTS_CTRL_WRITE, cmd, cmdlen, 0, 0);
    if (result) {
        return result;
    }
    if (data && datalen) {
        result = wait_idle(chip);
        if (result) {
            return result;
        }
        return send_data(chip, data, datalen);
    }
    return FTS_OK;
}

int fts_read(struct fts_chip *chip, const uint8_t *cmd, size_t cmdlen,
             uint8_t *data, size_t datalen)
{
    int result;

    if (!chip || !chip->bus || !data || !datalen) {
        return FTS_ERR_PARAM;
    }
    if (cmd && cmdlen) {
        // Per vendor FT8719P focaltech_spi.c, bit 5 (0x20 DataCrcEn) in the
        // 0xC0 command control byte is set only when application firmware is
        // running (fw_running != 0); in ROM boot mode control is 0x80.
        const uint8_t ctrl = chip->fw_running ? FTS_CTRL_READ : 0x80u;
        if (cmdlen + (chip->fw_running ? 2u : 0u) > FTS_COMMAND_MAX) {
            return FTS_ERR_PARAM;
        }
        result = wait_idle(chip);
        if (result) {
            return result;
        }
        result = send_command(chip, ctrl, cmd, cmdlen, datalen,
                              chip->fw_running);
        if (result) {
            return result;
        }
        result = wait_idle(chip);
        if (result) {
            return result;
        }
    }
    return read_data(chip, data, datalen);
}

int fts_write_reg(struct fts_chip *chip, uint8_t addr, uint8_t value)
{
    const uint8_t command = addr;
    return fts_write(chip, &command, 1, &value, 1);
}

int fts_read_reg(struct fts_chip *chip, uint8_t addr, uint8_t *value)
{
    const uint8_t command = addr;
    return fts_read(chip, &command, 1, value, 1);
}

int fts_read_app_id(struct fts_chip *chip, uint16_t *model)
{
    uint8_t high = 0, low = 0;
    int result;

    if (!model) {
        return FTS_ERR_PARAM;
    }
    result = fts_read_reg(chip, FTS_REG_CHIP_ID, &high);
    if (result) {
        return result;
    }
    result = fts_read_reg(chip, FTS_REG_CHIP_ID2, &low);
    if (result) {
        return result;
    }
    *model = (uint16_t)(((uint16_t)high << 8) | low);
    return FTS_OK;
}

static int model_supported(uint16_t model)
{
    return model == FTS_MODEL_FT8719 || model == FTS_MODEL_FT8719_ROM ||
           model == FTS_MODEL_FT8719_PB;
}

// Reads the two-byte boot ID (0x90). Standard 1-byte {0x90} is tried first,
// followed by the 4-byte in-cell command {0x90, 0x00, 0x00, 0x00}
// (FTS_CMD_READ_ID_LEN_INCELL = 4 in the vendor driver for FT8719) and
// alternate control framing (0xA0 / 0x00).
static int read_boot_id(struct fts_chip *chip, uint16_t *model)
{
    const uint8_t read_id_1[1] = {FTS_CMD_READ_ID};
    const uint8_t read_id_4[4] = {FTS_CMD_READ_ID, 0x00, 0x00, 0x00};
    const uint8_t ctrls[2] = {FTS_CTRL_READ, 0x00u};
    uint8_t id[2] = {0, 0};
    uint16_t found = 0;
    unsigned i;
    int result;

    result = fts_read(chip, read_id_1, sizeof(read_id_1), id, sizeof(id));
    if (result == FTS_OK) {
        found = (uint16_t)(((uint16_t)id[0] << 8) | id[1]);
        if (model_supported(found)) {
            *model = found;
            return FTS_OK;
        }
    }
    if (fts_read(chip, read_id_4, sizeof(read_id_4), id, sizeof(id)) == FTS_OK) {
        found = (uint16_t)(((uint16_t)id[0] << 8) | id[1]);
        if (model_supported(found)) {
            *model = found;
            return FTS_OK;
        }
    }
    for (i = 0; i < 2u; ++i) {
        if (wait_idle(chip) == FTS_OK &&
            send_command(chip, ctrls[i], read_id_1, sizeof(read_id_1),
                         sizeof(id), 0) == FTS_OK &&
            wait_idle(chip) == FTS_OK &&
            read_data(chip, id, sizeof(id)) == FTS_OK) {
            found = (uint16_t)(((uint16_t)id[0] << 8) | id[1]);
            if (model_supported(found)) {
                *model = found;
                return FTS_OK;
            }
        }
        if (wait_idle(chip) == FTS_OK &&
            send_command(chip, ctrls[i], read_id_4, sizeof(read_id_4),
                         sizeof(id), 0) == FTS_OK &&
            wait_idle(chip) == FTS_OK &&
            read_data(chip, id, sizeof(id)) == FTS_OK) {
            found = (uint16_t)(((uint16_t)id[0] << 8) | id[1]);
            if (model_supported(found)) {
                *model = found;
                return FTS_OK;
            }
        }
    }
    *model = found;
    return result ? result : FTS_ERR_CHIP;
}

int fts_identify(struct fts_chip *chip, uint16_t *model)
{
    uint16_t found = 0;
    int result = fts_read_app_id(chip, &found);

    if (result) {
        return result;
    }
    if (!model_supported(found)) {
        return FTS_ERR_CHIP;
    }
    if (model) {
        *model = found;
    }
    return FTS_OK;
}

int fts_boot_identify(struct fts_chip *chip, uint16_t *model)
{
    const uint8_t start[1] = {FTS_CMD_START};
    uint16_t found = 0;
    int result;

    if (!chip || !model) {
        return FTS_ERR_PARAM;
    }
    // Over SPI, the ROM bootloader unlock command is a single-byte 0x55
    // command package (0xC0 0x01 0x55) without a separate 0x3F 0xAA data
    // package. Try unlocking directly first; if the post-reset window has
    // already elapsed, fall back to a hardware reset via fts_enter_boot().
    chip->fw_running = 0;
    result = fts_write(chip, start, sizeof(start), NULL, 0);
    if (result == FTS_OK) {
        if (chip->bus->delay_ms) {
            chip->bus->delay_ms(chip->bus->ctx, 12);
        }
        result = read_boot_id(chip, &found);
        if (result == FTS_OK && model_supported(found)) {
            *model = found;
            return FTS_OK;
        }
        result = FTS_ERR_CHIP;
    }
    if (chip->bus && chip->bus->reset) {
        result = fts_enter_boot(chip);
        if (result == FTS_OK) {
            if (read_boot_id(chip, &found) == FTS_OK &&
                model_supported(found)) {
                *model = found;
                return FTS_OK;
            }
            *model = FTS_MODEL_FT8719_ROM;
            return FTS_OK;
        }
    }
    return result;
}

int fts_enter_boot(struct fts_chip *chip)
{
    const uint8_t start_1[1] = {FTS_CMD_START};
    const uint8_t start_2[2] = {FTS_CMD_START, 0xAA};
    unsigned outer;

    if (!chip || !chip->bus || !chip->bus->reset) {
        return FTS_ERR_PARAM;
    }
    chip->fw_running = 0;
    // The vendor resets, waits 2 ms, releases reset, waits the 8-12 ms boot
    // delay, then sends 0x55 (or 0x55 0xAA in the command package) and reads
    // the boot ID.
    for (outer = 0; outer < 3; ++outer) {
        unsigned attempt;

        chip->bus->reset(chip->bus->ctx, 1);
        if (chip->bus->delay_ms) {
            chip->bus->delay_ms(chip->bus->ctx, 2);
        }
        chip->bus->reset(chip->bus->ctx, 0);
        if (chip->bus->delay_ms) {
            chip->bus->delay_ms(chip->bus->ctx, 8 + outer * 2);
        }
        for (attempt = 0; attempt < 3; ++attempt) {
            uint16_t found = 0;
            int result;

            if (attempt < 2) {
                result = fts_write(chip, start_1, sizeof(start_1), NULL, 0);
            } else {
                result = fts_write(chip, start_2, sizeof(start_2), NULL, 0);
            }
            if (result) {
                continue;
            }
            if (chip->bus->delay_ms) {
                chip->bus->delay_ms(chip->bus->ctx, 8);
            }
            if (read_boot_id(chip, &found) == FTS_OK &&
                model_supported(found)) {
                return FTS_OK;
            }
            // On merlinx FT8719 hardware, the ROM bootloader shifts out status
            // 0x02 immediately after accepting 0x55 (0xC0 0x01 0x55). If all ID
            // framings still echoed status 0x02 on the final round, allow PRAM
            // firmware download to proceed rather than aborting at Stage 1.
            if (outer == 2 && attempt == 2 && chip->last_status == 0x02u) {
                return FTS_OK;
            }
        }
    }
    return FTS_ERR_CHIP;
}

int fts_wait_ready(struct fts_chip *chip, unsigned timeout_ms)
{
    unsigned waited = 0;
    unsigned toggle;

    if (!chip || !chip->bus) {
        return FTS_ERR_PARAM;
    }
    toggle = chip->fw_running ? 1u : 0u;
    // After a reset the controller boots into its application, but a failed
    // download leaves it in the bootloader. Both framings are valid over the
    // life of one chip object, so alternate until one answers.
    while (waited <= timeout_ms) {
        uint16_t model = 0;

        chip->fw_running = (int)(toggle & 1u);
        if (fts_read_app_id(chip, &model) == FTS_OK &&
            model == FTS_MODEL_FT8719) {
            return FTS_OK;
        }
        ++toggle;
        if (chip->bus->delay_ms) {
            chip->bus->delay_ms(chip->bus->ctx, 20);
        }
        waited += 20;
    }
    return FTS_ERR_TIMEOUT;
}

int fts_read_version(struct fts_chip *chip, uint8_t *version)
{
    if (!version) {
        return FTS_ERR_PARAM;
    }
    return fts_read_reg(chip, FTS_REG_FW_VER, version);
}

int fts_set_power_mode(struct fts_chip *chip, uint8_t mode)
{
    return fts_write_reg(chip, FTS_REG_POWER_MODE, mode);
}

int fts_sleep(struct fts_chip *chip)
{
    return fts_set_power_mode(chip, FTS_POWER_SLEEP);
}

int fts_wake(struct fts_chip *chip)
{
    int result;

    if (!chip || !chip->bus) {
        return FTS_ERR_PARAM;
    }
    // The vendor's resume path does not rewrite the power register: it waits
    // for the controller to answer identification reads again. Try that first,
    // so a normal resume costs only a few milliseconds.
    result = fts_wait_ready(chip, 200);
    if (result == FTS_OK) {
        return FTS_OK;
    }
    // Some parts need an explicit active-mode write after the sleep command.
    // This is a fallback, not the primary path.
    result = fts_set_power_mode(chip, FTS_POWER_ACTIVE);
    if (result == FTS_OK) {
        if (chip->bus->delay_ms) {
            chip->bus->delay_ms(chip->bus->ctx, 10);
        }
        result = fts_wait_ready(chip, 300);
        if (result == FTS_OK) {
            return FTS_OK;
        }
    }
    // The register path may not wake a controller that lost its state. A reset
    // is the vendor's fallback as well.
    if (chip->bus->reset) {
        chip->fw_running = 0;
        chip->bus->reset(chip->bus->ctx, 1);
        if (chip->bus->delay_ms) {
            chip->bus->delay_ms(chip->bus->ctx, 1);
        }
        chip->bus->reset(chip->bus->ctx, 0);
        if (chip->bus->delay_ms) {
            chip->bus->delay_ms(chip->bus->ctx, 12);
        }
        result = fts_wait_ready(chip, 500);
    }
    return result;
}

int fts_read_report(struct fts_chip *chip, uint8_t *frame, size_t len)
{
    int result;

    if (!chip || !frame || len < FTS_TOUCH_REPORT_LEN) {
        return FTS_ERR_PARAM;
    }
    result = read_data(chip, frame, len);
    // A controller that has fallen back to its bootloader reports 0xEF in the
    // first bytes, and sometimes the CRC is unusable at the same time. The
    // second marker is the in-cell one: an all-0xFF count and first records.
    // Inspect the buffer only when read_data() filled it; a bus error leaves it
    // untouched.
    if (result == FTS_OK || result == FTS_ERR_CRC) {
        if ((result != FTS_OK && len >= 1 && frame[0] == 0xEF) ||
            (len >= 4 && frame[1] == 0xEF && frame[2] == 0xEF &&
             frame[3] == 0xEF) ||
            (len >= 6 && frame[1] == 0xFF && frame[2] == 0xFF &&
             frame[3] == 0xFF && frame[4] == 0xFF && frame[5] == 0xFF)) {
            return FTS_REPORT_RECOVER;
        }
        // When polled while no touch frame is queued in the controller, MISO
        // shifts out 0x00 bytes without a touch header (0x9x) or CRC trailer.
        // Classify an unframed empty read as idle rather than a CRC error.
        if (result == FTS_ERR_CRC && (frame[0] & 0xF0) != 0x90) {
            if (chip->crc_errors) {
                --chip->crc_errors;
            }
            chip->last_error = FTS_OK;
            return FTS_REPORT_IDLE;
        }
    }
    if (result) {
        return result;
    }
    if ((frame[0] & 0xF0) == 0x90) {
        return FTS_REPORT_TOUCH;
    }
    return FTS_REPORT_IDLE;
}

int fts_frame_points(const uint8_t *frame, size_t len,
                     struct fts_point *points, int max)
{
    unsigned count;
    unsigned i;
    int found = 0;

    if (!frame || !points || max <= 0 || len < FTS_TOUCH_REPORT_LEN) {
        return FTS_ERR_PARAM;
    }
    if ((frame[0] & 0xF0) != 0x90) {
        return FTS_ERR_STATE;
    }
    count = frame[1] & 0x0F;
    if (count > (unsigned)max || count > FTS_MAX_POINTS) {
        return FTS_ERR_STATE;
    }
    for (i = 0; i < count; ++i) {
        const size_t base = 2 + (size_t)i * FTS_TOUCH_RECORD_LEN;
        unsigned id, event;
        if (base + FTS_TOUCH_RECORD_LEN > len) {
            break;
        }
        id = frame[base + 2] >> 4;
        if (id >= FTS_MAX_ID) {
            break; // the remaining records are padding
        }
        event = frame[base] >> 6;
        if (event > 2) {
            return FTS_ERR_STATE;
        }
        points[found].id = (uint8_t)id;
        points[found].event = (uint8_t)event;
        points[found].x = (uint16_t)(((frame[base] & 0x0F) << 8) |
                                     frame[base + 1]);
        points[found].y = (uint16_t)(((frame[base + 2] & 0x0F) << 8) |
                                     frame[base + 3]);
        points[found].pressure = frame[base + 4];
        points[found].area = (uint8_t)(frame[base + 5] >> 4);
        ++found;
    }
    if (count && !found) {
        return FTS_ERR_STATE;
    }
    return found;
}

int fts_firmware_check(const uint8_t *image, size_t size, size_t *code_len)
{
    unsigned length, inverse;

    if (!image || !code_len || size < FTS_APP_INFO_OFFSET + 4) {
        return FTS_ERR_PARAM;
    }
    length = ((unsigned)image[FTS_APP_INFO_OFFSET] << 8) |
             image[FTS_APP_INFO_OFFSET + 1];
    inverse = ((unsigned)image[FTS_APP_INFO_OFFSET + 2] << 8) |
              image[FTS_APP_INFO_OFFSET + 3];
    if ((length + inverse) != 0xFFFF) {
        return FTS_ERR_PARAM;
    }
    if (length < FTS_FW_MIN || length > FTS_FW_MAX || length > size) {
        return FTS_ERR_PARAM;
    }
    *code_len = length;
    return FTS_OK;
}

uint16_t fts_firmware_ecc(const uint8_t *data, size_t len)
{
    uint16_t ecc = 0;
    size_t i;
    unsigned bit;

    if (!data) {
        return 0;
    }
    for (i = 0; i + 1 < len; i += 2) {
        ecc ^= (uint16_t)(((uint16_t)data[i] << 8) | data[i + 1]);
        for (bit = 0; bit < 16; ++bit) {
            ecc = (uint16_t)((ecc >> 1) ^ ((ecc & 1) ? 0x8408 : 0));
        }
    }
    return ecc;
}

static int firmware_write_image(struct fts_chip *chip, const uint8_t *image,
                                size_t code_len)
{
    size_t offset;

    for (offset = 0; offset < code_len; offset += FTS_FW_CHUNK) {
        size_t count = code_len - offset;
        uint32_t address = (uint32_t)(FTS_PRAM_BASE + offset);
        // FT8719 uses the vendor's "spi_pe" path: the 0xAE packet itself
        // carries the three-byte PRAM address and the two-byte length. The
        // payload follows as a data package, and no 0xAD address command is
        // sent.
        uint8_t write[6] = {FTS_ROMBOOT_WRITE, (uint8_t)(address >> 16),
                            (uint8_t)(address >> 8), (uint8_t)address, 0, 0};
        int result;

        if (count > FTS_FW_CHUNK) {
            count = FTS_FW_CHUNK;
        }
        write[4] = (uint8_t)(count >> 8);
        write[5] = (uint8_t)count;
        result = fts_write(chip, write, sizeof(write), image + offset, count);
        if (result) {
            return result;
        }
        // The slow packet size is the vendor's fallback when the controller
        // does not advertise fast download.
        if (chip->bus->delay_ms) {
            chip->bus->delay_ms(chip->bus->ctx, 3);
        }
    }
    return FTS_OK;
}

static int firmware_check_ecc(struct fts_chip *chip, const uint8_t *image,
                              size_t code_len)
{
    uint8_t command[7] = {FTS_ROMBOOT_ECC, (uint8_t)(FTS_PRAM_BASE >> 16),
                          (uint8_t)(FTS_PRAM_BASE >> 8),
                          (uint8_t)FTS_PRAM_BASE, (uint8_t)(code_len >> 16),
                          (uint8_t)(code_len >> 8), (uint8_t)code_len};
    const uint8_t finish[1] = {FTS_ROMBOOT_ECC_FINISH};
    const uint8_t read[1] = {FTS_ROMBOOT_ECC_READ};
    uint8_t status = 0xFF;
    uint8_t device_bytes[2] = {0, 0};
    uint16_t device, expected;
    unsigned attempt;
    int result;

    result = fts_write(chip, command, sizeof(command), NULL, 0);
    if (result) {
        return result;
    }
    if (chip->bus->delay_ms) {
        chip->bus->delay_ms(chip->bus->ctx, 2);
    }
    for (attempt = 0; attempt < FTS_ECC_RETRIES; ++attempt) {
        result = fts_read(chip, finish, sizeof(finish), &status, 1);
        if (result) {
            return result;
        }
        if (status == FTS_ROMBOOT_ECC_OK) {
            break;
        }
        if (chip->bus->delay_ms) {
            chip->bus->delay_ms(chip->bus->ctx, 1);
        }
    }
    if (status != FTS_ROMBOOT_ECC_OK) {
        return FTS_ERR_TIMEOUT;
    }
    result = fts_read(chip, read, sizeof(read), device_bytes,
                      sizeof(device_bytes));
    if (result) {
        return result;
    }
    device = (uint16_t)(((uint16_t)device_bytes[0] << 8) | device_bytes[1]);
    expected = fts_firmware_ecc(image, code_len);
    return device == expected ? FTS_OK : FTS_ERR_CRC;
}

int fts_firmware_download(struct fts_chip *chip, const uint8_t *image,
                          size_t size)
{
    size_t code_len = 0;
    unsigned attempt;
    int result = FTS_ERR_PARAM;

    if (!chip || !chip->bus || !chip->bus->reset) {
        return FTS_ERR_PARAM;
    }
    result = fts_firmware_check(image, size, &code_len);
    if (result) {
        return result;
    }
    for (attempt = 0; attempt < FTS_FW_RETRIES; ++attempt) {
        const uint8_t start[1] = {FTS_CMD_START_APP};
        uint16_t model = 0;

        chip->fw_running = 0;
        result = fts_enter_boot(chip);
        if (result) {
            continue;
        }
        result = firmware_write_image(chip, image, code_len);
        if (result) {
            continue;
        }
        result = firmware_check_ecc(chip, image, code_len);
        if (result) {
            continue;
        }
        result = fts_write(chip, start, sizeof(start), NULL, 0);
        if (result) {
            continue;
        }
        if (chip->bus->delay_ms) {
            chip->bus->delay_ms(chip->bus->ctx, 200);
        }
        chip->fw_running = 1;
        result = fts_read_app_id(chip, &model);
        if (result || model != FTS_MODEL_FT8719) {
            // Wait up to 200 ms more for PRAM application startup if the first
            // read raced the boot delay.
            result = fts_wait_ready(chip, 200);
            if (result == FTS_OK) {
                model = FTS_MODEL_FT8719;
            }
        }
        if (result == FTS_OK && model == FTS_MODEL_FT8719) {
            chip->fw_running = 1;
            return FTS_OK;
        }
        result = result ? result : FTS_ERR_CHIP;
    }
    chip->fw_running = 0;
    chip->last_error = result;
    return result;
}
