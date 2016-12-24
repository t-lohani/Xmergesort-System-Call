#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <stdbool.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/string.h>
#include <linux/path.h>
#include <linux/namei.h>
#include "myinput.h"

asmlinkage extern long (*sysptr)(void *arg);

struct inputstruct *input;
struct inputstruct *kargs;

int write_file(struct file *filp, char *buf, unsigned int len, unsigned long long offset)
{
	mm_segment_t oldfs;
	int errval = 0;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	errval = vfs_write(filp, buf, len, &offset);
	set_fs(oldfs);

	kfree(buf);
	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);

	return errval;
}


int read_file(struct file *filp, char *buf, unsigned int len, unsigned long long offset)
{
	mm_segment_t oldfs;
	int errval = 0;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	errval = vfs_read(filp, buf, len, &offset);
	set_fs(oldfs);

    return errval;
}

int compare_strs(char *buffer1, int end1, char *buffer2, int end2, bool ignore_case)
{
	int counter = 0;
	int val1, val2;

	while (counter < end1 && counter < end2) {
		if (*(buffer1+counter) < *(buffer2+counter)) {
			val1 = (int) *(buffer1+counter);
			val2 = ((int) *(buffer2+counter)) - 32;
			if (ignore_case && val1 == val2) {
				/*printk("Avoiding mismatch ignoring case. Character : %c\n", val1);*/
			} else {
				return 1;
			}
		} else if (*(buffer1+counter) > *(buffer2+counter)) {
			val1 = ((int) *(buffer1+counter)) - 32;
			val2 = (int) *(buffer2+counter);
			if (ignore_case && val1 == val2) {
				/*printk("Avoiding mismatch ignoring case. Character : %c\n", val1);*/
			} else {
				return -1;
			}
		}
		counter++;
	}

	if (end1 == counter && end2 == counter) {
		return 0;
	} else if (end1 == counter) {
		return 1;
	} else if (end2 == counter) {
		return -1;
	}
	return 0;
}


long mergefiles (char *buf1, char *buf2, char *bufOut, bool uflg, bool aflg, bool iflg, bool tflg, bool dflg)
{
	struct path path;
	int filesize1, filesize2, filetracker1, filetracker2;
	long errval = 0;
	int compare_res;
	int data_count = 0;

	char *trackbufout = bufOut;
	char *lastreadstr = kmalloc(PAGE_SIZE, GFP_KERNEL);
	char *lasttracker = lastreadstr;
	int lastlinetracker = 0;
	int complast_res;

	bool write_data = false;
	int bufcharcount1 = 0, bufcharcount2 = 0, bufcharcountout = 0;
	int offset1 = 0, offset2 = 0, offsetout = 0;
	int linetracker1, linetracker2, buftracker1, buftracker2;
	struct inode *inode1, *inode2;
	struct file *file_in1 = NULL, *file_in2 = NULL, *file_out = NULL;

	kern_path(kargs->inFile1, LOOKUP_FOLLOW, &path);
	inode1 = path.dentry->d_inode;
	filesize1 = inode1->i_size;

	kern_path(kargs->inFile2, LOOKUP_FOLLOW, &path);
	inode2 = path.dentry->d_inode;
	filesize2 = inode2->i_size;

	if (inode1 == inode2) {
		printk(KERN_ERR "Files are same. Return error");
		errval = -EINVAL;
		goto exit;
	}

	file_in1 = filp_open(kargs->inFile1, O_RDONLY, 0);

	if (!file_in1) {
		printk(KERN_ERR "File does not Exist/Bad File.\n");
		errval = -EBADF;
		goto exit;
	}

	if (IS_ERR(file_in1)) {
		printk(KERN_ERR "File error\n");
		errval = PTR_ERR(file_in1);
		goto exit;
	}

	if (!(file_in1->f_mode & FMODE_READ)) {
		printk(KERN_ERR "Read permissions not present\n");
		errval = -EIO;
		goto exit;
	}

	file_in2 = filp_open(kargs->inFile2, O_RDONLY, 0);

	if (!file_in2) {
		printk(KERN_ERR "File does not Exist/Bad File.\n");
		errval = -EBADF;
		goto exit;
	}

	if (IS_ERR(file_in2)) {
		printk(KERN_ERR "File error\n");
		errval = PTR_ERR(file_in2);
		goto exit;
	}

	if (!(file_in2->f_mode & FMODE_READ)) {
		printk(KERN_ERR "Read permissions not present\n");
		errval = -EIO;
		goto exit;
	}

	file_out = filp_open(kargs->outFile, O_WRONLY|O_CREAT, 0);

	if (!file_out) {
		printk(KERN_ERR "File does not Exist/Bad File.\n");
		errval = -EBADF;
		goto exit;
	}

	if (IS_ERR(file_out)) {
		printk(KERN_ERR "File error\n");
		errval = PTR_ERR(file_out);
		goto exit;
	}

	if (!(file_out->f_mode & FMODE_WRITE)) {
		printk(KERN_ERR "Write permissions not present\n");
		errval = -EIO;
		goto exit;
	}

	if (filesize1 > 0 && filesize2 > 0) {
		bufcharcount1 = read_file(file_in1, buf1, PAGE_SIZE, offset1);
		bufcharcount2 = read_file(file_in2, buf2, PAGE_SIZE, offset2);

		if (bufcharcount1 < 0) {
			errval = bufcharcount1;
			goto exit;
		}

		if (bufcharcount2 < 0) {
			errval = bufcharcount2;
			goto exit;
		}

		filetracker1 = filesize1;
		filetracker2 = filesize2;

		while (bufcharcount1 > 0 && bufcharcount2 > 0) {
			linetracker1 = 0;
			linetracker2 = 0;

			buftracker1 = bufcharcount1;
			buftracker2 = bufcharcount2;

			while (*(buf1+linetracker1) != '\n' && buftracker1 > 0) {
				linetracker1++;
				filetracker1--;
				buftracker1--;
			}

			while (*(buf2+linetracker2) != '\n' && buftracker2 > 0) {
				linetracker2++;
				filetracker2--;
				buftracker2--;
			}

			if (buftracker1 == 0 || buftracker2 == 0) {
				if (buftracker1 == 0) {
					if (filetracker1 != 0)	{
						bufcharcount1 = read_file(file_in1, buf1, PAGE_SIZE, offset1);

						if (bufcharcount1 < 0)	{
							errval = bufcharcount1;
							goto exit;
						}
					}
				}
				if (buftracker2 == 0) {
					if (filetracker2 != 0) {
						bufcharcount2 = read_file(file_in2, buf2, PAGE_SIZE, offset2);

						if (bufcharcount2 < 0) {
							errval = bufcharcount2;
							goto exit;
						}
					}
				}
			} else {
				compare_res = compare_strs(buf1, linetracker1, buf2, linetracker2, iflg);

				if (compare_res == 0) {

					if (uflg && aflg) {
						printk(KERN_ERR "Duplicate found. u and a can not come together.\n");
						errval = -EINVAL;
						goto exit;
					}

					bufcharcount1 = buftracker1;
					bufcharcount2 = buftracker2;

					if (lasttracker != lastreadstr) {
						complast_res = compare_strs(buf1, linetracker1, lastreadstr, lastlinetracker, iflg);
						lasttracker = lastreadstr;
						lastlinetracker = 0;
					}

/*					if (complast_res == 1) {
						if (tflg) {
							write_data = false;
						} else {
							errval = -EINVAL;
							goto exit;
						}
					} else if (complast_res == 0) {
						if (uflg && aflg) {
							printk(KERN_ERR "Duplicate found. u and a can not come together.\n");
							errval = -EINVAL;
							goto exit;
						} else if (uflg) {
							write_data = false;
						} else {
							write_data = true;
						}
					}
*/
					while (linetracker1 > 0) {
						if (bufcharcountout <= PAGE_SIZE) {
//							if (write_data) {
								*lasttracker++ = *buf1;
								lastlinetracker++;
								*trackbufout++ = *buf1++;
								bufcharcountout++;
//							} else {
//								buf1++;
//							}
							linetracker1--;
							filesize1--;
							offset1++;
						} else {
							data_count = data_count + write_file(file_out, bufOut, bufcharcountout, offsetout);
							offsetout = offsetout + PAGE_SIZE;
							trackbufout = bufOut;
							bufcharcountout = 0;
						}
					}
//					if (write_data) {
						*lasttracker++ = '\n';
						lastlinetracker++;
						*trackbufout++ = '\n';
						bufcharcountout++;
//					}
					buf1++;
					bufcharcount1--;
					offset1++;
					filesize1--;

					lastlinetracker = 0;

					if (!uflg) {
						while (linetracker2 > 0) {
							if (bufcharcountout <= PAGE_SIZE) {
								*lasttracker++ = *buf2;
								lastlinetracker++;
								*trackbufout++ = *buf2++;
								linetracker2--;
								offset2++;
								filesize2--;
								bufcharcountout++;
							} else {
								data_count = data_count + write_file(file_out, bufOut, bufcharcountout, offsetout);
								offsetout = offsetout + PAGE_SIZE;
								trackbufout = bufOut;
								bufcharcountout = 0;
							}
						}
						*lasttracker++ = '\n';
						lastlinetracker++;
						*trackbufout++ = '\n';
						bufcharcountout++;
						buf2++;
						bufcharcount2--;
						offset2++;
						filesize2--;
					} else {
						while (linetracker2 > 0) {
							buf2++;
							linetracker2--;
							offset2++;
							filesize2--;
						}
						buf2++;
						bufcharcount2--;
						offset2++;
						filesize2--;
					}
				} else if (compare_res == 1) {
					bufcharcount1 = buftracker1;

					if (lasttracker != lastreadstr) {
						complast_res = compare_strs(buf1, linetracker1, lastreadstr, lastlinetracker, iflg);
						lasttracker = lastreadstr;
						lastlinetracker = 0;
					}

/*					if (complast_res == 1) {
						if (tflg) {
							write_data = false;
						} else {
							errval = -EINVAL;
							goto exit;
						}
					} else if (complast_res == 0) {
						if (uflg && aflg) {
							printk(KERN_ERR "Duplicate found. u and a can not come together.\n");
							errval = -EINVAL;
							goto exit;
						} else if (uflg) {
							write_data = false;
						} else {
							write_data = true;
						}
					}
*/
					while (linetracker1 > 0) {
						if (bufcharcountout <= PAGE_SIZE) {
//							if (write_data) {
								*lasttracker++ = *buf1;
								lastlinetracker++;
								*trackbufout++ = *buf1++;
								bufcharcountout++;
//							} else {
//								buf1++;
//							}
							linetracker1--;
							offset1++;
							filesize1--;
						} else {
							data_count = data_count + write_file(file_out, bufOut, bufcharcountout, offsetout);
							offsetout = offsetout + PAGE_SIZE;
							trackbufout = bufOut;
							bufcharcountout = 0;
						}
					}
//					if (write_data) {
						*lasttracker++ = '\n';
						lastlinetracker++;
						*trackbufout++ = '\n';
						bufcharcountout++;
//					}
					buf1++;
					bufcharcount1--;
					offset1++;
					filesize1--;
				} else if (compare_res == -1) {
					bufcharcount2 = buftracker2;

					if (lasttracker != lastreadstr) {
						complast_res = compare_strs(buf2, linetracker2, lastreadstr, lastlinetracker, iflg);
						lasttracker = lastreadstr;
						lastlinetracker = 0;
					}
/*
					if (complast_res == 1) {
						if (tflg) {
							write_data = false;
						} else {
							errval = -EINVAL;
							goto exit;
						}
					} else if (complast_res == 0) {
						if (uflg && aflg) {
							printk(KERN_ERR "Duplicate found. u and a can not come together.\n");
							errval = -EINVAL;
							goto exit;
						} else if (uflg) {
							write_data = false;
						} else {
							write_data = true;
						}
					}
*/
					while (linetracker2 > 0) {
						if (bufcharcountout <= PAGE_SIZE) {
//							if (write_data) {
								*lasttracker++ = *buf2;
								lastlinetracker++;
								*trackbufout++ = *buf2++;
								bufcharcountout++;
//							} else {
//								buf2++;
//							}
							linetracker2--;
							offset2++;
							filesize2--;
						} else {
							data_count = data_count + write_file(file_out, bufOut, bufcharcountout, offsetout);
							offsetout = offsetout + PAGE_SIZE;
							trackbufout = bufOut;
							bufcharcountout = 0;
						}
					}
//					if (write_data) {
						*lasttracker++ = '\n';
						lastlinetracker++;
						*trackbufout++ = '\n';
						bufcharcountout++;
//					}
					buf2++;
					bufcharcount2--;
					offset2++;
					filesize2--;
				}
			}
		}
	}

	if (filesize1 > 0) {
		filetracker1 = filesize1;

		while (bufcharcount1 > 0) {
			linetracker1 = 0;

			buftracker1 = bufcharcount1;

			while (*(buf1+linetracker1) != '\n' && buftracker1 > 0) {
				linetracker1++;
				filetracker1--;
				buftracker1--;
			}

			if (buftracker1 == 0) {
				if (filetracker1 != 0) {
					bufcharcount1 = read_file(file_in1, buf1, PAGE_SIZE, offset1);
					if (bufcharcount1 < 0) {
						errval = bufcharcount1;
						goto exit;
					}
				}
			} else {
				bufcharcount1 = buftracker1;

				if (lasttracker != lastreadstr) {
					complast_res = compare_strs(buf1, linetracker1, lastreadstr, lastlinetracker, iflg);
					lasttracker = lastreadstr;
					lastlinetracker = 0;
				}

/*				if (complast_res == 1) {
					if (tflg) {
						write_data = false;
					} else {
						errval = -EINVAL;
						goto exit;
					}
				} else if (complast_res == 0) {
					if (uflg && aflg) {
					printk(KERN_ERR "Duplicate found. u and a can not come together.\n");
					errval = -EINVAL;
					goto exit;
				} else if (uflg) {
					write_data = false;
				} else {
					write_data = true;
				}
			}
*/
				while (linetracker1 > 0) {
					if (bufcharcountout <= PAGE_SIZE) {
//						if (write_data) {
							*lasttracker++ = *buf1;
							lastlinetracker++;
							*trackbufout++ = *buf1++;
							bufcharcountout++;
//						} else {
//							buf1++;
//						}
						linetracker1--;
						offset1++;
						filesize1--;
					} else {
						data_count = data_count + write_file(file_out, bufOut, bufcharcountout, offsetout);
						offsetout = offsetout + PAGE_SIZE;
						trackbufout = bufOut;
						bufcharcountout = 0;
					}
				}
//				if (write_data) {
					*lasttracker++ = '\n';
					lastlinetracker++;
					*trackbufout++ = '\n';
					bufcharcountout++;
//				}
				buf1++;
				bufcharcount1--;
				offset1++;
				filesize1--;
			}
		}

	} else if (filesize2 > 0) {

		filetracker2 = filesize2;

		while (bufcharcount2 > 0) {

			linetracker2 = 0;
			buftracker2 = bufcharcount2;

			while (*(buf2+linetracker2) != '\n' && buftracker2 > 0) {
				linetracker2++;
				filetracker2--;
				buftracker2--;
			}

			if (buftracker2 == 0) {
				if (filetracker2 != 0) {
					bufcharcount2 = read_file(file_in2, buf2, PAGE_SIZE, offset2);
					if (bufcharcount2 < 0) {
						errval = bufcharcount2;
						goto exit;
					}
				}
			} else {
				bufcharcount2 = buftracker2;

				if (lasttracker != lastreadstr) {
					complast_res = compare_strs(buf2, linetracker2, lastreadstr, lastlinetracker, iflg);
					lasttracker = lastreadstr;
					lastlinetracker = 0;
				}

/*				if (complast_res == 1) {
					if (tflg) {
						write_data = false;
					} else {
						errval = -EINVAL;
						goto exit;
					}
				} else if (complast_res == 0) {
					if (uflg && aflg) {
						printk(KERN_ERR "Duplicate found. u and a can not come together.\n");
						errval = -EINVAL;
						goto exit;
					} else if (uflg) {
						write_data = false;
					} else {
						write_data = true;
					}
				}
*/
				while (linetracker2 > 0) {
					if (bufcharcountout <= PAGE_SIZE) {
//						if (write_data) {
							*lasttracker++ = *buf2;
							lastlinetracker++;
							*trackbufout++ = *buf2++;
							bufcharcountout++;
//						} else {
//							buf2++;
//						}
						linetracker2--;
						offset2++;
						filesize2--;
					} else {
						data_count = data_count + write_file(file_out, bufOut, bufcharcountout, offsetout);
						offsetout = offsetout + PAGE_SIZE;
						trackbufout = bufOut;
						bufcharcountout = 0;
					}
				}
//				if (write_data) {
					*lasttracker++ = '\n';
					lastlinetracker++;
					*trackbufout++ = '\n';
					bufcharcountout++;
//				}
				buf2++;
				bufcharcount2--;
				offset2++;
				filesize2--;
			}
		}
	}

	data_count = data_count + write_file(file_out, bufOut, bufcharcountout, offsetout);

	lasttracker = lastreadstr;

	if (dflg) {
		kargs->data = &data_count;
		copy_to_user(input->data, kargs->data, sizeof(unsigned int));
	}

exit:

	filp_close(file_in1, NULL);
	filp_close(file_in2, NULL);
	filp_close(file_out, NULL);

	if (lastreadstr != NULL) {
		kfree(lastreadstr);
	}

	if (bufOut != NULL) {
		kfree(bufOut);
	}

	return errval;
}


asmlinkage long xmergesort(void *arg)
{
	int errval = 0, in1_len, in2_len, out_len;
	char *bufferIn1 = NULL, *bufferIn2 = NULL, *bufferOut = NULL;
	bool uflag = false, aflag = false, tflag = false, iflag = false, dflag = false;
	unsigned int flag;

	if (arg == NULL) {
		printk(KERN_ERR "Invalid arguments");
		errval = -EINVAL;
		goto exit;
	}

	if (!access_ok(VERIFY_READ, arg, sizeof(struct inputstruct))) {
		printk(KERN_ERR "Access denied\n");
		errval = -EFAULT;
		goto exit;
	}

	input = (struct inputstruct *) arg;
	kargs = (struct inputstruct *) kmalloc(sizeof(struct inputstruct), GFP_KERNEL);

	if ((!kargs)) {
		printk(KERN_ERR "Memory allocation failed\n");
		errval = -ENOMEM;
		goto exit;
	}

	bufferIn1 = kmalloc(PAGE_SIZE, GFP_KERNEL);
	bufferIn2 = kmalloc(PAGE_SIZE, GFP_KERNEL);
	bufferOut = kmalloc(PAGE_SIZE, GFP_KERNEL);

	if (!bufferIn1 || !bufferIn2 || !bufferOut) {
		printk(KERN_ERR "Memory allocation failed\n");
		errval = -ENOMEM;
		goto exit;
	}

	if (copy_from_user(&(kargs->flags), &(input->flags), sizeof(unsigned int))) {
		printk(KERN_ERR "Error in copying file\n");
		errval = -EFAULT;
		goto exit;
	}

	flag = kargs->flags;

	if (flag > 23) {
		dflag = true;
		flag = flag - 32;
	}
	if (flag > 7) {
		tflag = true;
		flag = flag - 16;
	}
	if (flag > 4) {
		iflag = true;
		flag = flag - 7;
	}
	if (flag > 1) {
		aflag = true;
		flag = flag - 2;
	}
	if (flag == 1) {
		uflag = true;
	}

	kargs->inFile1 = kmalloc(strlen_user(input->inFile1), GFP_KERNEL);
	kargs->inFile2 = kmalloc(strlen_user(input->inFile2), GFP_KERNEL);
	kargs->outFile = kmalloc(strlen_user(input->outFile), GFP_KERNEL);

	if (!kargs->inFile1 || !kargs->inFile2 || !kargs->outFile) {
		printk(KERN_ERR "Memory allocation failed\n");
		errval = -ENOMEM;
		goto exit;
	}

	in1_len = strncpy_from_user(kargs->inFile1, input->inFile1, strlen_user(input->inFile1));
	in2_len = strncpy_from_user(kargs->inFile2, input->inFile2, strlen_user(input->inFile2));
	out_len = strncpy_from_user(kargs->outFile, input->outFile, strlen_user(input->outFile));

	if (!kargs->inFile1 || !kargs->inFile2 || !kargs->outFile) {
		printk(KERN_ERR "Kernel space arguments missing");
		errval = -EINVAL;
		goto exit;
	}

	if (kargs->inFile1 == NULL || kargs->inFile2 == NULL || kargs->outFile == NULL) {
		printk(KERN_ERR "Some argument is Null");
		errval = -EINVAL;
		goto exit;
	}

	errval = mergefiles(bufferIn1, bufferIn2, bufferOut, uflag, aflag, iflag, tflag, dflag);

	exit:

	if (bufferIn1 != NULL)
		kfree(bufferIn1);

	if (bufferIn2 != NULL)
		kfree(bufferIn2);

	if (bufferOut != NULL)
		kfree(bufferOut);

	if (kargs != NULL)
		kfree(kargs);

	return errval;
}


static int __init init_sys_xmergesort(void)
{
	printk(KERN_ERR "installed new sys_xmergesort module\n");
	if (sysptr == NULL)
		sysptr = xmergesort;
	return 0;
}


static void  __exit exit_sys_xmergesort(void)
{
	if (sysptr != NULL)
		sysptr = NULL;
	printk(KERN_ERR "removed sys_xmergesort module\n");
}

module_init(init_sys_xmergesort);
module_exit(exit_sys_xmergesort);
MODULE_LICENSE("GPL");
