"""Validate real catalog offsets and execute the C reader against installed data."""
import importlib.util
import pathlib
import struct
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("title_index", ROOT / "scripts/generate-title-index.py")
index_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(index_module)
SMASH = 0x01006A800016E000


class TitleIndexTest(unittest.TestCase):
    def test_all_committed_offsets_match_actual_bytes(self):
        data = (ROOT / "romfs/titles/titles.txt").read_bytes()
        index = (ROOT / "romfs/titles/titles.idx").read_bytes()
        self.assertEqual(index, index_module.build_index(data))
        magic, version, count = struct.unpack_from("<8sII", index)
        self.assertEqual((magic, version), (b"SHAIDX1\0", 1))
        previous = 0
        smash_found = False
        for i in range(count):
            title_id, offset = struct.unpack_from("<QQ", index, 16 + 16 * i)
            self.assertGreater(title_id, previous)
            previous = title_id
            line = data[offset:].split(b"\n", 1)[0]
            key, name = line.split(b";", 1)
            self.assertEqual(int(key, 16), title_id)
            self.assertTrue(name.strip())
            if title_id == SMASH:
                smash_found = True
                self.assertIn(b"Super Smash Bros.", name)
        self.assertTrue(smash_found)

    def test_byte_offsets_handle_crlf_utf8_and_duplicates(self):
        data = "0000000000000002;Pokémon\r\n0000000000000001;First\r\n0000000000000002;Duplicate\r\n".encode()
        index = index_module.build_index(data)
        self.assertEqual(struct.unpack_from("<QQ", index, 16), (1, data.index(b"0000000000000001;")))
        self.assertEqual(struct.unpack_from("<QQ", index, 32), (2, 0))

    def test_c_reader_recovers_from_stale_installed_index(self):
        harness = r'''
#include "title_cache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 char name[512];
 assert(title_cache_lookup(0x01006A800016E000ULL,name,sizeof(name)));
 assert(strcmp(name,"Super Smash Bros.™ Ultimate")==0);
 assert(title_cache_lookup(0x01006A800016E000ULL,name,sizeof(name)));
 assert(!title_cache_lookup(0,name,sizeof(name)));
 assert(!title_cache_lookup(0x01006A800016E000ULL,name,0));
 puts(name);
}
'''
        data = (ROOT / "romfs/titles/titles.txt").read_bytes()
        for stale in [False, True]:
            with self.subTest(stale=stale), tempfile.TemporaryDirectory() as temp:
                temp = pathlib.Path(temp)
                installed = temp / "sdmc:/switch/switch-ha"
                installed.mkdir(parents=True)
                (installed / "titles.txt").write_bytes(data)
                # Emulate an index generated before CRLF-to-LF conversion.
                index_data = data.replace(b"\n", b"\r\n") if stale else data
                (installed / "titles.idx").write_bytes(index_module.build_index(index_data))
                c = temp / "lookup.c"
                binary = temp / "lookup"
                c.write_text(harness)
                subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-I" + str(ROOT / "tests/stubs"), "-I" + str(ROOT / "include"), str(ROOT / "source/title_cache.c"), str(c), "-o", str(binary)], check=True)
                subprocess.run([str(binary)], cwd=temp, check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()
