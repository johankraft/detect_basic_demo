import os, sys, time, shutil

# DevAlert Store-ELF
# This tool can be called after builds to store the elf files.
# It reads the gcc build id and uses that to rename the file.
# The build id can also be read programatically (see STM32L4 demo project)
# and can thereby be provided as the Revision field on DFM alerts.
# This way, you can retrive the right elf file in Dispatcher.
#
# Parameters:
#   - Path to ELF file
#   - Destination directory
#
# Example: store-elf image.elf C:\local-elf-library
#          If the Build-ID stored in image.elf is "abc123"
#          the script creates a copy of image.elf at C:\local-elf-library\abc123.elf 



def get_dir_size(path='.'):
    total = 0
    with os.scandir(path) as it:
        for entry in it:
            if entry.is_file():
                total += entry.stat().st_size
            elif entry.is_dir():
                total += get_dir_size(entry.path)
    return total

def main():
    argc = len(sys.argv)
    build_id = "(not found)"

    if (argc == 3):
        elf_file = sys.argv[1] 
        dest_dir = sys.argv[2]
    else:
        print("Missing argument - Usage: python store-elf.py <elf-file> <dest-path>")
        exit(-1)

    cmd = "arm-none-eabi-readelf -n " + elf_file

    build_id_raw = os.popen(cmd).readlines()

    if (('GNU' in build_id_raw[3]) and ('NT_GNU_BUILD_ID' in build_id_raw[3]) and ('Build ID:' in build_id_raw[4])):
        bid_stripped = build_id_raw[4].strip()
        build_id = bid_stripped.split('Build ID:')[1].strip()
    else:
        print("Error: Could nof find Build ID in elf file. Add '-Wl,--build-id' to your linker flags.")
        exit(-1)

    dest = dest_dir + "/" + build_id + ".elf"

    shutil.copy2(elf_file, dest)

    print("store-elf.py - Copied elf file to \"" + dest + "\"") 

    dir_size_in_bytes = get_dir_size(dest_dir)
    if (dir_size_in_bytes > 250 * 1024 * 1024):
        print("Note: You have over 250 MB of stored elf files in " + dest_dir)

main()
