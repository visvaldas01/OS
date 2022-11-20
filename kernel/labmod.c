#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/namei.h>
#define BUFFER_SIZE 4096


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simple example Linux module.");
MODULE_VERSION("1.0");

static const char *NONE_STRUCT_MESG = "No structs recorded to file\n";

enum struct_type { NONE, VM_AREA, ADDR_SPACE };

static struct dentry *root_dir;
static struct dentry *args_file;

static enum struct_type current_struct = NONE; 
static struct vm_area_struct *vm_area = NULL;
static struct address_space *addr_space_struct = NULL;




//func declarations
struct vm_area_struct *mod_get_vm(int pid);
struct address_space *mod_get_addr_space(char filename[]);
static void print_addr_space(struct seq_file *file, struct address_space *addr_space);
static void print_vm_area(struct seq_file *file, struct vm_area_struct *vm_area);
static int print_struct(struct seq_file *file, void *data);
void parse_flags(struct seq_file *file, unsigned long flags);




// file operations
static int mod_open(struct inode *inode, struct file *file) {
    pr_info("labmod: debugfs file opened\n");
    return single_open(file, print_struct, NULL);
}

static ssize_t mod_write(struct file *file, const char __user *buffer, size_t length, loff_t *ptr_offset) {
  pr_info("labmod: get arguments\n");
  char user_message[BUFFER_SIZE];
  char filename[BUFFER_SIZE];
  int pid;
  if (copy_from_user(user_message, buffer, length)) {
    pr_err("labmod: Can't write to kernel");
    return -EFAULT;
  }
  pr_info("%s", user_message);
  if (sscanf(user_message, "filename: %s", filename) == 1) {
    pr_info("labmod: filename: %s", filename);
    current_struct = ADDR_SPACE;
    addr_space_struct = mod_get_addr_space(filename);
  } else if (sscanf(user_message, "pid: %d", &pid) == 1) {
    pr_info("labmod: pid %d", pid);
    current_struct = VM_AREA;
    vm_area = mod_get_vm(pid);
    if (vm_area == NULL) {
      pr_err("labmod: error occured while getting VM area\n");
    }
  } else {
    pr_err("labmod: Can't parse input");
    return -EINVAL;
  }
  single_open(file, print_struct, NULL);
  return strlen(user_message);
}  

static struct file_operations mod_io_ops = {
  .owner = THIS_MODULE,
  .read = seq_read,
  .write = mod_write,
  .open = mod_open,
};

// module entry/exit points
static int __init mod_init(void) {
  pr_info("labmod: module loaded\n");

  root_dir = debugfs_create_dir("labmod", NULL);
  args_file = debugfs_create_file("labmod_io", 0777, root_dir, NULL, &mod_io_ops);

  return 0;
}

static void __exit mod_exit(void) {
  debugfs_remove_recursive(root_dir);
  pr_info("labmod: module unloaded\n");
}

//structs output
static int print_struct(struct seq_file *file, void *data) {
  if (current_struct == ADDR_SPACE) {
    if (addr_space_struct == NULL) {
      seq_printf(file, "Address space struct with provided params not found\n");
      return 0;
    }
    print_addr_space(file, addr_space_struct);
  } else if (current_struct == VM_AREA) {
    if (vm_area == NULL) {
      seq_printf(file, "VM area struct with provided params not found\n");
      return 0;
    }
    print_vm_area(file, vm_area);
  } else {
    seq_printf(file, NONE_STRUCT_MESG);
  }
  return 0;
}

static void print_addr_space(struct seq_file *file, struct address_space *addr_space) {
  seq_printf(file, "Address space structure: {\n");
  seq_printf(file, "  pages: %lu,\n", addr_space->nrpages);
  seq_printf(file, "  flags: %lu,\n", addr_space->flags);
  seq_printf(file, "  host: %lu,\n", addr_space->host->i_uid);
  seq_printf(file, "  VM_SHARED mappings: %lu\n", addr_space->i_mmap_writable);
  seq_printf(file, "}\n");
}

static void print_vm_area(struct seq_file *file, struct vm_area_struct *vm_area) {
  seq_printf(file, "VM area structure: {\n");
  seq_printf(file, "  vm_start: %lx,\n", vm_area->vm_start);
  seq_printf(file, "  vm_end: %lx,\n", vm_area->vm_end);
  seq_printf(file, "  vm_flags: %lx,\n", vm_area->vm_flags);
  parse_flags(file, vm_area->vm_flags);
  seq_printf(file, "  vm_pgoff: %lx,\n", vm_area->vm_pgoff);
  if (!vm_area->vm_file) seq_printf(file, "  vm_file: -\n");
  else seq_printf(file, "  vm_file: %s\n", vm_area->vm_file->f_path.dentry->d_name.name);
  seq_printf(file, "}\n");
  if (vm_area->vm_next) print_vm_area(file, vm_area->vm_next);
}

struct vm_area_struct *mod_get_vm(int pid) {
  struct task_struct *task;
  struct mm_struct *mm;
  struct vm_area_struct *vm_area;

  task = get_pid_task(find_get_pid(pid), PIDTYPE_PID);
  if (task == NULL) {
    pr_err("labmod: process not found\n");
    return NULL;
  }
  mm = task->mm;
  if (mm == NULL) {
    pr_err("labmod: process has no address space\n");
    return NULL;
  }

  vm_area = mm->mmap;
  return vm_area;
}

struct address_space *mod_get_addr_space(char filename[]) {
  struct path current_path;
  int status = kern_path(filename, LOOKUP_PARENT, &current_path);
  if (status) {
    pr_err("labmod: can't find file %s\n", filename);
    return NULL;
  }

  struct address_space *addr_space = current_path.dentry->d_inode->i_mapping;

  return addr_space;
}

void parse_flags(struct seq_file *file, unsigned long flags) {
  char str[BUFFER_SIZE];
  size_t end = sprintf(str, "%lx", flags);
  str[end] = '\0';
  size_t len = strlen(str);
  int arr[len];
  int i;
  for (i = 0; str[i] != 0; i++) {
    arr[i] = str[i] - '0';
  }
  seq_printf(file, "  decoded: ");
  if (len > 0) {
    if (arr[len-1]-8 >= 0) {
      seq_printf(file, "sh ");
      arr[len-1] -= 8;
    }
    if (arr[len-1]-4 >= 0) {
      seq_printf(file, "ex ");
      arr[len-1] -= 4;
    }
    if (arr[len-1]-2 >= 0) {
      seq_printf(file, "wr ");
      arr[len-1] -= 2;
    }
    if (arr[len-1]-1 >= 0) {
      seq_printf(file, "rd ");
      arr[len-1] -= 1;
    }
  }
  if (len > 1) {
    if (arr[len-2]-8 >= 0) {
      seq_printf(file, "ms ");
      arr[len-2] -= 8;
    }
    if (arr[len-2]-4 >= 0) {
      seq_printf(file, "me ");
      arr[len-2] -= 4;
    }
    if (arr[len-2]-2 >= 0) {
      seq_printf(file, "mw ");
      arr[len-2] -= 2;
    }
    if (arr[len-2]-1 >= 0) {
      seq_printf(file, "mr ");
      arr[len-2] -= 1;
    }	
  }
  if (len > 2) {
    if (arr[len-3]-8 >= 0) {
      seq_printf(file, "dw ");
      arr[len-3] -= 8;
    }
    if (arr[len-3]-4 >= 0) {
      seq_printf(file, "pf ");
      arr[len-3] -= 4;
    }
    if (arr[len-3]-2 >= 0) {
      seq_printf(file, "um ");
      arr[len-3] -= 2;
    }
    if (arr[len-3]-1 >= 0) {
      seq_printf(file, "gd ");
      arr[len-3] -= 1;
    }
  }
  if (len > 3) {
    if (arr[len-4]-8 >= 0) {
      seq_printf(file, "sr ");
      arr[len-4] -= 8;
    }
    if (arr[len-4]-4 >= 0) {
      seq_printf(file, "io ");
      arr[len-4] -= 4;
    }
    if (arr[len-4]-2 >= 0) {
      seq_printf(file, "lo ");
      arr[len-4] -= 2;
    }
    if (arr[len-4]-1 >= 0) {
      seq_printf(file, "uw ");
      arr[len-4] -= 1;
    }
  }
  if (len > 4) {
    if (arr[len-5]-8 >= 0) {
      seq_printf(file, "VM_LOCKONFAULT"); // dunno its 2-letter code
      arr[len-5] -= 8;
    }
    if (arr[len-5]-4 >= 0) {
      seq_printf(file, "de ");
      arr[len-5] -= 4;
    }
    if (arr[len-5]-2 >= 0) {
      seq_printf(file, "dc ");
      arr[len-5] -= 2;
    }
    if (arr[len-5]-1 >= 0) {
      seq_printf(file, "rr ");
      arr[len-5] -= 1;
    }
  }
  if (len > 5) {
    if (arr[len-6]-8 >= 0) {
      seq_printf(file, "sf ");
      arr[len-6] -= 8;
    }
    if (arr[len-6]-4 >= 0) {
      seq_printf(file, "ht ");
      arr[len-6] -= 4;
    }
    if (arr[len-6]-2 >= 0) {
      seq_printf(file, "nr ");
      arr[len-6] -= 2;
    }
    if (arr[len-6]-1 >= 0) {
      seq_printf(file, "ac ");
      arr[len-6] -= 1;
    }
  }
  if (len > 6) {
    if (arr[len-7]-8 >= 0) {
      seq_printf(file, "sd ");
      arr[len-7] -= 8;
    }
    if (arr[len-7]-4 >= 0) {
      seq_printf(file, "dd ");
      arr[len-7] -= 4;
    }
    if (arr[len-7]-2 >= 0) {
      seq_printf(file, "wf ");
      arr[len-7] -= 2;
    }
    if (arr[len-7]-1 >= 0) {
      seq_printf(file, "ar ");
      arr[len-7] -= 1;
    }
  }
  if (len > 7) {
    if (arr[len-8]-8 >= 0) {
      seq_printf(file, "mm ");
      arr[len-8] -= 8;
    }
    if (arr[len-8]-4 >= 0) {
      seq_printf(file, "hg ");
      arr[len-8] -= 4;
    }
    if (arr[len-8]-2 >= 0) {
      seq_printf(file, "nh ");
      arr[len-8] -= 2;
    }
    if (arr[len-8]-1 >= 0) {
      seq_printf(file, "mg ");
      arr[len-8] -= 1;
    }
  }
  seq_printf(file, "\n");
}

module_init(mod_init);
module_exit(mod_exit);