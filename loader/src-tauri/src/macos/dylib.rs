#![allow(deprecated)]
#![allow(non_camel_case_types)]
#![allow(arithmetic_overflow)]

use crate::dprintln;
use libc::*;
use std::mem::{transmute, zeroed};

static CODESIG_FLAG: i32 = 0;

const FAT_MAGIC: u32 = 0xcafebabe;
const FAT_CIGAM: u32 = 0xbebafeca; /* NXSwapLong(FAT_MAGIC) */

const LC_SYMTAB: u32 = 0x2;
const LC_REQ_DYLD: u32 = 0x80000000;
const LC_CODE_SIGNATURE: u32 = 0x1d;
const LC_LOAD_DYLIB: u32 = 0xc;
const LC_LOAD_WEAK_DYLIB: u32 = 0x18 | LC_REQ_DYLD;

const MH_MAGIC: u32 = 0xfeedface; /* the mach magic number */
const MH_CIGAM: u32 = 0xcefaedfe; /* NXSwapInt(MH_MAGIC) */
const MH_MAGIC_64: u32 = 0xfeedfacf; /* the 64-bit mach magic number */
const MH_CIGAM_64: u32 = 0xcffaedfe; /* NXSwapInt(MH_MAGIC_64) */

#[repr(C)]
#[derive(Clone)]
struct fat_header {
    pub magic: u32,     /* FAT_MAGIC or FAT_MAGIC_64 */
    pub nfat_arch: u32, /* number of structs that follow */
}

#[repr(C)]
#[derive(Clone)]
struct fat_arch {
    pub cputype: i32,    /* cpu specifier (int) */
    pub cpusubtype: i32, /* machine specifier (int) */
    pub offset: u32,     /* file offset to this object file */
    pub size: u32,       /* size of this object file */
    pub align: u32,      /* alignment as a power of 2 */
}

#[repr(C)]
#[derive(Clone)]
struct dylib_ {
    pub offset: u32,                /* library's path name */
    pub timestamp: u32,             /* library's build time stamp */
    pub current_version: u32,       /* library's current version number */
    pub compatibility_version: u32, /* library's compatibility vers number*/
}

#[repr(C)]
#[derive(Clone)]
struct dylib_command {
    pub cmd: u32,      /* LC_ID_DYLIB, LC_LOAD_{,WEAK_}DYLIB, LC_REEXPORT_DYLIB */
    pub cmd_size: u32, /* includes pathname string */
    pub dylib: dylib_, /* the library identification */
}

#[repr(C)]
#[derive(Clone)]
struct linkedit_data_command {
    pub cmd: u32,      /* LC_CODE_SIGNATURE, LC_SEGMENT_SPLIT_INFO,
                       LC_FUNCTION_STARTS, LC_DATA_IN_CODE,
                       LC_DYLIB_CODE_SIGN_DRS, LC_ATOM_INFO,
                       LC_LINKER_OPTIMIZATION_HINT,
                       LC_DYLD_EXPORTS_TRIE, or
                       LC_DYLD_CHAINED_FIXUPS. */
    pub cmdsize: u32,  /* sizeof(struct linkedit_data_command) */
    pub dataoff: u32,  /* file offset of data in __LINKEDIT segment */
    pub datasize: u32, /* file size of data in __LINKEDIT segment  */
}

#[repr(C)]
#[derive(Clone)]
struct symtab_command {
    pub cmd: u32,     /* LC_SYMTAB */
    pub cmdsize: u32, /* sizeof(struct symtab_command) */
    pub symoff: u32,  /* symbol table offset */
    pub nsyms: u32,   /* number of symbol table entries */
    pub stroff: u32,  /* string table offset */
    pub strsize: u32, /* string table size in bytes */
}

fn is_little_endian(magic: u32) -> bool {
    magic == FAT_CIGAM || magic == MH_CIGAM_64 || magic == MH_CIGAM
}

fn swap32(x: u32, magic: u32) -> u32 {
    if is_little_endian(magic) {
        x.to_be()
    } else {
        x
    }
}

fn swap64(x: u64, magic: u32) -> u64 {
    if is_little_endian(magic) {
        x.to_be()
    } else {
        x
    }
}

fn round_up(x: u64, y: u32) -> u64 {
    (x + (y as u64) - 1) & (-i64::from(y) as u64)
}

fn absdiff(x: off_t, y: off_t) -> usize {
    if x > y {
        x as usize - y as usize
    } else {
        y as usize - x as usize
    }
}

type off_t = i64;
const BUFSIZE: size_t = 512;

unsafe fn fbzero(f: *mut FILE, offset: off_t, mut len: size_t) {
    const ZEROES: [u8; BUFSIZE] = [0; BUFSIZE];
    fseeko(f, offset, SEEK_SET);
    while len != 0 {
        let size = std::cmp::min(len, BUFSIZE);
        fwrite(ZEROES.as_ptr() as *const c_void, size, 1, f);
        len -= size;
    }
}

unsafe fn fmemmove(f: *mut FILE, mut dst: off_t, mut src: off_t, mut len: size_t) {
    static BUF: [u8; BUFSIZE] = [0; BUFSIZE];
    while len != 0 {
        let size = std::cmp::min(len, BUFSIZE);
        fseeko(f, src, SEEK_SET);
        fread(transmute(BUF.as_ptr()), size, 1, f);
        fseeko(f, dst, SEEK_SET);
        fwrite(BUF.as_ptr() as *const c_void, size, 1, f);

        len -= size;
        src += size as off_t;
        dst += size as off_t;
    }
}

unsafe fn fpeek(ptr: *mut c_void, size: size_t, nitems: size_t, stream: *mut FILE) -> size_t {
    let pos = ftello(stream);
    let result = fread(ptr, size, nitems, stream);
    fseeko(stream, pos, SEEK_SET);
    return result;
}

unsafe fn read_load_command(f: *mut FILE, cmdsize: u32) -> *mut c_void {
    let lc = malloc(cmdsize as size_t);
    fpeek(lc, cmdsize as size_t, 1, f);
    return lc;
}

unsafe fn check_load_commands(
    f: *mut FILE,
    mh: &mut mach_header,
    header_offset: off_t,
    commands_offset: off_t,
    dylib_path: *const c_char,
    slice_size: &mut off_t,
    cont_anyway: bool,
) -> bool {
    fseeko(f, commands_offset, SEEK_SET);

    let ncmds = swap32(mh.ncmds, mh.magic);

    let mut linkedit_32_pos: off_t = -1;
    let mut linkedit_64_pos: off_t = -1;
    let mut linkedit_32: segment_command = zeroed();
    let mut linkedit_64: segment_command_64 = zeroed();

    let mut symtab_pos: off_t = -1;
    let mut symtab_size: size_t = 0;

    for i in 0..ncmds - 1 {
        let lc: load_command = zeroed();
        fpeek(transmute(&lc), size_of::<load_command>(), 1, f);

        let cmdsize = swap32(lc.cmdsize, mh.magic);
        let cmd = swap32(lc.cmd, mh.magic);

        match cmd {
            LC_CODE_SIGNATURE => {
                let mut fix_header = false;

                if i == ncmds - 1 {
                    if CODESIG_FLAG == 2 {
                        return true;
                    }

                    if CODESIG_FLAG == 0 {
                        dprintln!("LC_CODE_SIGNATURE load command found. Remove it?");
                        if !cont_anyway {
                            return true;
                        }
                    }

                    let cmd: &mut linkedit_data_command = transmute(read_load_command(f, cmdsize));

                    fbzero(f, ftello(f), cmdsize as size_t);

                    let dataoff = swap32(cmd.dataoff, mh.magic);
                    let datasize = swap32(cmd.datasize, mh.magic);

                    free(transmute(cmd));

                    let mut linkedit_fileoff: u64 = 0;
                    let mut linkedit_filesize: u64 = 0;

                    if linkedit_32_pos != -1 {
                        linkedit_fileoff = swap32(linkedit_32.fileoff, mh.magic) as u64;
                        linkedit_filesize = swap32(linkedit_32.filesize, mh.magic) as u64;
                    } else if linkedit_64_pos != -1 {
                        linkedit_fileoff = swap64(linkedit_64.fileoff, mh.magic);
                        linkedit_filesize = swap64(linkedit_64.filesize, mh.magic);
                    } else {
                        dprintln!("Warning: __LINKEDIT segment not found.");
                    }

                    if linkedit_32_pos != -1 || linkedit_64_pos != -1 {
                        if linkedit_fileoff + linkedit_filesize != *slice_size as u64 {
                            dprintln!("Warning: __LINKEDIT segment is not at the end of the file, so codesign will not work on the patched binary.");
                        } else {
                            if dataoff + datasize != *slice_size as u32 {
                                dprintln!("Warning: Codesignature is not at the end of __LINKEDIT segment, so codesign will not work on the patched binary.");
                            } else {
                                *slice_size -= datasize as i64;
                                //int64_t diff_size = 0;
                                if symtab_pos == -1 {
                                    dprintln!("Warning: LC_SYMTAB load command not found. codesign might not work on the patched binary.");
                                } else {
                                    fseeko(f, symtab_pos, SEEK_SET);
                                    let symtab: *mut symtab_command =
                                        transmute(read_load_command(f, symtab_size as u32));

                                    let strsize = swap32((*symtab).strsize, mh.magic);
                                    let diff_size: i64 = swap32((*symtab).stroff as u32, mh.magic)
                                        as i64
                                        + strsize as i64
                                        - *slice_size;

                                    if -0x10 <= diff_size && diff_size <= 0 {
                                        (*symtab).strsize =
                                            swap32(strsize - diff_size as u32, mh.magic);
                                        fwrite(transmute(symtab), symtab_size, 1, f);
                                    } else {
                                        dprintln!("Warning: String table doesn't appear right before code signature. codesign might not work on the patched binary. {:#}", diff_size);
                                    }

                                    free(transmute(symtab));
                                }

                                linkedit_filesize -= datasize as u64;
                                let linkedit_vmsize: u64 = round_up(linkedit_filesize, 0x1000);

                                if linkedit_32_pos != -1 {
                                    linkedit_32.filesize =
                                        swap32(linkedit_filesize as u32, mh.magic);
                                    linkedit_32.vmsize = swap32(linkedit_vmsize as u32, mh.magic);

                                    fseeko(f, linkedit_32_pos, SEEK_SET);
                                    fwrite(
                                        transmute(&linkedit_32),
                                        size_of_val(&linkedit_32),
                                        1,
                                        f,
                                    );
                                } else {
                                    linkedit_64.filesize = swap64(linkedit_filesize, mh.magic);
                                    linkedit_64.vmsize = swap64(linkedit_vmsize, mh.magic);

                                    fseeko(f, linkedit_64_pos, SEEK_SET);
                                    fwrite(
                                        transmute(&linkedit_64),
                                        size_of_val(&linkedit_64),
                                        1,
                                        f,
                                    );
                                }

                                fix_header = true;
                            }
                        }
                    }

                    if !fix_header {
                        // If we haven't truncated the file, zero out the code signature
                        fbzero(f, header_offset + dataoff as i64, datasize as size_t);
                    }

                    mh.ncmds = swap32(ncmds - 1, mh.magic);
                    mh.sizeofcmds = swap32(swap32(mh.sizeofcmds, mh.magic) - cmdsize, mh.magic);

                    return true;
                } else {
                    dprintln!(
                        "LC_CODE_SIGNATURE is not the last load command, so couldn't remove."
                    );
                }
            }

            LC_LOAD_DYLIB | LC_LOAD_WEAK_DYLIB => {
                let dylib_command: *mut dylib_command = transmute(read_load_command(f, cmdsize));

                let offset: u32 = (*dylib_command).dylib.offset;
                let name: *const c_char = transmute(
                    transmute::<_, isize>(dylib_command) + swap32(offset, mh.magic) as isize,
                );

                let cmp = strcmp(name, dylib_path);

                free(transmute(dylib_command));

                if cmp == 0 {
                    dprintln!(
                        "Binary already contains a load command for that dylib. Continue anyway?"
                    );
                    if !cont_anyway {
                        return false;
                    }
                }
            }

            LC_SEGMENT => {
                let cmd: &mut segment_command = transmute(read_load_command(f, cmdsize));
                if strcmp(cmd.segname.as_ptr(), "__LINKEDIT\0".as_ptr() as *const i8) == 0 {
                    linkedit_32_pos = ftello(f);
                    linkedit_32 = *cmd;
                }
                free(transmute(cmd));
            }

            LC_SEGMENT_64 => {
                let cmd: &mut segment_command_64 = transmute(read_load_command(f, cmdsize));
                if strcmp(cmd.segname.as_ptr(), "__LINKEDIT\0".as_ptr() as *const i8) == 0 {
                    linkedit_64_pos = ftello(f);
                    linkedit_64 = *cmd;
                }
                free(transmute(cmd));
            }

            LC_SYMTAB => {
                symtab_pos = ftello(f);
                symtab_size = cmdsize as usize;
            }

            _ => (),
        }

        fseeko(f, swap32(lc.cmdsize, mh.magic) as i64, SEEK_CUR);
    }

    return true;
}

unsafe fn insert_dylib(
    f: *mut FILE,
    header_offset: off_t,
    dylib_path: *const c_char,
    slice_size: &mut off_t,
    weak: bool,
    cont_anyway: bool,
) -> bool {
    fseeko(f, header_offset, SEEK_SET);

    let mut mh: mach_header = std::mem::zeroed();
    fread(transmute(&mh), size_of::<mach_header>(), 1, f);

    if mh.magic != MH_MAGIC_64
        && mh.magic != MH_CIGAM_64
        && mh.magic != MH_MAGIC
        && mh.magic != MH_CIGAM
    {
        dprintln!("Unknown magic: {:#x}\n", mh.magic);
        return false;
    }

    let commands_offset = header_offset + size_of::<mach_header_64>() as off_t;

    let cont = check_load_commands(
        f,
        &mut mh,
        header_offset,
        commands_offset,
        dylib_path,
        slice_size,
        cont_anyway,
    );
    if !cont {
        return true;
    }

    // Even though a padding of 4 works for x86_64, codesign doesn't like it
    let path_padding: usize = 8;

    let dylib_path_len = strlen(dylib_path);
    let dylib_path_size = (dylib_path_len & !(path_padding - 1)) + path_padding;
    let cmdsize: u32 = (size_of::<dylib_command>() + dylib_path_size) as u32;

    let dylib_command = dylib_command {
        cmd: swap32(
            if weak {
                LC_LOAD_WEAK_DYLIB
            } else {
                LC_LOAD_DYLIB
            },
            mh.magic,
        ),
        cmd_size: swap32(cmdsize, mh.magic),
        dylib: dylib_ {
            offset: swap32(size_of::<dylib_command>() as u32, mh.magic),
            timestamp: 0,
            current_version: 0,
            compatibility_version: 0,
        },
    };

    let mut sizeofcmds = swap32(mh.sizeofcmds, mh.magic);

    fseeko(f, commands_offset + sizeofcmds as i64, SEEK_SET);
    let space: Vec<u8> = vec![0; cmdsize as usize];

    fread(transmute(space.as_ptr()), cmdsize as size_t, 1, f);

    let mut empty = true;
    for x in &space {
        if *x != 0 {
            empty = false;
            break;
        }
    }

    if !empty {
        dprintln!("It doesn't seem like there is enough empty space. Continue anyway?");
        if !cont_anyway {
            return false;
        }
    }

    fseeko(f, -(cmdsize as off_t), SEEK_CUR);

    let dylib_path_padded = calloc(dylib_path_size, 1);
    memcpy(dylib_path_padded, transmute(dylib_path), dylib_path_len);

    fwrite(transmute(&dylib_command), size_of_val(&dylib_command), 1, f);
    fwrite(transmute(dylib_path_padded), dylib_path_size, 1, f);

    free(transmute(dylib_path_padded));

    mh.ncmds = swap32(swap32(mh.ncmds, mh.magic) + 1, mh.magic);
    sizeofcmds += cmdsize;
    mh.sizeofcmds = swap32(sizeofcmds, mh.magic);

    fseeko(f, header_offset, SEEK_SET);
    fwrite(transmute(&mh), size_of::<mach_header>(), 1, f);

    return true;
}

///
/// Insert `LC_LOAD_DYLIB` to `binary_path` to load `dylib_path`,
/// the action will break signature of `binary_path`. To use `LC_LOAD_WEAK_DYLIB`,
/// set `weak` to true.
///
pub unsafe fn insert(dylib_path: &str, binary_path: &str, weak: bool, cont_anyway: bool) -> bool {
    let dylib_path = [dylib_path, "\0"].join("");
    let binary_path = [binary_path, "\0"].join("");

    let lc_name = if weak {
        "LC_LOAD_WEAK_DYLIB"
    } else {
        "LC_LOAD_DYLIB"
    };

    let f = fopen(
        binary_path.as_ptr() as *const i8,
        "r+\0".as_ptr() as *const i8,
    );

    if f.is_null() {
        dprintln!("Couldn't open file {}", binary_path);
        return false;
    }

    let mut success = true;

    fseeko(f, 0, SEEK_END);
    let mut file_size = ftello(f);
    rewind(f);

    let magic: u32 = 0;
    fread(transmute(&magic), size_of::<u32>(), 1, f);

    match magic {
        FAT_MAGIC | FAT_CIGAM => {
            fseeko(f, 0, SEEK_SET);

            let fh: fat_header = zeroed();
            fread(transmute(&fh), size_of::<fat_header>(), 1, f);

            let nfat_arch: u32 = swap32(fh.nfat_arch, magic);
            dprintln!("Binary is a fat binary with {} archs.", nfat_arch);

            let mut archs: Vec<fat_arch> = vec![zeroed(); nfat_arch as usize];
            fread(
                transmute(archs.as_ptr()),
                size_of::<fat_arch>(),
                archs.len(),
                f,
            );

            let mut fails = 0;

            let mut offset: off_t = 0;
            if nfat_arch > 0 {
                offset = swap32(archs[0].offset, magic) as off_t;
            }

            for i in 0..archs.len() {
                let orig_offset = swap32(archs[i].offset, magic) as off_t;
                let orig_slice_size = swap32(archs[i].size, magic) as off_t;
                offset = round_up(offset as u64, 1 << swap32(archs[i].align, magic)) as off_t;

                if orig_offset != offset {
                    fmemmove(f, offset, orig_offset, orig_slice_size as usize);
                    fbzero(
                        f,
                        std::cmp::min(offset as i64, orig_offset) as i64 + orig_slice_size,
                        absdiff(offset, orig_offset),
                    );

                    archs[i].offset = swap32(offset as u32, magic);
                }

                let mut slice_size = orig_slice_size;
                let r = insert_dylib(
                    f,
                    offset,
                    dylib_path.as_ptr() as *const i8,
                    &mut slice_size,
                    weak,
                    cont_anyway,
                );
                if !r {
                    dprintln!("Failed to add {} to arch #{}!", lc_name, i + 1);
                    fails += 1;
                }

                if (slice_size < orig_slice_size) && (i < nfat_arch as usize - 1) {
                    fbzero(
                        f,
                        offset + slice_size,
                        (orig_slice_size - slice_size) as usize,
                    );
                }

                file_size = offset as i64 + slice_size;
                offset += slice_size;
                archs[i].size = swap32(slice_size as u32, magic);
            }

            rewind(f);
            fwrite(transmute(&fh), size_of::<fat_header>(), 1, f);
            fwrite(
                transmute(archs.as_ptr()),
                size_of::<fat_arch>(),
                archs.len(),
                f,
            );

            // We need to flush before truncating
            fflush(f);
            ftruncate(fileno(f), file_size);

            if fails == 0 {
                dprintln!("Added {} to all archs in {}", lc_name, binary_path);
            } else if fails == nfat_arch {
                dprintln!("Failed to add {} to any archs.", lc_name);
                success = false;
            } else {
                dprintln!(
                    "Added {} to {}/{} archs in {}",
                    lc_name,
                    nfat_arch - fails,
                    nfat_arch,
                    binary_path
                );
            }
        }

        MH_MAGIC_64 | MH_CIGAM_64 | MH_MAGIC | MH_CIGAM => {
            if insert_dylib(
                f,
                0,
                dylib_path.as_ptr() as *const i8,
                &mut file_size,
                weak,
                cont_anyway,
            ) {
                ftruncate(fileno(f), file_size);
                dprintln!("Added {} to {}", lc_name, binary_path);
            } else {
                dprintln!("Failed to add {}!", lc_name);
                success = false;
            }
        }

        _ => {
            dprintln!("Unknown magic: {:#}", magic);
            success = false;
        }
    }

    fclose(f);

    if !success {
        unlink(binary_path.as_ptr() as *const i8);
        return false;
    }

    true
}
