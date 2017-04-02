#include <levos/bitmap.h>
#include <levos/kernel.h>
#include <levos/arch.h>
#include <levos/arithmetic.h>

/* Returns the index of the element that contains the bit
   numbered BIT_IDX. */
static inline size_t
elem_idx(size_t bit_idx) 
{
	return bit_idx / ELEM_BITS;
}

/* Returns an elem_type where only the bit corresponding to
   BIT_IDX is turned on. */
static inline elem_type
bit_mask(size_t bit_idx) 
{
	return (elem_type) 1 << (bit_idx % ELEM_BITS);
}

/* Returns the number of elements required for BIT_CNT bits. */
static inline size_t
elem_cnt(size_t bit_cnt)
{
	return DIV_ROUND_UP(bit_cnt, ELEM_BITS);
}

/* Returns the number of bytes required for BIT_CNT bits. */
static inline size_t
byte_cnt(size_t bit_cnt)
{
	return sizeof(elem_type) * elem_cnt(bit_cnt);
}

/* Returns a bit mask in which the bits actually used in the last
   element of B's bits are set to 1 and the rest are set to 0. */
static inline elem_type
last_mask(const struct bitmap *b) 
{
	int last_bits = b->bit_cnt % ELEM_BITS;
	return last_bits ? ((elem_type) 1 << last_bits) - 1 : (elem_type) -1;
}

/* Creation and destruction. */

/* Initializes B to be a bitmap of BIT_CNT bits
   and sets all of its bits to false.
   Returns true if success, false if memory allocation
   failed. */
struct bitmap *
bitmap_create(size_t bit_cnt) 
{
	struct bitmap *b = malloc(sizeof *b);

	if (b != NULL) {
		b->bit_cnt = bit_cnt;
		b->bits = malloc(byte_cnt(bit_cnt));
		if (b->bits != NULL || bit_cnt == 0) {
			bitmap_set_all(b, false);
			return b;
		}
		free(b);
	}
	return NULL;
}

/* Creates and returns a bitmap with BIT_CNT bits in the
   BLOCK_SIZE bytes of storage preallocated at BLOCK.
   BLOCK_SIZE must be at least bitmap_needed_bytes(BIT_CNT). */
struct bitmap *
bitmap_create_in_buf(size_t bit_cnt, void *block, size_t block_size __unused)
{
	struct bitmap *b = block;

	b->bit_cnt = bit_cnt;
	b->bits = (elem_type *) (b + 1);
	bitmap_set_all(b, false);
	return b;
}

void
bitmap_create_using_buffer(size_t bit_cnt, void *buffer, struct bitmap *bmp)
{
    bmp->bit_cnt = bit_cnt;
    bmp->bits = buffer;
}

/* Returns the number of bytes required to accomodate a bitmap
   with BIT_CNT bits (for use with bitmap_create_in_buf()). */
size_t
bitmap_buf_size(size_t bit_cnt) 
{
	return sizeof(struct bitmap) + byte_cnt(bit_cnt);
}

#if 0
/* Destroys bitmap B, freeing its storage.
   Not for use on bitmaps created by
   bitmap_create_preallocated(). */
void
bitmap_destroy(struct bitmap *b) 
{
	if (b != NULL) {
		free (b->bits);
		free(b);
	}
}
#endif

/* Bitmap size. */

/* Returns the number of bits in B. */
size_t
bitmap_size(const struct bitmap *b)
{
	return b->bit_cnt;
}

/* Setting and testing single bits. */

/* Atomically sets the bit numbered IDX in B to VALUE. */
void
bitmap_set(struct bitmap *b, size_t idx, bool value) 
{
	if (value)
		bitmap_mark(b, idx);
	else
		bitmap_reset(b, idx);
}

/* Atomically sets the bit numbered BIT_IDX in B to true. */
void
bitmap_mark(struct bitmap *b, size_t bit_idx) 
{
	size_t idx = elem_idx(bit_idx);
	elem_type mask = bit_mask(bit_idx);

    //arch_atomic_or(&b->bits[idx], mask);
	asm ("orl %1, %0" : "=m" (b->bits[idx]) : "r" (mask) : "cc");
}

/* Atomically sets the bit numbered BIT_IDX in B to false. */
void
bitmap_reset(struct bitmap *b, size_t bit_idx) 
{
	size_t idx = elem_idx(bit_idx);
	elem_type mask = bit_mask(bit_idx);
    
    //arch_atomic_and(&b->bits[idx], ~mask);
	asm ("andl %1, %0" : "=m" (b->bits[idx]) : "r" (~mask) : "cc");
}

/* Atomically toggles the bit numbered IDX in B;
   that is, if it is true, makes it false,
   and if it is false, makes it true. */
void
bitmap_flip(struct bitmap *b, size_t bit_idx) 
{
	size_t idx = elem_idx(bit_idx);
	elem_type mask = bit_mask(bit_idx);

    //arch_atomic_xor(&b->bits[idx], mask);
	asm ("xorl %1, %0" : "=m" (b->bits[idx]) : "r" (mask) : "cc");
}

/* Returns the value of the bit numbered IDX in B. */
bool
bitmap_test(const struct bitmap *b, size_t idx) 
{
	return (b->bits[elem_idx(idx)] & bit_mask(idx)) != 0;
}

/* Setting and testing multiple bits. */

/* Sets all bits in B to VALUE. */
void
bitmap_set_all(struct bitmap *b, bool value) 
{
	bitmap_set_multiple(b, 0, bitmap_size(b), value);
}

/* Sets the CNT bits starting at START in B to VALUE. */
void
bitmap_set_multiple(struct bitmap *b, size_t start, size_t cnt, bool value) 
{
	size_t i;

	for (i = 0; i < cnt; i++)
		bitmap_set(b, start + i, value);
}

/* Returns the number of bits in B between START and START + CNT,
   exclusive, that are set to VALUE. */
size_t
bitmap_count(const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
	size_t i, value_cnt;

	value_cnt = 0;
	for(i = 0; i < cnt; i++)
	if(bitmap_test(b, start + i) == value)
		value_cnt++;
	return value_cnt;
}

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to VALUE, and false otherwise. */
bool
bitmap_contains(const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
	size_t i;

	for (i = 0; i < cnt; i++)
	if (bitmap_test(b, start + i) == value)
		return true;
	return false;
}

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
bitmap_any(const struct bitmap *b, size_t start, size_t cnt) 
{
	return bitmap_contains(b, start, cnt, true);
}

/* Returns true if no bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
bitmap_none(const struct bitmap *b, size_t start, size_t cnt) 
{
	return !bitmap_contains(b, start, cnt, true);
}

/* Returns true if every bit in B between START and START + CNT,
   exclusive, is set to true, and false otherwise. */
bool
bitmap_all(const struct bitmap *b, size_t start, size_t cnt) 
{
	return !bitmap_contains(b, start, cnt, false);
}

/* Finding set or unset bits. */

/* Finds and returns the starting index of the first group of CNT
   consecutive bits in B at or after START that are all set to
   VALUE.
   If there is no such group, returns BITMAP_ERROR. */
size_t
bitmap_scan(const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
	if (cnt <= b->bit_cnt) {
		size_t last = b->bit_cnt - cnt;
		size_t i;
		for (i = start; i <= last; i++)
			if (!bitmap_contains(b, i, cnt, !value))
				return i; 
	}

	return BITMAP_ERROR;
}

/* Finds the first group of CNT consecutive bits in B at or after
   START that are all set to VALUE, flips them all to !VALUE,
   and returns the index of the first bit in the group.
   If there is no such group, returns BITMAP_ERROR.
   If CNT is zero, returns 0.
   Bits are set atomically, but testing bits is not atomic with
   setting them. */
size_t
bitmap_scan_and_flip(struct bitmap *b, size_t start, size_t cnt, bool value)
{
  size_t idx = bitmap_scan(b, start, cnt, value);

  if (idx != BITMAP_ERROR) 
    bitmap_set_multiple(b, idx, cnt, !value);

  return idx;
}
