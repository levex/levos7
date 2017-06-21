#include <levos/kernel.h>
#include <levos/fs.h>
#include <levos/ext2.h>
#include <levos/string.h>
#include <levos/list.h>
#include <levos/task.h>

#define MAX_MOUNTS 256

struct mount *mounts[MAX_MOUNTS];
int nmounts = 0;

struct mount *root_mount = 0;

struct fs_ops *fs_ops_s[8];
static int fs_ops_n = 0;

void
file_seek(struct file *file, int pos)
{
    file->fpos = pos;
}


/*
 * __check_mounts - Checks whether a path is already mounted
 *
 * @internal
 */
struct mount *__check_mounts(char *str)
{
    for (int i = 0; i < nmounts; i++)
        if (strcmp(mounts[i]->point, str) == 0)
            return mounts[i];

    return 0;
}

/*
 * __vfs_set_mount - Sets path to be mounted as a particular
 * filesystem on the device
 *
 * @internal
 */
int __vfs_set_mount(char *p, struct device *dev, struct filesystem *fs)
{
    struct mount *m = malloc(sizeof(*m));
    if (!m)
        return -ENOMEM;

    m->point = p;
    m->fs = fs;
    m->dev = dev;
    mounts[nmounts] = m;
    nmounts ++;
    //printk("vfs: mounted %s on %s to %s\n", fs->fs_ops->fsname, dev->name, p);
    if (strcmp(p, "/") == 0)
        root_mount = m;
    return 0;
}

/*
 * find_mount - Finds the internal mount structure for
 * a particular path
 */
struct mount *find_mount(char *path)
{
    struct mount *ret = root_mount;
    char *str;

    /* create a copy of the string */
    str = malloc(strlen(path) + 1);
    memcpy(str, path, strlen(path) + 1);
    if (strcmp(str, "/") == 0)
        goto out;

    while (strlen(str) > 1) {
        //printk("Checking str %s\n", str);
        ret = __check_mounts(str);
        if (ret)
            goto out;
        str[strlen(str) - 1] = 0;
    }

    ret = root_mount;
out:
    free(str);
    return ret;
}

/*
 * vfs_mount_fs - Finds a filesystem that is willing to be
 * mounted on this particular device.
 *
 * WARNING: Filesystem will do its mounting on the device,
 * possibly even modify it!
 *
 * @internal
 */
struct filesystem *vfs_mount_fs(struct device *dev)
{
    struct filesystem *fs = 0;
    for (int i = 0; i < fs_ops_n; i++)
    {
        if (fs_ops_s[i]->mount) {
            fs = fs_ops_s[i]->mount(dev);
            if (fs && !IS_ERR(fs))
                return fs;
        }
    }
    return 0;
}

/*
 * vfs_mount - Mount a device on a path
 *
 * BUG: allows potential 'floating' mounts
 *
 */
int vfs_mount(char *path, struct device *dev)
{
    if (!root_mount && strcmp("/", path) != 0)
        return -EAGAIN;

    /* 
     * if there is a root mount then the path must exist and
     * be empty
     */
    if (root_mount) {
        /* FIXME */
    }

    struct filesystem *fs = vfs_mount_fs(dev);
    if (!fs)
        return -ENXIO;

    printk("%s: fs->name: %s\n", __func__, fs->fs_ops->fsname);

    return __vfs_set_mount(path, dev, fs);
}

/*
 * vfs_root_mounted - Returns whether root directory was mounted
 * or not
 */
int vfs_root_mounted()
{
    return (root_mount)?1:0;
}

/*
 * register_fs - Register a filesystem so that it can later probe and
 * mounted on devices.
 */
int register_fs(struct fs_ops *fs)
{
    if (!fs)
        return -EINVAL;

    printk("vfs: registered filesystem %s\n", fs->fsname);

    fs_ops_s[fs_ops_n ++] = fs;
    return 0;
}

/*
 * vfs_open - backend for FS-related open(2)
 */
struct file *vfs_open(char *path)
{
    /* HACK XXX */
    //if (strcmp(path, "/dev/urandom") == 0)
        //return dup_file(&urandom_base_file);
    //else if (strcmp(path, "/dev/null") == 0)
        //return dup_file(&null_base_file);

    struct mount *m = find_mount(path);

    char *exppath;
    if (strlen(path) == strlen(m->point)) {
        exppath = "/";
    } else {
        exppath = path + strlen(m->point);
    }

    //printk("exppath is %s\n", exppath);

    //printk("experimental path would be for \"%s\": \"%s\"\n",
            //path, exppath);

    /* FIXME: the path is wrong if multiple mounts exist */
   struct file *f = m->fs->fs_ops->open(m->fs, exppath);
   if (f && !IS_ERR(f)) {
       f->refc = 1;
       f->full_path = strdup(path);
   }

   return f;
}

void
vfs_close(struct file *f)
{

    //printk("CLOSING %s refc %d\n", f->respath, f->refc);

    f->refc --;

    if (f->refc == 0) {
        free(f->full_path);
        f->fops->close(f);
    }

    //dump_stack(8);
}

void
vfs_inc_refc(struct file *f)
{
    //printk("file %s increased refc from %d\n", f->respath, f->refc);
    f->refc ++;
    //dump_stack(8);
}

struct file *
vfs_create(char *path)
{
    struct mount *m = find_mount(path);

    //printk("experimental path would be for \"%s\": \"%s\"\n",
            //path, path + strlen(m->point) - 1);

    /* FIXME: the path is wrong if multiple mounts exist */
    if (m->fs->fs_ops->create)
        return m->fs->fs_ops->create(m->fs, path);

    return ERR_PTR(-EROFS);
}

int
vfs_truncate(struct file *f)
{
    if (f->fops->truncate)
        return f->fops->truncate(f, 0);

    return -EROFS;
}

struct file *
dup_file(struct file *f)
{
    struct file *ret = malloc(sizeof(*ret));
    if (!ret)
        return (void *) -ENOMEM;

    /* copy file operations */
    ret->fops = f->fops;
    /* they are on the same filesystem */
    ret->fs = f->fs;
    /* but it starts at zero */
    ret->fpos = 0;
    /* same statistics and other stuff */
    ret->isdir = f->isdir;
    if (f->respath)
        ret->respath = strdup(f->respath);
    ret->type = f->type;
    ret->length = f->length;
    ret->refc = 1;
    if (f->full_path)
        ret->full_path = strdup(f->full_path);

    /* XXX: GIANT HACK */
    if (f->priv)
        ret->priv = ext2_dup_priv(f->priv);

    /* done */
    return ret;
}

/*
 * vfs_stat - backend for FS-related stat(2)
 */
int vfs_stat(char *p, struct stat *buf)
{
    struct mount *m = find_mount(p);
    if (!m)
        panic("Location %s was not mounted!\n", p);

    if (!m->fs)
        panic("Mount deciphered for %s had not fs\n", p);

    if (!m->fs->fs_ops->stat)
        return -ENOSYS;

    char *exppath;
    if (strlen(p) == strlen(m->point)) {
        exppath = "/";
    } else {
        exppath = p + strlen(m->point);
    }

    return m->fs->fs_ops->stat(m->fs, exppath, buf);
}

/*
 * vfs_mkdir  - backend for FS-related mkdir(2)
 */
int vfs_mkdir(char *p, int mode)
{
    struct mount *m = find_mount(p);
    if (!m)
        panic("Location %s was not mounted!\n", p);

    if (!m->fs)
        panic("Mount deciphered for %s had not fs\n", p);

    if (!m->fs->fs_ops->mkdir)
        return -ENOSYS;

    return m->fs->fs_ops->mkdir(m->fs, p, mode);
}

/*
 * vfs_init - Initialize the Virtual Filesystem Layer
 */
int vfs_init(void)
{
    printk("vfs: loading filesystems\n");
    fs_ops_n = 0;

    ext2_init();
    procfs_init();
    devfs_init();

    return 0;
}

#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STRING "/"
#define PATH_UP  ".."
#define PATH_DOT "."

struct path_list {
    struct list_elem elem;
    char *s;
};

/**
 * canonicalize_path: Canonicalize a path.
 *
 * @param cwd   Current working directory
 * @param input Path to append or canonicalize on
 * @returns An absolute path string
 */
char *__canonicalize_path(char *cwd, char *input) {
	struct list out;

    list_init(&out);

	/*
	 * If we have a relative path, we need to canonicalize
	 * the working directory and insert it into the stack.
	 */
	if (strlen(input) && input[0] != PATH_SEPARATOR) {
		/* Make a copy of the working directory */
		char *path = malloc((strlen(cwd) + 1) * sizeof(char));
		memcpy(path, cwd, strlen(cwd) + 1);

		/* Setup tokenizer */
		char *pch;
		char *save;
		pch = strtok_r(path,PATH_SEPARATOR_STRING,&save);

		/* Start tokenizing */
		while (pch != NULL) {
			/* Make copies of the path elements */
			char *s = malloc(sizeof(char) * (strlen(pch) + 1));
            struct path_list *pl = malloc(sizeof(*pl));
			memcpy(s, pch, strlen(pch) + 1);
            pl->s = s;
			/* And push them */
			list_push_back(&out, &pl->elem);
			pch = strtok_r(NULL,PATH_SEPARATOR_STRING,&save);
		}
		free(path);
	}

	/* Similarly, we need to push the elements from the new path */
	char *path = malloc((strlen(input) + 1) * sizeof(char));
	memcpy(path, input, strlen(input) + 1);

	/* Initialize the tokenizer... */
	char *pch;
	char *save;
	pch = strtok_r(path,PATH_SEPARATOR_STRING,&save);

	/*
	 * Tokenize the path, this time, taking care to properly
	 * handle .. and . to represent up (stack pop) and current
	 * (do nothing)
	 */
	while (pch != NULL) {
		if (!strcmp(pch,PATH_UP)) {
			/*
			 * Path = ..
			 * Pop the stack to move up a directory
			 */
            if (!list_empty(&out)) {
                struct list_elem *elem = list_pop_back(&out);
                if (elem) {
                    struct path_list *n = list_entry(elem, struct path_list, elem);
                    free(n->s);
                    free(n);
                }
            }
		} else if (!strcmp(pch,PATH_DOT)) {
			/*
			 * Path = .
			 * Do nothing
			 */
		} else {
			/*
			 * Regular path, push it
			 * XXX: Path elements should be checked for existence!
			 */
			char *s = malloc(sizeof(char) * (strlen(pch) + 1));
            struct path_list *pl = malloc(sizeof(*pl));
			memcpy(s, pch, strlen(pch) + 1);
            pl->s = s;
            list_push_back(&out, &pl->elem);
		}
		pch = strtok_r(NULL, PATH_SEPARATOR_STRING, &save);
	}
	free(path);

	/* Calculate the size of the path string */
	size_t size = 0;
    struct list_elem *elem;
	list_foreach_raw(&out, elem) {
        struct path_list *item = list_entry(elem, struct path_list, elem);
		/* Helpful use of our foreach macro. */
		size += strlen(item->s) + 1;
	}

	/* join() the list */
	char *output = malloc(sizeof(char) * (size + 1));
	char *output_offset = output;
	if (size == 0) {
		/*
		 * If the path is empty, we take this to mean the root
		 * thus we synthesize a path of "/" to return.
		 */
		output = realloc(output, sizeof(char) * 2);
		output[0] = PATH_SEPARATOR;
		output[1] = '\0';
	} else {
		/* Otherwise, append each element together */
		list_foreach_raw(&out, elem) {
            struct path_list *item = list_entry(elem, struct path_list, elem);
			output_offset[0] = PATH_SEPARATOR;
			output_offset++;
			memcpy(output_offset, item->s, strlen(item->s) + 1);
			output_offset += strlen(item->s);
		}
	}

	/* Clean up the various things we used to get here */
    while (!list_empty(&out)) {
        struct path_list *item = list_entry(list_pop_front(&out), struct path_list, elem);
        free(item->s);
        free(item);
    }

    return output;
}

char *
canonicalize_path(char *cwd, char *ap)
{
    struct stat st;
    
    char *output = __canonicalize_path(cwd, ap);

    int rc = vfs_stat(output, &st);
    if (rc)
        return ERR_PTR(rc);

    if (!(st.st_mode & S_IFDIR))
        return ERR_PTR(-ENOTDIR);

	/* And return a working, absolute path */
	return output;
}

void
task_do_cloexec(struct task *task)
{
    int i;

    for (i = 0; i < FD_MAX; i ++) {
        struct file *f = task->file_table[i];
        if (f && f->mode & O_CLOEXEC || task->file_table_flags[i] & FD_CLOEXEC) {
            vfs_close(f);
            task->file_table[i] = NULL;
            task->file_table_flags[i] = 0;
        }
    }
}
