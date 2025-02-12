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

static int do_fat_mailbox(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
    char flag_str[16];
    unsigned int boot_flag;
    int ret,dev,part;
    struct disk_partition mailbox_part;
	struct blk_desc *dev_desc;
	struct disk_partition info;
    char *part_name = "mailbox"; // Update with the actual partition label
///////////
       printf("\n**************************************************\n");
       printf("\n*                     ELYNXO                     *\n");
       printf("\n**************************************************\n");
       printf("\n*     UBOOT VERSION : C22-1925-AA-001_1.0.0      *\n");
       printf("\n**************************************************\n");
//////////
	part = blk_get_device_part_str(argv[1], argv[2], &dev_desc, &info, 1);
	if (part < 0)
		return 1;

	dev = dev_desc->devnum;
	if (fat_set_blk_dev(dev_desc, &info) != 0) {
		printf("\n** Unable to use %s %d:%d for fatinfo **\n",
			argv[1], dev, part);
		return 1;
	}
    ret = file_fat_read("boot_flag.txt", flag_str, sizeof(flag_str));
    if (ret >= 0) {
        boot_flag = simple_strtoul(flag_str, NULL, 10);
        ret = 0;
	printf(" boot flag is : %lu\n", boot_flag);

        if (boot_flag == 1) {
            env_set("mmcdev", "2");
            env_set("mmcpart", "1");
            env_set("mmcroot", "2");
	    //run_command("setenv mmcpart 1", 0);
	    //run_command("setenv mmcroot 2;",0);
	    printf("Booting Application1 ");
        } else if (boot_flag == 2) {
            //setenv("bootcmd", "run boot_b");
            env_set("mmcdev", "2");    // boot from mmc disk amovible.
            env_set("mmcpart", "3");
            env_set("mmcroot", "5");
	    //run_command("setenv mmcpart 3", 0);
	    //run_command("setenv mmcroot 5;",0);
	    printf("Booting Application2 ");
        } else {
            printf("Invalid boot flag value! Booting default.\n");
            env_set("mmcdev", "2");
            env_set("mmcpart", "1");
            env_set("mmcroot", "2");
	    //run_command("setenv mmcpart 1", 0);
	    //run_command("setenv mmcroot 2;",0);
	    printf("Booting Application1 ");
        }

    } else {
        printf("Failed to read boot flag from mailbox partition\n");
	ret = 0;
            env_set("mmcdev", "2");
            env_set("mmcpart", "1");
            env_set("mmcroot", "2");
	    //run_command("setenv mmcpart 1", 0);
	    //run_command("setenv mmcroot 2;",0);
	    printf("Booting Application1 ");
    }


	run_command("saveenv", 0);
//	run_command("run bootcmd", 0);

    return ret;
}

U_BOOT_CMD(
	fatmailbox,	3,	1,	do_fat_mailbox,
	"list files in a directory (default /)",
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
