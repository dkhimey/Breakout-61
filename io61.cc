#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <climits>
#include <cerrno>

// io61.c
//    YOUR CODE HERE!


// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

struct io61_file {
    int fd;
    static constexpr off_t bufsize = 16384; // block size for this cache
    unsigned char cbuf[bufsize];
    int mode;
    // These “tags” are addresses—file offsets—that describe the cache’s contents.
    off_t tag;      // file offset of first byte in cache (0 when file is opened)
    off_t end_tag;  // file offset one past last valid byte in cache
    off_t pos_tag;
};


// io61_fdopen(fd, mode)
//    Return a new io61_file for file descriptor `fd`. `mode` is
//    either O_RDONLY for a read-only file or O_WRONLY for a
//    write-only file. You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    f->mode = mode;
    return f;
}


// io61_close(f)
//    Close the io61_file `f` and release all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    delete f;
    return r;
}

// io61_fill()
// Fills the cache for file f with data.
int io61_fill(io61_file* f) {
    // Check invariants.
    // assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    // assert(f->end_tag - f->pos_tag <= f->bufsize);
    // Reset the cache to empty.
    f->tag = f->pos_tag = f->end_tag;
    // Read data.
    ssize_t n = read(f->fd, f->cbuf, f->bufsize);
    if (n >= 0) {
        f->end_tag = f->tag + n;
        return 0;
    }
    else {
        return -1;
    }
    // Recheck invariants (good practice!).
    // assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    // assert(f->end_tag - f->pos_tag <= f->bufsize);
}

// io61_readc(f)
//    Read a single (unsigned) character from `f` and return it. Returns EOF
//    (which is -1) on error or end-of-file.

int io61_readc(io61_file* f) {
    unsigned char buf[1];
    if (io61_read(f, buf, 1) == 1) {
        return buf[0];
    } else {
        return EOF;
    }
}

// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count, which might be zero, if the file ended before `sz` characters
//    could be read. Returns -1 if an error occurred before any characters
//    were read.

ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) {
    // printf("size: %zu\n", sz);
    // Check invariants.
    // assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    // assert(f->end_tag - f->pos_tag <= f->bufsize);
    size_t pos = 0;
    while (pos < sz) {
        if (f->pos_tag == f->end_tag) {
            int test = io61_fill(f);
            if (test < 0) {
                return -1;
            }
            if (f->pos_tag == f->end_tag) {
                break;
            }
        }
        // if sz-pos fits in cache, copy completely
        // instead of one byte, memcpy into the remaining room in the cache
        size_t remaining = (f->end_tag - f->pos_tag);
        if ((sz-pos) < remaining) {
            remaining = (sz-pos);
        }
        memcpy(&buf[pos], &f->cbuf[f->pos_tag - f->tag], remaining);
        pos += remaining;
        f->pos_tag += remaining;
    }
    return pos;
}

// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file* f, int ch) {
    unsigned char buf[1];
    buf[0] = ch;
    if (io61_write(f, buf, 1) == 1) {
        return 0;
    } else {
        return -1;
    }
}

// io61_write(f, buf, sz)
//    Write `sz` characters from `buf` to `f`. Returns the number of
//    characters written on success; normally this is `sz`. Returns -1 if
//    an error occurred before any characters were written.

ssize_t io61_write(io61_file* f, const unsigned char* buf, size_t sz) {
    // Check invariants.
    // assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    // assert(f->end_tag - f->pos_tag <= f->bufsize);
    // Write cache invariant.
    // assert(f->pos_tag == f->end_tag);
    size_t pos = 0;
    while (pos < sz) {
        if (f->end_tag == f->tag + f->bufsize) {
            int test = io61_flush(f);
            if (test < 0) {
                return -1;
            }
        }
        size_t remaining = f->bufsize + f->tag - f->end_tag;
        if ((sz-pos) < remaining) {
            remaining = (sz-pos);
        }
        memcpy(&f->cbuf[f->pos_tag - f->tag], &buf[pos], remaining);
        pos += remaining;
        f->pos_tag += remaining;
        f->end_tag += remaining;
    }
    return pos;
}

// io61_flush(f)
//    Forces a write of all buffered data written to `f`.
//    If `f` was opened read-only, io61_flush(f) may either drop all
//    data buffered for reading, or do nothing.

int io61_flush(io61_file* f) {
    ssize_t n = write(f->fd, f->cbuf, f->pos_tag - f->tag);
    if (n < 0) {
        return -1;
    }
    f->tag = f->pos_tag;
    f->end_tag = f->pos_tag;
    return 0; 
}

// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t pos) {
    if (pos >= f->tag && pos <= f->end_tag){
        f->pos_tag = pos;
        return 0;
    }
    else if (f->mode == O_RDONLY) {
        off_t tag = pos - (pos % f->bufsize);
        off_t r = lseek(f->fd, tag, SEEK_SET);
        if (r != tag) {
            return -1;
        }
        f->pos_tag = f->end_tag = f->tag = tag;
        io61_fill(f);
        f->pos_tag = pos;
        return 0;
    }
    else if (f->mode == O_WRONLY){
        io61_flush(f);
        off_t r = lseek(f->fd, pos, SEEK_SET);
        if (r != pos) {
            return -1;
        }
        f->tag = f->end_tag = f->pos_tag = pos;
        return 0;
    }
    return -1;
}


// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Open the file corresponding to `filename` and return its io61_file.
//    If `!filename`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != nullptr` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename) {
        fd = open(filename, mode, 0666);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        fd = STDIN_FILENO;
    } else {
        fd = STDOUT_FILENO;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_filesize(f)
//    Return the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode)) {
        return s.st_size;
    } else {
        return -1;
    }
}
