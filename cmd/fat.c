// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2002
 * Richard Jones, rjones@nexus-tech.net
 */

/*
 * Boot support
 */
#include <common.h>
#include <command.h>
#include <mapmem.h>
#include <fat.h>
#include <fs.h>
#include <part.h>
#include <asm/cache.h>
#include <stdlib.h>
#include <env.h>
#include <ext4fs.h>

int do_fat_size(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	return do_size(cmdtp, flag, argc, argv, FS_TYPE_FAT);
}

U_BOOT_CMD(
	fatsize,	4,	0,	do_fat_size,
	"determine a file's size",
	"<interface> <dev[:part]> <filename>\n"
	"    - Find file 'filename' from 'dev' on 'interface'\n"
	"      and determine its size."
);

int do_fat_fsload(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	return do_load(cmdtp, flag, argc, argv, FS_TYPE_FAT);
}


U_BOOT_CMD(
	fatload,	7,	0,	do_fat_fsload,
	"load binary file from a dos filesystem",
	"<interface> [<dev[:part]> [<addr> [<filename> [bytes [pos]]]]]\n"
	"    - Load binary file 'filename' from 'dev' on 'interface'\n"
	"      to address 'addr' from dos filesystem.\n"
	"      'pos' gives the file position to start loading from.\n"
	"      If 'pos' is omitted, 0 is used. 'pos' requires 'bytes'.\n"
	"      'bytes' gives the size to load. If 'bytes' is 0 or omitted,\n"
	"      the load stops on end of file.\n"
	"      If either 'pos' or 'bytes' are not aligned to\n"
	"      ARCH_DMA_MINALIGN then a misaligned buffer warning will\n"
	"      be printed and performance will suffer for the load."
);

static int do_fat_ls(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
	return do_ls(cmdtp, flag, argc, argv, FS_TYPE_FAT);
}

U_BOOT_CMD(
	fatls,	4,	1,	do_fat_ls,
	"list files in a directory (default /)",
	"<interface> [<dev[:part]>] [directory]\n"
	"    - list files from 'dev' on 'interface' in a 'directory'"
);

#define REBOOT_MAX_ALLOWED 3

static int do_fat_mailbox(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
    char read_str[10];
	char data_str[10];
	unsigned int temp_u;
    unsigned int boot_flag;
	unsigned int reboot_counter_1;
	unsigned int reboot_counter_2;
	unsigned int boot_os_id;
	loff_t size;
    int ret,dev,part;
	struct blk_desc *dev_desc;
	struct disk_partition info;
	unsigned int boot_counter;
	unsigned int corrupted_part;
	char log_fn[20];

    //print custom u-boot version
    printf("\n\n Elynxo U-Boot Version: C22-1925-AA-002_1.4.1\n");

	//update boot counter
	part = blk_get_device_part_str("mmc", "2:a", &dev_desc, &info, 1); //part10 of emmc (FactoryData)
	if (part < 0) return 1;
	dev = dev_desc->devnum;
	if (fat_set_blk_dev(dev_desc, &info) != 0)
	{
		printf("\n Unable to use %s %d:%d\n","mmc", dev, part);
		return 1;
	}

	if (file_fat_read("boot_counter", read_str, sizeof(read_str)) < 0)
	{
		//counter doesn't exist, first use, create it and set to 0
		boot_counter = 1;
		if (file_fat_write("boot_counter", (void*)"000001", 0,1, &size) != 0)
		{	
			printf(" Failed to create file boot_counter\n");
		}
	}
	else
	{
		//counter exists, increment counter
		boot_counter = simple_strtoul(read_str, NULL, 10);
		boot_counter++;
		snprintf(data_str, sizeof(data_str), "%06d", boot_counter);
		if (file_fat_write("boot_counter", (void*)data_str, 0,strlen(data_str), &size) != 0)
		{	
			printf(" Failed to write file boot_counter\n");
		}
	}


	//get mailbox partition as blk dev (mmc dev:part)
	part = blk_get_device_part_str(argv[1], argv[2], &dev_desc, &info, 1);
	if (part < 0)
		return 1;

	dev = dev_desc->devnum;
	if (fat_set_blk_dev(dev_desc, &info) != 0) {
		printf("\n Unable to use %s %d:%d\n",
			argv[1], dev, part);
		return 1;
	}


	//read boot flag
    ret = file_fat_read("boot_flag.txt", read_str, sizeof(read_str));
    if (ret >= 0) {
        boot_flag = simple_strtoul(read_str, NULL, 10);
		printf(" boot flag: %u\n", boot_flag);
		if ( (boot_flag != 1) && (boot_flag != 2) )
		{
			printf(" Invalid boot flag value! %d\n\n", boot_flag);
		}
    }
	else
	{
        printf(" Failed to read boot flag\n"); //first run (os tools)
        boot_flag = 1;
    }

	//read last reboot counter
	ret = file_fat_read("reboot_counter.txt", read_str, sizeof(read_str));
	if (ret >= 0) {
		temp_u = simple_strtoul(read_str, NULL, 16);		
		reboot_counter_1 = temp_u & 0xFF; //first byte
		reboot_counter_2 = (temp_u >> 8) & 0xFF; //second byte
	}
	else
	{
		printf(" Failed to read boot counter flag\n");
		reboot_counter_1 = 0;
		reboot_counter_2 = 0;
	}

	//set reboot counter (check for possible corruption)
	ret = file_fat_read("healthy_os", read_str, sizeof(read_str));
	if (ret < 0) //file not existing
	{
		printf(" Failed to read health flag\n");
		if (boot_flag == 1)
		{
			reboot_counter_1++;
			reboot_counter_1 &= 0xFF;
		}
		else if (boot_flag == 2)
		{
			reboot_counter_2++;
			reboot_counter_2 &= 0xFF;
		}		
	}
	else //OS re-started normally, reset error
	{
		printf(" health flag read ok\n");
		if (boot_flag == 1)
		{
			reboot_counter_1 = 0;
		}
		else if (boot_flag == 2)
		{
			reboot_counter_2 = 0;
		}

		//delete file for next reboot check
		int result = fat_unlink("healthy_os");
		if (result < 0)
		{
			printf(" Failed to delete file healthy_os\n");
		}
	}

	//save reboot counter
	temp_u = reboot_counter_1 + (reboot_counter_2 << 8);
	snprintf(data_str, sizeof(data_str), "%04x", temp_u);
	if (file_fat_write("reboot_counter.txt", (void*)data_str, 0,strlen(data_str), &size) != 0)
	{
		printf(" Failed to write file reboot_counter.txt\n");
	}

	//select boot device
	if ( (reboot_counter_1 < REBOOT_MAX_ALLOWED) && (reboot_counter_2 < REBOOT_MAX_ALLOWED) )
	{
		//no corruption
		printf(" No corruption detected on EMMC\n");
		corrupted_part = 0;
		boot_os_id = boot_flag;
	}
	else if (reboot_counter_1 < REBOOT_MAX_ALLOWED) //corruption only on OS 2
	{
		printf(" OS-2 corrupted\n");
		corrupted_part = 2;
		boot_os_id = 1;
	}
	else if (reboot_counter_2 < REBOOT_MAX_ALLOWED) //corruption only on OS 1
	{
		printf(" OS-1 corrupted\n");
		corrupted_part = 1;
		boot_os_id = 2;
	}
	else //all of both corrupted
	{
		printf(" All EMMC RootFS corrupted\n");
		corrupted_part = 3;
		boot_os_id = 3; //will boot from SD
	}

	//save selected boot device
	snprintf(data_str, sizeof(data_str), "%d", boot_os_id);
	if (file_fat_write("boot_flag.txt", (void*)data_str, 0,strlen(data_str), &size) != 0)
	{
		printf(" Failed to write file boot_flag.txt\n");
	}


	//Log if there is corruption, on SD card
	dev_desc = blk_get_dev("mmc", 1);
	if (dev_desc && (dev_desc->type != DEV_TYPE_UNKNOWN))
	{
		//SD card present
		if (part_get_info(dev_desc, 1, &info) == 0) //check for partition 1 found
		{
			if (fat_set_blk_dev(dev_desc, &info) == 0)
			{
				snprintf(log_fn, sizeof(log_fn), "BootOS_%06d", boot_counter);
				switch(corrupted_part)
				{
					case 1:
					{
						const char *mess1 = "OS1 is corrupted, switched to boot on OS2";
						if (file_fat_write(log_fn, (void*)mess1, 0,strlen(mess1), &size) != 0)
						{
							printf(" Failed to write %s\n", log_fn);
						}
					}
					break;

					case 2:
					{
						const char *mess2 = "OS2 is corrupted, switched to boot on OS1";
						if (file_fat_write(log_fn, (void*)mess2, 0,strlen(mess2), &size) != 0)
						{
							printf(" Failed to write %s\n", log_fn);
						}
					}
					break;

					case 3:
					{
						const char *mess3 = "OS corruptions, waiting for an Elynxo SD Flasher to repair";
						if (file_fat_write(log_fn, (void*)mess3, 0,strlen(mess3), &size) != 0)
						{
							printf(" Failed to write %s\n", log_fn);
						}
					}
					break;

					default:
					break;
				}
			}
			else
			{
				printf(" Unable to use partition 1 on SD\n");
			}
		}
		else
		{
			printf(" No partition 1 on SD\n");
		}
	}
	else
	{
		printf(" No SD card\n");
	}



	//set boot device partitions (dev, part, root)
	if (boot_os_id == 1)
	{
		env_set("mmcdev", "2"); //emmc OS1
		env_set("mmcpart", "1");
		env_set("mmcroot", "2");
		printf(" Booting Application1 \n");
    }
	else if (boot_os_id == 2)
	{
		env_set("mmcdev", "2"); //emmc OS2
		env_set("mmcpart", "3");
		env_set("mmcroot", "5");
		printf(" Booting Application2 \n");
	}
	else if (boot_os_id == 3)
	{
		//get rootFS from SD card (mmc 1:2)
		// and check inside "var/os-version" file: "elynxo_FLASHER_C22-7932-"
		part = blk_get_device_part_str("mmc", "1:2", &dev_desc, &info, 1);
		if (part < 0)
		{
			printf(" No bootable SD card inserted\n");
			goto fail;
			return 1;
		}
		dev = dev_desc->devnum;
		ext4fs_set_blk_dev(dev_desc, &info);
		if (!ext4fs_mount(info.size))
		{
			printf("\n Unable to mount mmc 1:2 \n");
			goto fail;
			return 1;

		}
		
		char f_contents[200];
		loff_t off;
		ret = ext4_read_file("var/os-version", f_contents, 0, sizeof(f_contents), &off);
		f_contents[199] = '\0';
		ext4fs_close();

		if (ret >= 0)
		{
			//check content
			char *p = strstr(f_contents, "elynxo_FLASHER_C22-7932-");
			if(p) //signature found
			{
				env_set("mmcdev", "1"); //SD card
				env_set("mmcpart", "1");
				env_set("mmcroot", "2");
				printf(" Booting from SD card \n");
			}
			else
			{
				printf("\n Wrong SD card inserted\n");
				goto fail;
				return 1;
			}
		}
		else
		{
			printf("\n Can not check SD card contents\n");		
			goto fail;
			return 1;
		}
	}


	//save
	run_command("saveenv", 0);

	printf("\n");
    return 0;

	//don't boot if failure in case of corruption
	fail:
		env_set("mmcdev", "");
		env_set("mmcpart", "");
		env_set("mmcroot", "");
		return 1;

}

U_BOOT_CMD(
	fatmailbox,	3,	1,	do_fat_mailbox,
	"check infos in mailbox partition and boot accordingly",
	"<interface> [<dev[:part]>] \n"
	"    - reads flag from 'dev' on 'interface' mailbox"
);


static int do_fat_fsinfo(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	int dev, part;
	struct blk_desc *dev_desc;
	struct disk_partition info;

	if (argc < 2) {
		printf("usage: fatinfo <interface> [<dev[:part]>]\n");
		return 0;
	}

	part = blk_get_device_part_str(argv[1], argv[2], &dev_desc, &info, 1);
	if (part < 0)
		return 1;

	dev = dev_desc->devnum;
	if (fat_set_blk_dev(dev_desc, &info) != 0) {
		printf("\n** Unable to use %s %d:%d for fatinfo **\n",
			argv[1], dev, part);
		return 1;
	}
	return file_fat_detectfs();
}

U_BOOT_CMD(
	fatinfo,	3,	1,	do_fat_fsinfo,
	"print information about filesystem",
	"<interface> [<dev[:part]>]\n"
	"    - print information about filesystem from 'dev' on 'interface'"
);

#ifdef CONFIG_FAT_WRITE
static int do_fat_fswrite(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	return do_save(cmdtp, flag, argc, argv, FS_TYPE_FAT);
}

U_BOOT_CMD(
	fatwrite,	7,	0,	do_fat_fswrite,
	"write file into a dos filesystem",
	"<interface> <dev[:part]> <addr> <filename> [<bytes> [<offset>]]\n"
	"    - write file 'filename' from the address 'addr' in RAM\n"
	"      to 'dev' on 'interface'"
);

static int do_fat_rm(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
	return do_rm(cmdtp, flag, argc, argv, FS_TYPE_FAT);
}

U_BOOT_CMD(
	fatrm,	4,	1,	do_fat_rm,
	"delete a file",
	"<interface> [<dev[:part]>] <filename>\n"
	"    - delete a file from 'dev' on 'interface'"
);

static int do_fat_mkdir(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	return do_mkdir(cmdtp, flag, argc, argv, FS_TYPE_FAT);
}

U_BOOT_CMD(
	fatmkdir,	4,	1,	do_fat_mkdir,
	"create a directory",
	"<interface> [<dev[:part]>] <directory>\n"
	"    - create a directory in 'dev' on 'interface'"
);
#endif
