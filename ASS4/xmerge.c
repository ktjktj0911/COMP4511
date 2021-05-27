/* comp4511/xmerge.c */
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/segment.h>
// Include necessary header files

struct xmerge_params {
	char __user *outfile;
	char __user **infiles;
	unsigned int num_files;
	int oflags;
	mode_t mode;
	int __user *ofile_count;
};

// You should use SYSCALL_DEFINEn macro
SYSCALL_DEFINE2(xmerge, void *, args, size_t, argslen) // Fill in the arguments
{	
	printk(KERN_ERR "Syscall xmerge");
	char buffer[PAGE_SIZE];
	struct xmerge_params xp;
	char* infilename;
	int infd, outfd;
	int filecount = 0;
	int bytecount = 0;

	mm_segment_t old_fs;
	old_fs = get_fs();
	set_fs(get_ds());

	if (copy_from_user(&xp, (struct xmerge_param*)args, argslen) != 0) {
		printk(KERN_ERR "copy xmerge_params error\n");
		set_fs(old_fs);
		return -EFAULT;
	}
	
	if ((outfd = ksys_open(xp.outfile, xp.oflags, xp.mode)) < 0) {
		printk(KERN_ERR "outfile error\n");
		set_fs(old_fs);
		return outfd;
	}
	
	int x;
	for ( x = 0; x < xp.num_files; x++) {
		if (copy_from_user(&infilename, xp.infiles + x, sizeof(char*)) != 0) {
			printk(KERN_ERR "copy infile error\n");
			ksys_close(outfd);
			set_fs(old_fs);
			return -EFAULT;
		}
		printk(KERN_ERR "infile = %s", infilename);
		
		if ((infd = ksys_open(infilename, O_RDONLY, 0)) < 0) {
			printk(KERN_ERR "infile %d error", x);
			ksys_close(outfd);
			set_fs(old_fs);
			return infd;
		}

		int read;
		int err;
		while ((read = ksys_read(infd, buffer, PAGE_SIZE)) != 0) {
			if (read < 0) {
				printk(KERN_ERR "read error");
				ksys_close(outfd);
				ksys_close(infd);
				set_fs(old_fs);
				return read;
			}
			bytecount += (long)read;
			if ((err = ksys_write(outfd, buffer, read)) < 0) {
				printk(KERN_ERR "write error");
				ksys_close(outfd);
				ksys_close(infd);
				set_fs(old_fs);
				return err;
			}
		}
		
		filecount++;
		if ((err = copy_to_user(xp.ofile_count, &filecount, sizeof(filecount))) < 0) {
			printk(KERN_ERR "filecount %d", filecount);
			ksys_close(outfd);
			ksys_close(infd);
			set_fs(old_fs);
			return -EFAULT;
		}
		ksys_close(infd);
	}

	ksys_close(outfd);
	set_fs(old_fs);
	return bytecount;
}
