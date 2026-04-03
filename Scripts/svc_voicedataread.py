import os
import struct
import sys
from dataclasses import dataclass
from typing import List, Optional

@dataclass
class PESection:
    virtual_address: int
    pointer_to_raw_data: int
    size_of_raw_data: int

def read_file_bytes(filepath: str) -> bytes:
    if not os.path.isfile(filepath):
        print(f"Error: {filepath} not found in current directory.")
        print("Expected location: GarrysMod/bin/win64/engine.dll (relative to Garry's Mod installation).")
        sys.exit(1)
    with open(filepath, "rb") as f:
        return f.read()

def find_function_signature(data: bytes) -> int:
    full_signature = bytes([0x48, 0x83, 0xEC, 0x38, 0x44, 0x8B, 0x5A, 0x1C, 0x45, 0x33, 0xC9])
    offset = data.find(full_signature)
    
    if offset != -1:
        print(f"Found full signature at file offset {hex(offset)}")
        return offset

    short_signature = bytes([0x48, 0x83, 0xEC, 0x38, 0x44])
    offset = data.find(short_signature)
    
    if offset != -1:
        print(f"Found short signature at file offset {hex(offset)}")
        return offset

    print("Signature not found. Aborting.")
    sys.exit(1)

def parse_pe_sections(data: bytes) -> List[PESection]:
    if data[:2] != b'MZ':
        print("Not a valid PE file.")
        sys.exit(1)

    e_lfanew = struct.unpack("<I", data[0x3C:0x40])[0]
    if data[e_lfanew:e_lfanew+4] != b'PE\x00\x00':
        print("Invalid PE signature.")
        sys.exit(1)

    file_header_offset = e_lfanew + 4
    machine = struct.unpack("<H", data[file_header_offset:file_header_offset+2])[0]
    
    if machine != 0x8664:
        print("Warning: This is not an x64 DLL.")
        
    num_sections = struct.unpack("<H", data[file_header_offset+2:file_header_offset+4])[0]
    opt_header_size = struct.unpack("<H", data[file_header_offset+16:file_header_offset+18])[0]
    opt_header_offset = file_header_offset + 20
    
    magic = struct.unpack("<H", data[opt_header_offset:opt_header_offset+2])[0]
    if magic != 0x20b:
        print("Not a 64-bit PE file.")
        sys.exit(1)

    section_header_offset = opt_header_offset + opt_header_size
    sections = []

    for i in range(num_sections):
        off = section_header_offset + i * 40
        virtual_address = struct.unpack("<I", data[off+12:off+16])[0]
        size_of_raw_data = struct.unpack("<I", data[off+16:off+20])[0]
        pointer_to_raw_data = struct.unpack("<I", data[off+20:off+24])[0]
        sections.append(PESection(virtual_address, pointer_to_raw_data, size_of_raw_data))

    return sections

def calculate_rva(offset: int, sections: List[PESection]) -> Optional[int]:
    for section in sections:
        if section.pointer_to_raw_data <= offset < section.pointer_to_raw_data + section.size_of_raw_data:
            return section.virtual_address + (offset - section.pointer_to_raw_data)
    return None

def main():
    target_file = "engine.dll"
    data = read_file_bytes(target_file)
    
    offset = find_function_signature(data)
    sections = parse_pe_sections(data)
    rva = calculate_rva(offset, sections)

    if rva is None:
        print("Cannot compute RVA: offset not within any section.")
        sys.exit(1)

    print(f"\nSVC_VoiceData::Read RVA = {hex(rva)}")
    print(f"Use in code: void* targetFunc = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(hEngine) + {hex(rva)});")
    print(f"Relative offset: 0x{rva:x}")

if __name__ == "__main__":
    main()