// SPDX-License-Identifier: GPL-2.0-only
//! Obsidian kernel dynamic memory backend (Rust).
//!
//! Implements exactly the same C ABI as mm/heap_c.c so the build links
//! one or the other: first-fit free list over a single region,
//! address-ordered blocks, adjacent frees coalesce, 16-byte headers
//! with magic values that catch double frees and wild pointers.

#![no_std]

use core::ptr;

#[panic_handler]
fn obsidian_panic(_info: &core::panic::PanicInfo) -> ! {
    loop {
        core::hint::spin_loop();
    }
}

const ALIGN: usize = 16;
const MAGIC_USED: u32 = 0x0B51_D1A0;
const MAGIC_FREE: u32 = 0xF4EE_B10C;
const END_OF_LIST: u32 = u32::MAX;
const HDR_SIZE: usize = 16;

#[repr(C)]
struct BlockHdr {
    magic: u32,
    size: u32,
    next_off: u32,
    used: u32,
}

struct State {
    base: *mut u8,
    size: usize,
    free_head: u32,
}

static mut ST: State = State {
    base: ptr::null_mut(),
    size: 0,
    free_head: END_OF_LIST,
};

fn align_up(v: usize) -> usize {
    (v + ALIGN - 1) & !(ALIGN - 1)
}

unsafe fn hdr_at(off: u32) -> *mut BlockHdr {
    (*ptr::addr_of_mut!(ST)).base.add(off as usize) as *mut BlockHdr
}

#[no_mangle]
pub extern "C" fn heap_init(base: u32, size: u32) -> bool {
    if size < HDR_SIZE as u32 {
        return false;
    }

    let aligned = align_up(base as usize);
    let total = align_up(size as usize);
    let span = total - (aligned - base as usize);

    unsafe {
        let st = ptr::addr_of_mut!(ST);
        (*st).base = aligned as *mut u8;
        (*st).size = span;
        (*st).free_head = 0;

        let first = hdr_at(0);
        (*first).magic = MAGIC_FREE;
        (*first).size = (span - HDR_SIZE) as u32;
        (*first).next_off = END_OF_LIST;
        (*first).used = 0;
    }
    true
}

#[no_mangle]
pub extern "C" fn kmalloc(size: u32) -> *mut u8 {
    if size == 0 {
        return ptr::null_mut();
    }

    unsafe {
        let want = align_up(size as usize) as u32;
        let st = ptr::addr_of_mut!(ST);
        let mut prev: u32 = END_OF_LIST;
        let mut cur = (*st).free_head;

        while cur != END_OF_LIST {
            let h = hdr_at(cur);

            if (*h).magic != MAGIC_FREE {
                return ptr::null_mut(); /* corrupted header */
            }
            if ((*h).size as usize) >= want as usize {
                let rem = (*h).size - want;

                if rem >= (HDR_SIZE + ALIGN) as u32 {
                    /* split: new free block follows the allocated one */
                    let split_off = cur + (HDR_SIZE as u32) + want;
                    let split = hdr_at(split_off);

                    (*split).magic = MAGIC_FREE;
                    (*split).size = rem - HDR_SIZE as u32;
                    (*split).next_off = (*h).next_off;
                    (*split).used = 0;

                    (*h).next_off = split_off;
                }
                (*h).magic = MAGIC_USED;
                (*h).used = 1;

                if prev == END_OF_LIST {
                    (*st).free_head = (*h).next_off;
                } else {
                    (*hdr_at(prev)).next_off = (*h).next_off;
                }
                return (*st).base.add(cur as usize + HDR_SIZE);
            }
            prev = cur;
            cur = (*h).next_off;
        }
    }
    ptr::null_mut()
}

#[no_mangle]
pub extern "C" fn kzalloc(size: u32) -> *mut u8 {
    let p = kmalloc(size);

    if !p.is_null() {
        unsafe {
            ptr::write_bytes(p, 0, size as usize);
        }
    }
    p
}

unsafe fn block_size_of(ptr: *mut u8) -> Option<u32> {
    let h = ptr.sub(HDR_SIZE) as *mut BlockHdr;

    if (*h).magic == MAGIC_USED && (*h).used == 1 {
        Some((*h).size)
    } else {
        None
    }
}

#[no_mangle]
pub extern "C" fn krealloc(ptr_: *mut u8, size: u32) -> *mut u8 {
    if ptr_.is_null() {
        return kmalloc(size);
    }
    if size == 0 {
        kfree(ptr_);
        return ptr::null_mut();
    }

    unsafe {
        let old = match block_size_of(ptr_) {
            Some(s) => s,
            None => return ptr::null_mut(),
        };
        let fresh = kmalloc(size);

        if !fresh.is_null() {
            let n = if old < size { old } else { size };
            ptr::copy_nonoverlapping(ptr_, fresh, n as usize);
            kfree(ptr_);
        }
        fresh
    }
}

/// Insert a freed block back into the list keeping it address ordered,
/// merging with left and right neighbours when they touch.
#[no_mangle]
pub extern "C" fn kfree(ptr_: *mut u8) {
    if ptr_.is_null() {
        return;
    }

    unsafe {
        let off = ptr_.offset_from((*ptr::addr_of_mut!(ST)).base) as usize - HDR_SIZE;
        if off >= (*ptr::addr_of_mut!(ST)).size {
            return;
        }

        let head = ptr_ as usize - HDR_SIZE;
        let hdr = head as *mut BlockHdr;

        if (*hdr).magic != MAGIC_USED || (*hdr).used == 0 {
            return; /* double free or wild pointer: ignore */
        }
        (*hdr).magic = MAGIC_FREE;
        (*hdr).used = 0;

        let st = ptr::addr_of_mut!(ST);
        let mut prev: *mut BlockHdr = ptr::null_mut();
        let mut cur = (*st).free_head;

        while cur != END_OF_LIST && cur < head as u32 {
            prev = hdr_at(cur);
            cur = (*prev).next_off;
        }

        /* link into address order */
        (*hdr).next_off = cur;
        if prev.is_null() {
            (*st).free_head = head as u32;
        } else {
            (*prev).next_off = head as u32;
        }

        /* merge right */
        if cur != END_OF_LIST {
            let right = hdr_at(cur);
            if head + HDR_SIZE + (*hdr).size as usize == cur as usize {
                (*hdr).size += (HDR_SIZE as u32) + (*right).size;
                (*hdr).next_off = (*right).next_off;
            }
        }

        /* merge left */
        if !prev.is_null() {
            let psize = (*prev).size as usize;
            if prev as usize + HDR_SIZE + psize == head {
                (*prev).size += (HDR_SIZE as u32) + (*hdr).size;
                (*prev).next_off = (*hdr).next_off;
            }
        }
    }
}
