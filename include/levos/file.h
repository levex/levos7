#ifndef __LEVOS_FILE_H
#define __LEVOS_FILE_H

#include <levos/types.h>

struct file
{
    struct file_operations fops;
};

struct file_operations
{
    ssize_t (*read)(struct file *, void *, size_t);
};

#enddif /* __LEVOS_FILE_H */
