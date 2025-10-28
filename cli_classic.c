/*
 * This file is part of the flashrom project.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2000 Silicon Integrated System Corporation
 * SPDX-FileCopyrightText: 2004 Tyan Corp <yhlu@tyan.com>
 * SPDX-FileCopyrightText: 2005-2008 coresystems GmbH
 * SPDX-FileCopyrightText: 2008,2009,2010 Carl-Daniel Hailfinger
 */

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <cli_getopt.h>
#include <cli_output.h>
#include <time.h>
#include "flash.h"
#include "flashchips.h"
#include "fmap.h"
#include "programmer.h"
#include "libflashrom.h"

#if CONFIG_RPMC_ENABLED == 1
#include "rpmc.h"
#endif /* CONFIG_RPMC_ENABLED */

enum {
	OPTION_IFD = 0x0100,
	OPTION_FMAP,
	OPTION_FMAP_FILE,
	OPTION_FMAP_VERIFY,
	OPTION_FLASH_CONTENTS,
	OPTION_FLASH_NAME,
	OPTION_FLASH_SIZE,
	OPTION_WP_STATUS,
	OPTION_WP_SET_RANGE,
	OPTION_WP_SET_REGION,
	OPTION_WP_ENABLE,
	OPTION_WP_DISABLE,
	OPTION_WP_LIST,
	OPTION_PROGRESS,
	OPTION_SACRIFICE_RATIO,
#if CONFIG_RPMC_ENABLED == 1
	OPTION_RPMC_READ_DATA,
	OPTION_RPMC_WRITE_ROOT_KEY,
	OPTION_RPMC_UPDATE_HMAC_KEY,
	OPTION_RPMC_INCREMENT_COUNTER,
	OPTION_RPMC_GET_COUNTER,
	OPTION_RPMC_COUNTER_ADDRESS,
	OPTION_RPMC_KEY_DATA,
	OPTION_RPMC_KEY_FILE,
#endif /* CONFIG_RPMC_ENABLED */
};

struct cli_options {
	bool read_it, extract_it, write_it, erase_it, verify_it;
	bool dont_verify_it, dont_verify_all;
	bool list_supported;
	char *filename;

	const struct programmer_entry *prog;
	char *pparam;

	bool ifd, fmap, fmap_verify;
	struct flashrom_layout *layout;
	struct layout_include_args *include_args;
	char *layoutfile;
	char *fmapfile;

	unsigned int wp_start, wp_len;
	bool enable_wp, disable_wp, print_wp_status;
	bool set_wp_range, set_wp_region, print_wp_ranges;
	char *wp_region;

	bool force;
	bool flash_name, flash_size;
	bool show_progress;
	char *logfile;
	char *referencefile;
	const char *chip_to_probe;
	int sacrifice_ratio;

#if CONFIG_RPMC_ENABLED == 1
	bool rpmc_read_data;
	bool rpmc_write_root_key;
	bool rpmc_update_hmac_key;
	bool rpmc_increment_counter;
	bool rpmc_get_counter;
	unsigned int rpmc_counter_address;
	uint32_t rpmc_key_data, rpmc_previous_counter_value;
	const char *rpmc_root_key_file;
#endif /* CONFIG_RPMC_ENABLED */
};

static void cli_classic_usage(const char *name)
{
	printf("Usage: %s [-h|-R|-L|"
	       "\n\t-p <programmername>[:<parameters>] [-c <chipname>]\n"
	       "\t\t(--flash-name|--flash-size|\n"
	       "\t\t [-E|-x|(-r|-w|-v) [<file>]]\n"
	       "\t\t [(-l <layoutfile>|--ifd| --fmap|--fmap-file <file>) [-i <region>[:<file>]]...]\n"
	       "\t\t [-n] [-N] [-f])]\n"
	       "\t[-V[V[V]]] [-o <logfile>]\n\n", name);

	printf(" -h | --help                        print this help text\n"
	       " -R | --version                     print version (release)\n"
	       " -r | --read [<file>]               read flash and save to <file>\n"
	       " -w | --write [<file>|-]            write <file> or the content provided\n"
	       "                                    on the standard input to flash\n"
	       " -v | --verify [<file>|-]           verify flash against <file>\n"
	       "                                    or the content provided on the standard input\n"
	       " -E | --erase                       erase flash memory\n"
	       " -V | --verbose                     more verbose output\n"
	       " -c | --chip <chipname>             probe only for specified flash chip\n"
	       " -f | --force                       force specific operations (see man page)\n"
	       " -n | --noverify                    don't auto-verify\n"
	       " -N | --noverify-all                verify included regions only (cf. -i)\n"
	       " -x | --extract                     extract regions to files\n"
	       " -l | --layout <layoutfile>         read ROM layout from <layoutfile>\n"
	       "      --wp-disable                  disable write protection\n"
	       "      --wp-enable                   enable write protection\n"
	       "      --wp-list                     list supported write protection ranges\n"
	       "      --wp-status                   show write protection status\n"
	       "      --wp-range=<start>,<len>      set write protection range (use --wp-range=0,0\n"
	       "                                    to unprotect the entire flash)\n"
	       "      --wp-region <region>          set write protection region\n"
	       "      --flash-name                  read out the detected flash name\n"
	       "      --flash-size                  read out the detected flash size\n"
	       "      --fmap                        read ROM layout from fmap embedded in ROM\n"
	       "      --fmap-file <fmapfile>        read ROM layout from fmap in <fmapfile>\n"
	       "      --fmap-verify                 read ROM layout from fmap and verify it matches file fmap\n"
	       "      --ifd                         read layout from an Intel Firmware Descriptor\n"
	       " -i | --include <region>[:<file>]   only read/write image <region> from layout\n"
	       "                                    (optionally with data from <file>)\n"
	       "      --image <region>[:<file>]     deprecated, please use --include\n"
	       " -o | --output <logfile>            log output to <logfile>\n"
	       "      --flash-contents <ref-file>   assume flash contents to be <ref-file>\n"
	       " -L | --list-supported              print supported devices\n"
	       "      --progress                    show progress percentage on the standard output\n"
	       "      --sacrifice-ratio <ratio>     Fraction (as a percentage, 0-50) of an erase block\n"
	       "                                    that may be erased even if unmodified. Larger values\n"
	       "				    may complete programming faster, but may also hurt\n"
	       "				    chip longevity by erasing cells unnecessarily.\n"
	       "				    Default is 0, tradeoff is the speed of programming\n"
	       "                                    operation VS the longevity of the chip. Default is\n"
	       "                                    longevity.\n"
	       "                                    DANGEROUS! It wears your chip faster!\n"
#if CONFIG_RPMC_ENABLED == 1
	       "RPMC COMMANDS\n"
	       "      --get-rpmc-status             read the extended status\n"
	       "      --write-root-key              write the root key register\n"
	       "      --update-hmac-key             update the hmac key register\n"
	       "      --increment-counter <current>\n"
	       "                                    increment rpmc counter\n"
	       "      --get-counter                 get rpmc counter\n"
	       "RPMC OPTIONS\n"
	       "      --counter-address <address>   counter address (default: 0)\n"
	       "      --rpmc-root-key <keyfile>     rpmc root key file\n"
	       "      --key-data <value>            hex number to use as key data (default: 0)\n"
#endif /* CONFIG_RPMC_ENABLED */
	       "PROGRAMMER SELECTION OPTIONS\n"
	       " -p | --programmer <name>[:<param>] specify the programmer device. One of\n");
	list_programmers_linebreak(4, 80, 0);
	printf(".\n\nYou can specify one of -h, -R, -L, -E, -r, -w, -v"
#if CONFIG_RPMC_ENABLED == 1
		 ", a RPMC command"
#endif /* CONFIG_RPMC_ENABLED */
		 " or no operation.\n"
	       "If no operation is specified, flashrom will only probe for flash chips.\n");
}

static void cli_classic_abort_usage(const char *msg)
{
	if (msg)
		fprintf(stderr, "%s", msg);
	printf("Please run \"flashrom --help\" for usage info.\n");
	exit(1);
}

static void cli_classic_validate_singleop(int *operation_specified)
{
	if (++(*operation_specified) > 1) {
		cli_classic_abort_usage("More than one operation specified. Aborting.\n");
	}
}

static int check_filename(char *filename, const char *type)
{
	if (!filename || (filename[0] == '\0')) {
		fprintf(stderr, "Error: No %s file specified.\n", type);
		return 1;
	}
	/* Not an error, but maybe the user intended to specify a CLI option instead of a file name. */
	if (filename[0] == '-' && filename[1] != '\0')
		fprintf(stderr, "Warning: Supplied %s file name starts with -\n", type);
	return 0;
}

/* Ensure a file is open by means of fstat */
static bool check_file(FILE *file)
{
	struct stat statbuf;

	if (fstat(fileno(file), &statbuf) < 0)
		return false;
	return true;
}

static int parse_wp_range(unsigned int *start, unsigned int *len)
{
	char *delim = NULL;

	if (!optarg) {
		msg_gerr("Error: No wp-range values provided\n");
		return -1;
	}

	if ((delim = strchr(optarg, ',')) != strrchr(optarg, ',') ||
			delim == NULL ||
			delim == optarg ||
			*(delim + 1) == '\0') {
		msg_gerr("Error: Invalid wp-range argument format. Valid format is --wp-range <start>,<end>\n");
		return -1;
	}

	*start = strtoul(optarg, NULL, 0);
	*len = strtoul(delim+1, NULL, 0);

	return 0;
}

static int print_wp_range(struct flashrom_flashctx *flash, size_t start, size_t len)
{
	/* Start address and length */
	msg_ginfo("start=0x%08zx length=0x%08zx ", start, len);

	/* Easily readable description like 'none' or 'lower 1/8' */
	size_t chip_len = flashrom_flash_getsize(flash);

	if (len == 0) {
		msg_ginfo("(none)");
	} else if (len == chip_len) {
		msg_ginfo("(all)");
	} else {
		const char *location = "";
		if (start == 0)
			location = "lower ";
		if (start == chip_len - len)
			location = "upper ";

		/* Remove common factors of 2 to simplify */
		/* the (range_len/chip_len) fraction. */
		while ((chip_len % 2) == 0 && (len % 2) == 0) {
			chip_len /= 2;
			len /= 2;
		}

		msg_ginfo("(%s%zu/%zu)", location, len, chip_len);
	}

	return 0;
}

static const char *get_wp_error_str(int err)
{
	switch (err) {
	case FLASHROM_WP_ERR_CHIP_UNSUPPORTED:
		return "WP operations are not implemented for this chip";
	case FLASHROM_WP_ERR_READ_FAILED:
		return "failed to read the current WP configuration";
	case FLASHROM_WP_ERR_WRITE_FAILED:
		return "failed to write the new WP configuration";
	case FLASHROM_WP_ERR_VERIFY_FAILED:
		return "unexpected WP configuration read back from chip";
	case FLASHROM_WP_ERR_MODE_UNSUPPORTED:
		return "the requested protection mode is not supported";
	case FLASHROM_WP_ERR_RANGE_UNSUPPORTED:
		return "the requested protection range is not supported";
	case FLASHROM_WP_ERR_RANGE_LIST_UNAVAILABLE:
		return "could not determine what protection ranges are available";
	case FLASHROM_WP_ERR_UNSUPPORTED_STATE:
		return "can't operate on current WP configuration of the chip";
	}
	return "unknown WP error";
}

static int wp_cli(
		struct flashctx *flash,
		bool enable_wp,
		bool disable_wp,
		bool print_wp_status,
		bool print_wp_ranges,
		bool set_wp_range,
		uint32_t wp_start,
		uint32_t wp_len)
{
	if (print_wp_ranges) {
		struct flashrom_wp_ranges *list;
		enum flashrom_wp_result ret = flashrom_wp_get_available_ranges(&list, flash);
		if (ret != FLASHROM_WP_OK) {
			msg_gerr("Failed to get list of protection ranges: %s\n",
				 get_wp_error_str(ret));
			return 1;
		}
		size_t count = flashrom_wp_ranges_get_count(list);

		msg_ginfo("Available protection ranges:\n");
		for (size_t i = 0; i < count; i++) {
			size_t start, len;

			flashrom_wp_ranges_get_range(&start, &len, list, i);
			msg_ginfo("\t");
			print_wp_range(flash, start, len);
			msg_ginfo("\n");
		}

		flashrom_wp_ranges_release(list);
	}

	if (set_wp_range || disable_wp || enable_wp) {
		enum flashrom_wp_mode old_mode = FLASHROM_WP_MODE_DISABLED;
		struct flashrom_wp_cfg *cfg = NULL;
		enum flashrom_wp_result ret = flashrom_wp_cfg_new(&cfg);

		if (ret == FLASHROM_WP_OK)
			ret = flashrom_wp_read_cfg(cfg, flash);

		if (ret == FLASHROM_WP_OK) {
			/* Store current WP mode for printing help text if */
			/* changing the cfg fails. */
			old_mode = flashrom_wp_get_mode(cfg);

			if (set_wp_range)
				flashrom_wp_set_range(cfg, wp_start, wp_len);

			if (disable_wp)
				flashrom_wp_set_mode(cfg, FLASHROM_WP_MODE_DISABLED);

			if (enable_wp)
				flashrom_wp_set_mode(cfg, FLASHROM_WP_MODE_HARDWARE);

			ret = flashrom_wp_write_cfg(flash, cfg);
		}

		flashrom_wp_cfg_release(cfg);

		if (ret != FLASHROM_WP_OK) {
			msg_gerr("Failed to apply new WP settings: %s\n",
				 get_wp_error_str(ret));

			/* Warn user if active WP is likely to have caused failure */
			if (ret == FLASHROM_WP_ERR_VERIFY_FAILED) {
				switch (old_mode) {
				case FLASHROM_WP_MODE_HARDWARE:
					msg_gerr("Note: hardware status register protection is enabled. "
						 "The chip's WP# pin must be set to an inactive voltage "
						 "level to be able to change the WP settings.\n");
					break;
				case FLASHROM_WP_MODE_POWER_CYCLE:
					msg_gerr("Note: power-cycle status register protection is enabled. "
						 "A power-off, power-on cycle is usually required to change "
						 "the chip's WP settings.\n");
					break;
				case FLASHROM_WP_MODE_PERMANENT:
					msg_gerr("Note: permanent status register protection is enabled. "
						 "The chip's WP settings cannot be modified.\n");
					break;
				default:
					break;
				}
			}

			return 1;
		}

		if (disable_wp)
			msg_ginfo("Disabled hardware protection\n");

		if (enable_wp)
			msg_ginfo("Enabled hardware protection\n");

		if (set_wp_range) {
			msg_ginfo("Activated protection range: ");
			print_wp_range(flash, wp_start, wp_len);
			msg_ginfo("\n");
		}
	}

	if (print_wp_status) {
		size_t start, len;
		enum flashrom_wp_mode mode;
		struct flashrom_wp_cfg *cfg = NULL;
		enum flashrom_wp_result ret = flashrom_wp_cfg_new(&cfg);

		if (ret == FLASHROM_WP_OK)
			ret = flashrom_wp_read_cfg(cfg, flash);

		if (ret != FLASHROM_WP_OK) {
			msg_gerr("Failed to get WP status: %s\n",
				 get_wp_error_str(ret));

			flashrom_wp_cfg_release(cfg);
			return 1;
		}

		flashrom_wp_get_range(&start, &len, cfg);
		mode = flashrom_wp_get_mode(cfg);
		flashrom_wp_cfg_release(cfg);

		msg_ginfo("Protection range: ");
		print_wp_range(flash, start, len);
		msg_ginfo("\n");

		msg_ginfo("Protection mode: ");
		switch (mode) {
		case FLASHROM_WP_MODE_DISABLED:
			msg_ginfo("disabled");
			break;
		case FLASHROM_WP_MODE_HARDWARE:
			msg_ginfo("hardware");
			break;
		case FLASHROM_WP_MODE_POWER_CYCLE:
			msg_ginfo("power_cycle");
			break;
		case FLASHROM_WP_MODE_PERMANENT:
			msg_ginfo("permanent");
			break;
		default:
			msg_ginfo("unknown");
			break;
		}
		msg_ginfo("\n");
	}

	return 0;
}

#if CONFIG_RPMC_ENABLED == 1
static int rpmc_cli(struct flashctx *flash,
		    const char *const key_file,
		    const uint32_t key_data,
		    const unsigned int counter_address,
		    const uint32_t previous_counter,
		    const bool op_read_data,
		    const bool op_write_root_key,
		    const bool op_update_hmac_key,
		    const bool op_increment_counter,
		    const bool op_get_counter)
{
	if (op_write_root_key) {
		enum rpmc_result result = rpmc_write_root_key(flash, key_file, counter_address);
		if (result != RPMC_SUCCESS) {
			msg_gerr("Failed to write root key\n%s", rpmc_describe_result(result));
			return 1;
		}

		msg_ginfo("Successfully wrote new root key for counter %u.\n", counter_address);
	}

	if (op_update_hmac_key) {
		enum rpmc_result result = rpmc_update_hmac_key(flash,
							       key_file,
							       key_data,
							       counter_address);
		if (result != RPMC_SUCCESS) {
			msg_gerr("Failed to update hmac key\n%s", rpmc_describe_result(result));
			return 1;
		}

		msg_ginfo("Successfully updated hmac key to 0x%08x for counter %u.\n",
			  key_data,
			  counter_address);
	}

	if (op_increment_counter) {
		enum rpmc_result result = rpmc_increment_counter(flash,
								 key_file,
								 key_data,
								 counter_address,
								 previous_counter);
		if (result != RPMC_SUCCESS) {
			msg_gerr("Failed to increment the counter\n%s", rpmc_describe_result(result));
			return 1;
		}

		msg_ginfo("Successfully incremented counter %u.\n", counter_address);
	}

	if (op_get_counter) {
		uint32_t counter_value;
		enum rpmc_result result = rpmc_get_monotonic_counter(flash,
								     key_file,
								     key_data,
								     counter_address,
								     &counter_value);
		if (result != RPMC_SUCCESS) {
			msg_gerr("Failed to get the counter value\n%s", rpmc_describe_result(result));
			return 1;
		}

		msg_ginfo("Returned counter value %u for counter %u\n",	counter_value, counter_address);
	}

	if (op_read_data) {
		struct rpmc_status_register status;
		enum rpmc_result result = rpmc_read_data(flash, &status);
		if (result != RPMC_SUCCESS) {
			msg_gerr("Failed to read read rpmc data\n%s", rpmc_describe_result(result));
			return 1;
		}

		msg_ginfo("Reading rpmc data returned:\n");

		char bin_buffer[9];
		uint8_t status_bits = status.status;
		for (int i = 7; i >= 0; i--){
			bin_buffer[i] = '0' + (status_bits & 1);
			status_bits = status_bits >> 1;
		}
		bin_buffer[8] = '\0';
		msg_ginfo("Extended Status: 0b%s\n", bin_buffer);

		msg_ginfo("Tag:\n");
		for (size_t i = 0; i < RPMC_TAG_LENGTH; i++){
			msg_ginfo("0x%02x ", status.tag[i]);
		}
		msg_ginfo("\n");

		msg_ginfo("Counter: %u\n", status.counter_data);

		msg_ginfo("Signature:\n");
		for (size_t i = 0; i < RPMC_SIGNATURE_LENGTH; i++){
			msg_ginfo("0x%02x ", status.signature[i]);
		}
		msg_ginfo("\n");
	}

	return 0;
}
#endif /* CONFIG_RPMC_ENABLED */

/**
 * @brief Reads content to buffer from one or more files.
 *
 * Reads content to supplied buffer from files. If a filename is specified for
 * individual regions using the partial read syntax ('-i <region>[:<filename>]')
 * then this will read file data into the corresponding region in the
 * supplied buffer.
 *
 * @param layout   The layout to be used.
 * @param buf      Chip-sized buffer to write data to
 * @return 0 on success
 */
static int read_buf_from_include_args(const struct flashrom_layout *const layout, unsigned char *buf)
{
	const struct romentry *entry = NULL;

	/*
	 * Content will be read from -i args, so they must not overlap since
	 * we need to know exactly what content to write to the ROM.
	 */
	if (included_regions_overlap(layout)) {
		msg_gerr("Error: Included regions must not overlap when writing.\n");
		return 1;
	}

	while ((entry = layout_next_included(layout, entry))) {
		if (!entry->file)
			continue;
		const struct flash_region *region = &entry->region;
		if (read_buf_from_file(buf + region->start,
				       region->end - region->start + 1, entry->file))
			return 1;
	}
	return 0;
}

/**
 * @brief Writes content from buffer to one or more files.
 *
 * Writes content from supplied buffer to files. If a filename is specified for
 * individual regions using the partial read syntax ('-i <region>[:<filename>]')
 * then this will write files using data from the corresponding region in the
 * supplied buffer.
 *
 * @param layout   The layout to be used.
 * @param buf      Chip-sized buffer to read data from
 * @return 0 on success
 */
static int write_buf_to_include_args(const struct flashrom_layout *const layout, unsigned char *buf)
{
	const struct romentry *entry = NULL;

	while ((entry = layout_next_included(layout, entry))) {
		if (!entry->file)
			continue;
		const struct flash_region *region = &entry->region;
		if (write_buf_to_file(buf + region->start,
				      region->end - region->start + 1, entry->file))
			return 1;
	}

	return 0;
}

static char *get_optional_filename(char *argv[])
{
	char *filename = NULL;

	/* filename was supplied in optarg (i.e. -rfilename) */
	if (optarg != NULL)
		filename = strdup(optarg);
	/* filename is on optind if it is not another flag (i.e. -r filename)
	 * - is treated as stdin, so we still strdup in this case
	 */
	else if (optarg == NULL && argv[optind] != NULL &&
		 (argv[optind][0] != '-' || argv[optind][1] == '\0'))
		filename = strdup(argv[optind++]);

	return filename;
}

static int do_read(struct flashctx *const flash, const char *const filename)
{
	int ret;

	unsigned long size = flashrom_flash_getsize(flash);
	unsigned char *buf = calloc(size, sizeof(unsigned char));
	if (!buf) {
		msg_gerr("Memory allocation failed!\n");
		return 1;
	}

	ret = flashrom_image_read(flash, buf, size);
	if (ret > 0)
		goto free_out;

	if (write_buf_to_include_args(get_layout(flash), buf)) {
		ret = 1;
		goto free_out;
	}
	if (filename)
		ret = write_buf_to_file(buf, size, filename);

free_out:
	free(buf);
	return ret;
}

static int do_extract(struct flashctx *const flash)
{
	prepare_layout_for_extraction(flash);
	return do_read(flash, NULL);
}

static int do_write(struct flashctx *const flash, const char *const filename, const char *const referencefile)
{
	const size_t flash_size = flashrom_flash_getsize(flash);
	int ret = 1;

	uint8_t *const newcontents = malloc(flash_size);
	uint8_t *const refcontents = referencefile ? malloc(flash_size) : NULL;

	if (!newcontents || (referencefile && !refcontents)) {
		msg_gerr("Out of memory!\n");
		goto _free_ret;
	}

	/* Read '-w' argument first... */
	if (filename) {
		if (read_buf_from_file(newcontents, flash_size, filename))
			goto _free_ret;
	}
	/*
	 * ... then update newcontents with contents from files provided to '-i'
	 * args if needed.
	 */
	if (read_buf_from_include_args(get_layout(flash), newcontents))
		goto _free_ret;

	if (referencefile) {
		if (read_buf_from_file(refcontents, flash_size, referencefile))
			goto _free_ret;
	}

	ret = flashrom_image_write(flash, newcontents, flash_size, refcontents);

_free_ret:
	free(refcontents);
	free(newcontents);
	return ret;
}

static int do_verify(struct flashctx *const flash, const char *const filename)
{
	const size_t flash_size = flashrom_flash_getsize(flash);
	int ret = 1;

	uint8_t *const newcontents = malloc(flash_size);
	if (!newcontents) {
		msg_gerr("Out of memory!\n");
		goto _free_ret;
	}

	/* Read '-v' argument first... */
	if (filename) {
		if (read_buf_from_file(newcontents, flash_size, filename))
			goto _free_ret;
	}
	/*
	 * ... then update newcontents with contents from files provided to '-i'
	 * args if needed.
	 */
	if (read_buf_from_include_args(get_layout(flash), newcontents))
		goto _free_ret;

	ret = flashrom_image_verify(flash, newcontents, flash_size);

_free_ret:
	free(newcontents);
	return ret;
}

/* Returns the number of buses commonly supported by the current programmer and flash chip where the latter
 * can not be completely accessed due to size/address limits of the programmer. */
static unsigned int count_max_decode_exceedings(const struct flashctx *flash,
		const struct decode_sizes *max_rom_decode_)
{
	unsigned int limitexceeded = 0;
	uint32_t size = flash->chip->total_size * 1024;
	enum chipbustype buses = flash->mst->buses_supported & flash->chip->bustype;

	if ((buses & BUS_PARALLEL) && (max_rom_decode_->parallel < size)) {
		limitexceeded++;
		msg_pdbg("Chip size %"PRIu32" kB is bigger than supported "
			 "size %"PRIu32" kB of chipset/board/programmer "
			 "for %s interface, "
			 "probe/read/erase/write may fail. ", size / 1024,
			 max_rom_decode_->parallel / 1024, "Parallel");
	}
	if ((buses & BUS_LPC) && (max_rom_decode_->lpc < size)) {
		limitexceeded++;
		msg_pdbg("Chip size %"PRIu32" kB is bigger than supported "
			 "size %"PRIu32" kB of chipset/board/programmer "
			 "for %s interface, "
			 "probe/read/erase/write may fail. ", size / 1024,
			 max_rom_decode_->lpc / 1024, "LPC");
	}
	if ((buses & BUS_FWH) && (max_rom_decode_->fwh < size)) {
		limitexceeded++;
		msg_pdbg("Chip size %"PRIu32" kB is bigger than supported "
			 "size %"PRIu32" kB of chipset/board/programmer "
			 "for %s interface, "
			 "probe/read/erase/write may fail. ", size / 1024,
			 max_rom_decode_->fwh / 1024, "FWH");
	}
	if ((buses & BUS_SPI) && (max_rom_decode_->spi < size)) {
		limitexceeded++;
		msg_pdbg("Chip size %"PRIu32" kB is bigger than supported "
			 "size %"PRIu32" kB of chipset/board/programmer "
			 "for %s interface, "
			 "probe/read/erase/write may fail. ", size / 1024,
			 max_rom_decode_->spi / 1024, "SPI");
	}
	return limitexceeded;
}

static void parse_options(int argc, char **argv, const char *optstring,
			  const struct option *long_options,
			  struct cli_options *options)
{
	const char *name;
	int namelen, opt;
	int option_index = 0, operation_specified = 0;

	/* FIXME: Delay all operation_specified checks until after command
	 * line parsing to allow --help overriding everything else.
	 */
	while ((opt = getopt_long(argc, argv, optstring,
				  long_options, &option_index)) != EOF) {
		switch (opt) {
		case 'r':
			cli_classic_validate_singleop(&operation_specified);
			options->filename = get_optional_filename(argv);
			options->read_it = true;
			break;
		case 'w':
			cli_classic_validate_singleop(&operation_specified);
			options->filename = get_optional_filename(argv);
			options->write_it = true;
			break;
		case 'v':
			//FIXME: gracefully handle superfluous -v
			cli_classic_validate_singleop(&operation_specified);
			if (options->dont_verify_it) {
				cli_classic_abort_usage("--verify and --noverify are mutually exclusive. Aborting.\n");
			}
			options->filename = get_optional_filename(argv);
			options->verify_it = true;
			break;
		case 'n':
			if (options->verify_it) {
				cli_classic_abort_usage("--verify and --noverify are mutually exclusive. Aborting.\n");
			}
			options->dont_verify_it = true;
			break;
		case 'N':
			options->dont_verify_all = true;
			break;
		case 'x':
			cli_classic_validate_singleop(&operation_specified);
			options->extract_it = true;
			break;
		case 'c':
			options->chip_to_probe = strdup(optarg);
			break;
		case 'V':
			verbose_screen++;
			if (verbose_screen > FLASHROM_MSG_DEBUG2)
				verbose_logfile = verbose_screen;
			break;
		case 'E':
			cli_classic_validate_singleop(&operation_specified);
			options->erase_it = true;
			break;
		case 'f':
			options->force = true;
			break;
		case 'l':
			if (options->layoutfile)
				cli_classic_abort_usage("Error: --layout specified more than once. Aborting.\n");
			if (options->ifd)
				cli_classic_abort_usage("Error: --layout and --ifd both specified. Aborting.\n");
			if (options->fmap)
				cli_classic_abort_usage("Error: --layout and --fmap-file both specified. Aborting.\n");
			options->layoutfile = strdup(optarg);
			break;
		case OPTION_IFD:
			if (options->layoutfile)
				cli_classic_abort_usage("Error: --layout and --ifd both specified. Aborting.\n");
			if (options->fmap)
				cli_classic_abort_usage("Error: --fmap-file and --ifd both specified. Aborting.\n");
			options->ifd = true;
			break;
		case OPTION_FMAP_FILE:
			if (options->fmap)
				cli_classic_abort_usage("Error: --fmap, --fmap-file, or --fmap-verify specified "
					"more than once. Aborting.\n");
			if (options->ifd)
				cli_classic_abort_usage("Error: --fmap-file and --ifd both specified. Aborting.\n");
			if (options->layoutfile)
				cli_classic_abort_usage("Error: --fmap-file and --layout both specified. Aborting.\n");
			options->fmapfile = strdup(optarg);
			options->fmap = true;
			break;
		case OPTION_FMAP:
			if (options->fmap)
				cli_classic_abort_usage("Error: --fmap, --fmap-file, or --fmap-verify specified "
					"more than once. Aborting.\n");
			if (options->ifd)
				cli_classic_abort_usage("Error: --fmap and --ifd both specified. Aborting.\n");
			if (options->layoutfile)
				cli_classic_abort_usage("Error: --layout and --fmap both specified. Aborting.\n");
			options->fmap = true;
			break;
		case OPTION_FMAP_VERIFY:
			if (options->fmap)
				cli_classic_abort_usage("Error: --fmap, --fmap-file, or --fmap-verify specified "
					"more than once. Aborting.\n");
			if (options->ifd)
				cli_classic_abort_usage("Error: --fmap-verify and --ifd both specified. Aborting.\n");
			if (options->layoutfile)
				cli_classic_abort_usage("Error: --fmap-verify and --layout both specified. Aborting.\n");
			if (options->read_it || options->verify_it)
				cli_classic_abort_usage("Error: --fmap-verify cannot be used with read or verify operations. Aborting.\n");
			options->fmap = true;
			options->fmap_verify = true;
			break;
		case 'i':
			if (register_include_arg(&options->include_args, optarg))
				cli_classic_abort_usage(NULL);
			break;
		case OPTION_FLASH_CONTENTS:
			if (options->referencefile)
				cli_classic_abort_usage("Error: --flash-contents specified more than once."
							"Aborting.\n");
			options->referencefile = strdup(optarg);
			break;
		case OPTION_FLASH_NAME:
			cli_classic_validate_singleop(&operation_specified);
			options->flash_name = true;
			break;
		case OPTION_FLASH_SIZE:
			cli_classic_validate_singleop(&operation_specified);
			options->flash_size = true;
			break;
		case OPTION_WP_STATUS:
			options->print_wp_status = true;
			break;
		case OPTION_WP_LIST:
			options->print_wp_ranges = true;
			break;
		case OPTION_WP_SET_RANGE:
			if (parse_wp_range(&options->wp_start, &options->wp_len) < 0)
				cli_classic_abort_usage("Incorrect wp-range arguments provided.\n");

			options->set_wp_range = true;
			break;
		case OPTION_WP_SET_REGION:
			options->set_wp_region = true;
			options->wp_region = strdup(optarg);
			break;
		case OPTION_WP_ENABLE:
			options->enable_wp = true;
			break;
		case OPTION_WP_DISABLE:
			options->disable_wp = true;
			break;
		case 'L':
			cli_classic_validate_singleop(&operation_specified);
			options->list_supported = true;
			break;
		case 'p':
			if (options->prog != NULL) {
				cli_classic_abort_usage("Error: --programmer specified "
					"more than once. You can separate "
					"multiple\nparameters for a programmer "
					"with \",\". Please see the man page "
					"for details.\n");
			}
			size_t p;
			for (p = 0; p < programmer_table_size; p++) {
				name = programmer_table[p]->name;
				namelen = strlen(name);
				if (strncmp(optarg, name, namelen) == 0) {
					switch (optarg[namelen]) {
					case ':':
						options->pparam = strdup(optarg + namelen + 1);
						if (!strlen(options->pparam)) {
							free(options->pparam);
							options->pparam = NULL;
						}
						options->prog = programmer_table[p];
						break;
					case '\0':
						options->prog = programmer_table[p];
						break;
					default:
						/* The continue refers to the
						 * for loop. It is here to be
						 * able to differentiate between
						 * foo and foobar.
						 */
						continue;
					}
					break;
				}
			}
			if (options->prog == NULL) {
				fprintf(stderr, "Error: Unknown programmer \"%s\". Valid choices are:\n",
					optarg);
				list_programmers_linebreak(0, 80, 0);
				msg_ginfo(".\n");
				cli_classic_abort_usage(NULL);
			}
			break;
		case 'R':
			/* print_version() is always called during startup. */
			cli_classic_validate_singleop(&operation_specified);
			exit(0);
			break;
		case 'h':
			cli_classic_validate_singleop(&operation_specified);
			cli_classic_usage(argv[0]);
			exit(0);
			break;
		case 'o':
			if (options->logfile) {
				fprintf(stderr, "Warning: -o/--output specified multiple times.\n");
				free(options->logfile);
			}

			options->logfile = strdup(optarg);
			if (options->logfile[0] == '\0') {
				cli_classic_abort_usage("No log filename specified.\n");
			}
			break;
		case OPTION_PROGRESS:
			options->show_progress = true;
			break;
		case OPTION_SACRIFICE_RATIO:
			/* It is okay to convert invalid input to 0. */
			options->sacrifice_ratio = atoi(optarg);
			break;
#if CONFIG_RPMC_ENABLED == 1
		case OPTION_RPMC_READ_DATA:
			options->rpmc_read_data = true;
			break;
		case OPTION_RPMC_WRITE_ROOT_KEY:
			options->rpmc_write_root_key = true;
			break;
		case OPTION_RPMC_UPDATE_HMAC_KEY:
			options->rpmc_update_hmac_key = true;
			break;
		case OPTION_RPMC_INCREMENT_COUNTER:
			options->rpmc_increment_counter = true;
			options->rpmc_previous_counter_value = strtoumax(optarg, NULL, 10);
			break;
		case OPTION_RPMC_GET_COUNTER:
			options->rpmc_get_counter = true;
			break;
		case OPTION_RPMC_COUNTER_ADDRESS:
			options->rpmc_counter_address = strtoumax(optarg, NULL, 10);
			break;
		case OPTION_RPMC_KEY_DATA:
			options->rpmc_key_data = strtoumax(optarg, NULL, 16);
			break;
		case OPTION_RPMC_KEY_FILE:
			options->rpmc_root_key_file = strdup(optarg);
			break;
#endif /* CONFIG_RPMC_ENABLED */
		default:
			cli_classic_abort_usage(NULL);
			break;
		}
	}

	if (optind < argc)
		cli_classic_abort_usage("Error: Extra parameter found.\n");
}

static void free_options(struct cli_options *options)
{
	cleanup_include_args(&options->include_args);
	free(options->filename);
	free(options->fmapfile);
	free(options->referencefile);
	free(options->layoutfile);
	free(options->pparam);
	free(options->wp_region);
	free(options->logfile);
	free((char *)options->chip_to_probe);
}

int main(int argc, char *argv[])
{
	const struct flashchip *chip = NULL;
	struct flashctx context = {0}; /* holds the active detected chip and other info */
	char *tempstr = NULL;
	int i, j;
	int ret = 0;
	int all_matched_count = 0;
	const char **all_matched_names = NULL;
	time_t time_start, time_end;

	struct cli_options options = { 0 };
	static const char optstring[] = "r::Rw::v::nNVEfc:l:i:p:Lzho:x";
	static const struct option long_options[] = {
		{"read",		2, NULL, 'r'},
		{"write",		2, NULL, 'w'},
		{"erase",		0, NULL, 'E'},
		{"verify",		2, NULL, 'v'},
		{"noverify",		0, NULL, 'n'},
		{"noverify-all",	0, NULL, 'N'},
		{"extract",		0, NULL, 'x'},
		{"chip",		1, NULL, 'c'},
		{"verbose",		0, NULL, 'V'},
		{"force",		0, NULL, 'f'},
		{"layout",		1, NULL, 'l'},
		{"ifd",			0, NULL, OPTION_IFD},
		{"fmap",		0, NULL, OPTION_FMAP},
		{"fmap-file",		1, NULL, OPTION_FMAP_FILE},
		{"fmap-verify",		0, NULL, OPTION_FMAP_VERIFY},
		{"image",		1, NULL, 'i'}, // (deprecated): back compatibility.
		{"include",		1, NULL, 'i'},
		{"flash-contents",	1, NULL, OPTION_FLASH_CONTENTS},
		{"flash-name",		0, NULL, OPTION_FLASH_NAME},
		{"flash-size",		0, NULL, OPTION_FLASH_SIZE},
		{"get-size",		0, NULL, OPTION_FLASH_SIZE}, // (deprecated): back compatibility.
		{"wp-status",		0, NULL, OPTION_WP_STATUS},
		{"wp-list",		0, NULL, OPTION_WP_LIST},
		{"wp-range",		1, NULL, OPTION_WP_SET_RANGE},
		{"wp-region",		1, NULL, OPTION_WP_SET_REGION},
		{"wp-enable",		0, NULL, OPTION_WP_ENABLE},
		{"wp-disable",		0, NULL, OPTION_WP_DISABLE},
		{"list-supported",	0, NULL, 'L'},
		{"programmer",		1, NULL, 'p'},
		{"help",		0, NULL, 'h'},
		{"version",		0, NULL, 'R'},
		{"output",		1, NULL, 'o'},
		{"progress",		0, NULL, OPTION_PROGRESS},
		{"sacrifice-ratio",	1, NULL, OPTION_SACRIFICE_RATIO},
#if CONFIG_RPMC_ENABLED == 1
		{"get-rpmc-status",	0, NULL, OPTION_RPMC_READ_DATA},
		{"write-root-key",	0, NULL, OPTION_RPMC_WRITE_ROOT_KEY},
		{"update-hmac-key",	0, NULL, OPTION_RPMC_UPDATE_HMAC_KEY},
		{"increment-counter",	1, NULL, OPTION_RPMC_INCREMENT_COUNTER},
		{"get-counter",		0, NULL, OPTION_RPMC_GET_COUNTER},
		{"counter-address",	1, NULL, OPTION_RPMC_COUNTER_ADDRESS},
		{"key-data",		1, NULL, OPTION_RPMC_KEY_DATA},
		{"rpmc-root-key",	1, NULL, OPTION_RPMC_KEY_FILE},
#endif /* CONFIG_RPMC_ENABLED */
		{NULL,			0, NULL, 0},
	};

	/*
	 * Safety-guard against a user who has (mistakenly) closed
	 * stdout or stderr before exec'ing flashrom.  We disable
	 * logging in this case to prevent writing log data to a flash
	 * chip when a flash device gets opened with fd 1 or 2.
	 */
	if (check_file(stdout) && check_file(stderr)) {
		/* This is maximum log level for callback to be invoked,
		 * and cli wants callback to be always invoked. */
		flashrom_set_log_level(FLASHROM_MSG_SPEW);
		flashrom_set_log_callback(&flashrom_print_cb);
	}

	print_version();
	print_banner();

	setbuf(stdout, NULL);

	parse_options(argc, argv, optstring, long_options, &options);

	if (options.filename && check_filename(options.filename, "image"))
		cli_classic_abort_usage(NULL);
	if (options.layoutfile && check_filename(options.layoutfile, "layout"))
		cli_classic_abort_usage(NULL);
	if (options.fmapfile && check_filename(options.fmapfile, "fmap"))
		cli_classic_abort_usage(NULL);
	if (options.referencefile && check_filename(options.referencefile, "reference"))
		cli_classic_abort_usage(NULL);
	if (options.logfile && check_filename(options.logfile, "log"))
		cli_classic_abort_usage(NULL);
	if (options.logfile && open_logfile(options.logfile))
		cli_classic_abort_usage(NULL);

	if (options.list_supported) {
		if (print_supported())
			ret = 1;
		goto out;
	}

	start_logging();

	print_buildinfo();
	msg_gdbg("Command line (%i args):", argc - 1);
	for (i = 0; i < argc; i++) {
		msg_gdbg(" %s", argv[i]);
	}
	msg_gdbg("\n");

	if (options.layoutfile && layout_from_file(&options.layout, options.layoutfile)) {
		ret = 1;
		goto out;
	}

	if (!options.ifd && !options.fmap && process_include_args(options.layout, options.include_args)) {
		ret = 1;
		goto out;
	}
	/* Does a chip with the requested name exist in the flashchips array? */
	if (options.chip_to_probe) {
		for (chip = flashchips; chip && chip->name; chip++)
			if (!strcmp(chip->name, options.chip_to_probe))
				break;
		if (!chip || !chip->name) {
			msg_cerr("Error: Unknown chip '%s' specified.\n", options.chip_to_probe);
			msg_gerr("Run flashrom -L to view the hardware supported in this flashrom version.\n");
			ret = 1;
			goto out;
		}
		/* Keep chip around for later usage in case a forced read is requested. */
	}

	if (options.prog == NULL) {
		const struct programmer_entry *const default_programmer = CONFIG_DEFAULT_PROGRAMMER_NAME;

		if (default_programmer) {
			options.prog = default_programmer;
			/* We need to strdup here because we free(pparam) unconditionally later. */
			options.pparam = strdup(CONFIG_DEFAULT_PROGRAMMER_ARGS);
			msg_pinfo("Using default programmer \"%s\" with arguments \"%s\".\n",
				  default_programmer->name, options.pparam);
		} else {
			msg_perr("Please select a programmer with the --programmer parameter.\n"
#if CONFIG_INTERNAL == 1
				 "To choose the mainboard of this computer use 'internal'. "
#endif
				 "Valid choices are:\n");
			list_programmers_linebreak(0, 80, 0);
			msg_ginfo(".\n");
			ret = 1;
			goto out;
		}
	}

	/* FIXME: Delay calibration should happen in programmer code. */
	if (flashrom_init(1))
		exit(1);

	time(&time_start);

	if (programmer_init(options.prog, options.pparam)) {
		msg_perr("Error: Programmer initialization failed.\n");
		ret = 1;
		goto out_shutdown;
	}
	tempstr = flashbuses_to_text(get_buses_supported());
	msg_pdbg("The following protocols are supported: %s.\n", tempstr ? tempstr : "?");
	free(tempstr);

	all_matched_count = flashrom_flash_probe_v2(&context, &all_matched_names,
                                NULL, options.chip_to_probe);
	if (all_matched_count == -1) {
		/* -1 is the ret code which means "something went wrong".
		 * Multiple match and no match are different ret codes.
		 * More details about the error were printed during actual probing. */
		msg_cerr("Error: probing failed.\n");
		ret = 1;
		goto out_shutdown;
	}

	if (all_matched_count > 1) {
		msg_cinfo("Multiple flash chip definitions match the detected chip(s): \"%s\"",
			  context.chip->name);
		for (int ind = 1; ind < all_matched_count; ind++)
			msg_cinfo(", \"%s\"", all_matched_names[ind]);
		msg_cinfo("\nPlease specify which chip definition to use with the -c <chipname> option.\n");
		ret = 1;
		goto out_shutdown;
	} else if (!all_matched_count) {
		msg_cinfo("No EEPROM/flash device found.\n");
		if (!options.force || !options.chip_to_probe) {
			msg_cinfo("Note: flashrom can never write if the flash chip isn't found "
				  "automatically.\n");
		}
		if (options.force && options.read_it && options.chip_to_probe) {
			struct registered_master *mst;
			int compatible_masters = 0;
			msg_cinfo("Force read (-f -r -c) requested, pretending the chip is there:\n");
			/* This loop just counts compatible controllers. */
			for (j = 0; j < registered_master_count; j++) {
				mst = &registered_masters[j];
				/* chip is still set from the chip_to_probe earlier in this function. */
				if (mst->buses_supported & chip->bustype)
					compatible_masters++;
			}
			if (!compatible_masters) {
				msg_cinfo("No compatible controller found for the requested flash chip.\n");
				ret = 1;
				goto out_shutdown;
			}
			if (compatible_masters > 1)
				msg_cinfo("More than one compatible controller found for the requested flash "
					  "chip, using the first one.\n");

			int force_probe_ret = ERROR_FLASHROM_PROBE_NO_CHIPS_FOUND;
			for (j = 0; j < registered_master_count; j++) {
				mst = &registered_masters[j];
				force_probe_ret = probe_flash(mst, 0, &context, 1, options.chip_to_probe);
				if (force_probe_ret >= 0)
					break;
			}
			if (force_probe_ret < 0) {
				// FIXME: This should never happen! Ask for a bug report?
				msg_cinfo("Probing for flash chip '%s' failed.\n", options.chip_to_probe);
				ret = 1;
				goto out_shutdown;
			}
			msg_cinfo("Please note that forced reads most likely contain garbage.\n");
			flashrom_flag_set(&context, FLASHROM_FLAG_FORCE, options.force);
			ret = do_read(&context, options.filename);
			free(context.chip);
			goto out_shutdown;
		}
		ret = 1;
		goto out_shutdown;
	} else if (!options.chip_to_probe) {
		/* repeat for convenience when looking at foreign logs */
		tempstr = flashbuses_to_text(context.chip->bustype);
		msg_gdbg("Found %s flash chip \"%s\" (%d kB, %s).\n",
			 context.chip->vendor, context.chip->name, context.chip->total_size,
			 tempstr ? tempstr : "?");
		free(tempstr);
	}

	struct cli_progress cli_progress = {0};
	if (options.show_progress)
		flashrom_set_progress_callback_v2(&context, &flashrom_progress_cb, &cli_progress);

	print_chip_support_status(context.chip);

	unsigned int limitexceeded = count_max_decode_exceedings(&context, &max_rom_decode);
	if (limitexceeded > 0 && !options.force) {
		enum chipbustype commonbuses = context.mst->buses_supported & context.chip->bustype;

		/* Sometimes chip and programmer have more than one bus in common,
		 * and the limit is not exceeded on all buses. Tell the user. */
		if ((bitcount(commonbuses) > limitexceeded)) {
			msg_pdbg("There is at least one interface available which could support the size of\n"
				 "the selected flash chip.\n");
		}
		msg_cerr("This flash chip is too big for this programmer (--verbose/-V gives details).\n"
			 "Use --force/-f to override at your own risk.\n");
		ret = 1;
		goto out_shutdown;
	}

	const bool any_wp_op =
		options.set_wp_range || options.set_wp_region || options.enable_wp ||
		options.disable_wp || options.print_wp_status || options.print_wp_ranges;

	const bool any_rpmc_op =
#if CONFIG_RPMC_ENABLED == 1
		options.rpmc_read_data || options.rpmc_write_root_key || options.rpmc_update_hmac_key ||
		options.rpmc_increment_counter || options.rpmc_get_counter;
#else
		false;
#endif /* CONFIG_RPMC_ENABLED */

	const bool any_op = options.read_it || options.write_it || options.verify_it ||
		options.erase_it || options.flash_name || options.flash_size ||
		options.extract_it || any_wp_op || any_rpmc_op;

	if (!any_op) {
		msg_ginfo("No operations were specified.\n");
		goto out_shutdown;
	}

	if (options.enable_wp && options.disable_wp) {
		msg_ginfo("Error: --wp-enable and --wp-disable are mutually exclusive\n");
		ret = 1;
		goto out_shutdown;
	}
	if (options.set_wp_range && options.set_wp_region) {
		msg_gerr("Error: Cannot use both --wp-range and --wp-region simultaneously.\n");
		ret = 1;
		goto out_shutdown;
	}

	/*
	 * Common rules for -r/-w/-v syntax parsing:
	 *
	 * - If no filename is specified at all, quit.
	 *
	 * - If a file is specified for -r/-w/-v and no files are specified with
	 *   -i args (or -i is not used), then that file will be used for reading/
	 *   writing/verifying the entire ROM.
	 *
	 * - If no filename is specified for -r/-w/-v, but files are specified
	 *   for -i, then the number of file arguments for -i options must be
	 *   equal to the total number of -i options.
	 *
	 * Rules for reading:
	 *
	 * - If files are specified for -i args but not -r, do partial reads for
	 *   each -i arg, creating a new file for each region. Each -i option
	 *   must specify a filename.
	 *
	 * - If filenames are specified for -r and -i args, then:
	 *     - Do partial read for each -i arg, creating a new file for
	 *       each region where a filename is provided (-i region:filename).
	 *     - Create a ROM-sized file with partially filled content. For each
	 *       -i arg, fill the corresponding offset with content from ROM.
	 *
	 * Rules for writing and verifying:
	 *
	 * - If files are specified for both -w/-v and -i args, -i files take
	 *   priority (files specified for -w/-v are unused).
	 *
	 * - If files are specified for -i args but not -w, do partial writes
	 *   for each -i arg. Likewise for -v and -i args. All -i args must
	 *   supply a filename. Any omission is considered ambiguous.
	 *
	 * - Regions with a filename associated must not overlap. This is also
	 *   considered ambiguous. Note: This is checked later since it requires
	 *   processing the layout/fmap first.
	 */
	if ((options.read_it | options.write_it | options.verify_it) && !options.filename) {
		if (!options.include_args) {
			msg_gerr("Error: No image file specified.\n");
			ret = 1;
			goto out_shutdown;
		}

		if (check_include_args_filename(options.include_args)) {
			ret = 1;
			goto out_shutdown;
		}
	}

	if (options.flash_name) {
		if (context.chip->vendor && context.chip->name) {
			printf("vendor=\"%s\" name=\"%s\"\n",
				context.chip->vendor,
				context.chip->name);
		} else {
			ret = -1;
		}
		goto out_shutdown;
	}

	if (options.flash_size) {
		printf("%zu\n", flashrom_flash_getsize(&context));
		goto out_shutdown;
	}

	if (options.sacrifice_ratio) {
		if (options.sacrifice_ratio < 0 || options.sacrifice_ratio > 50) {
			msg_ginfo("Invalid input of sacrifice ratio, valid 0-50. Fallback to default value 0.\n");
			options.sacrifice_ratio = 0;
		}
		context.sacrifice_ratio = options.sacrifice_ratio;
	}

	if (options.ifd && (flashrom_layout_read_from_ifd(&options.layout, &context, NULL, 0) ||
			   process_include_args(options.layout, options.include_args))) {
		ret = 1;
		goto out_shutdown;
	} else if (options.fmap && options.fmapfile) {
		struct stat s;
		if (stat(options.fmapfile, &s) != 0) {
			msg_gerr("Failed to stat fmapfile \"%s\"\n", options.fmapfile);
			ret = 1;
			goto out_shutdown;
		}

		size_t fmapfile_size = s.st_size;
		uint8_t *fmapfile_buffer = malloc(fmapfile_size);
		if (!fmapfile_buffer) {
			ret = 1;
			goto out_shutdown;
		}

		if (read_buf_from_file(fmapfile_buffer, fmapfile_size, options.fmapfile)) {
			ret = 1;
			free(fmapfile_buffer);
			goto out_shutdown;
		}

		if (flashrom_layout_read_fmap_from_buffer(&options.layout, &context, fmapfile_buffer, fmapfile_size) ||
		    process_include_args(options.layout, options.include_args)) {
			ret = 1;
			free(fmapfile_buffer);
			goto out_shutdown;
		}
		free(fmapfile_buffer);
	} else if (options.fmap) {
		/* Read layout from ROM fmap */
		if (flashrom_layout_read_fmap_from_rom(&options.layout, &context, 0,
				flashrom_flash_getsize(&context)) ||
				process_include_args(options.layout, options.include_args)) {
			ret = 1;
			goto out_shutdown;
		}
		if (options.fmap_verify) {
			struct flashrom_layout *file_layout = NULL;
			struct stat s;
			if (stat(options.filename, &s) != 0) {
				msg_gerr("Failed to stat the file \"%s\"\n", options.filename);
				ret = 1;
				goto out_release;
			}

			size_t fmapfile_size = s.st_size;
			uint8_t *file_buffer = malloc(fmapfile_size);
			if (!file_buffer) {
				ret = 1;
				goto out_release;
			}

			if (read_buf_from_file(file_buffer, fmapfile_size, options.filename)) {
				ret = 1;
				free(file_buffer);
				goto out_release;
			}
			/* Read layout from file fmap */
			if (flashrom_layout_read_fmap_from_buffer(&file_layout, &context, file_buffer,
					flashrom_flash_getsize(&context)) ||
					process_include_args(file_layout, options.include_args)) {
				ret = 1;
				free(file_buffer);
				goto out_release;
			}
			free(file_buffer);
			/* compare the two layouts */
			if (flashrom_layout_compare(options.layout, file_layout)) {
				msg_cerr("FMAP layouts do not match! Aborting.\n");
				flashrom_layout_release(file_layout);
				ret = 1;
				goto out_release;
			}
			flashrom_layout_release(file_layout);
			msg_cinfo("FMAP layouts match.\n");
		}
	}
	flashrom_layout_set(&context, options.layout);

	if (any_wp_op) {
		if (options.set_wp_region && options.wp_region) {
			if (!options.layout) {
				msg_gerr("Error: A flash layout must be specified to use --wp-region.\n");
				ret = 1;
				goto out_release;
			}

			ret = flashrom_layout_get_region_range(options.layout, options.wp_region, &options.wp_start, &options.wp_len);
			if (ret) {
				msg_gerr("Error: Region %s not found in flash layout.\n", options.wp_region);
				goto out_release;
			}
			options.set_wp_range = true;
		}
		ret = wp_cli(
			&context,
			options.enable_wp,
			options.disable_wp,
			options.print_wp_status,
			options.print_wp_ranges,
			options.set_wp_range,
			options.wp_start,
			options.wp_len
		);
		if (ret)
			goto out_release;
	}

	flashrom_flag_set(&context, FLASHROM_FLAG_FORCE, options.force);
#if CONFIG_INTERNAL == 1
	flashrom_flag_set(&context, FLASHROM_FLAG_FORCE_BOARDMISMATCH, force_boardmismatch);
#endif
	flashrom_flag_set(&context, FLASHROM_FLAG_VERIFY_AFTER_WRITE, !options.dont_verify_it);
	flashrom_flag_set(&context, FLASHROM_FLAG_VERIFY_WHOLE_CHIP, !options.dont_verify_all);

	/* FIXME: We should issue an unconditional chip reset here. This can be
	 * done once we have a .reset function in struct flashchip.
	 * Give the chip time to settle.
	 */
	programmer_delay(&context, 100000);
	if (options.read_it)
		ret = do_read(&context, options.filename);
	else if (options.extract_it)
		ret = do_extract(&context);
	else if (options.erase_it) {
		ret = flashrom_flash_erase(&context);
	}
	else if (options.write_it)
		ret = do_write(&context, options.filename, options.referencefile);
	else if (options.verify_it)
		ret = do_verify(&context, options.filename);

#if CONFIG_RPMC_ENABLED == 1
	if (any_rpmc_op && ret == 0) {
		ret = rpmc_cli(&context,
			       options.rpmc_root_key_file,
			       options.rpmc_key_data,
			       options.rpmc_counter_address,
			       options.rpmc_previous_counter_value,
			       options.rpmc_read_data,
			       options.rpmc_write_root_key,
			       options.rpmc_update_hmac_key,
			       options.rpmc_increment_counter,
			       options.rpmc_get_counter);
	}
#endif /* CONFIG_RPMC_ENABLED */

out_release:
	flashrom_layout_release(options.layout);
out_shutdown:
	flashrom_programmer_shutdown(NULL);
out:
	flashrom_layout_release(context.default_layout);
	free(context.chip);
	flashrom_data_free(all_matched_names);

	free_options(&options);

	time(&time_end);
	msg_gdbg("Runtime from programmer init to shutdown: %dmin%2dsec\n",
		(int)(difftime(time_end, time_start) / 60), (int)(difftime(time_end, time_start)) % 60);

	ret |= close_logfile();
	return ret;
}
